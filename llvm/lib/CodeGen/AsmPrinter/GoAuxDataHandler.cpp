// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: (Apache-2.0 WITH LLVM-exception) AND BSD-3-Clause
//
// Copyright 2018 The Go Authors. All rights reserved.
// Copyright (c) 2025 ByteDance Ltd. and/or its affiliates.
// This file has been modified by ByteDance Ltd. and/or its affiliates on 2025-01-28.
// visitPointers is adapted from getPtrBitmapForTypeHelper in
// passes/Util.cpp of gollvm (https://go.googlesource.com/gollvm/).
// These portions and their modifications retain BSD 3-Clause terms in
// third-party/gollvm/LICENSE. Other TangoLLVM code uses
// Apache-2.0 WITH LLVM-exception; see LICENSE.TXT.

//===- lib/MC/MCGoAuxData.cpp - MCGoAuxData implementation ----------------===//

#include "GoAuxDataHandler.h"
#include "GoGCProgWriter.h"
#include "llvm/ADT/BreadthFirstIterator.h"
#include "llvm/ADT/DepthFirstIterator.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/StackMaps.h"
#include "llvm/CodeGen/TangoLLVM.h"
#include "llvm/CodeGen/TargetFrameLowering.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCObjectFileInfo.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/Support/EndianStream.h"
#include "llvm/Target/TargetLoweringObjectFile.h"

using namespace llvm;

static cl::opt<bool> TangoLLVMLTO("tangollvm-lto", cl::Hidden, cl::init(false));

static cl::opt<bool> EnableUnsafePoint("go-enable-unsafe-point",
                                      cl::Hidden,
                                      cl::init(true));
static cl::opt<bool> DebugUnsafePoint("go-debug-unsafe-point",
                                      cl::Hidden,
                                      cl::init(false));
static cl::opt<bool> StrictUnsafePoint("go-strict-unsafe-point",
                                      cl::Hidden,
                                      cl::init(false));

// Any negative value would be OK because no index should be negative.
const int32_t DeferReturnStackMapIndexPlaceholder = -1000;

// copied from MCObjectStreamer.cpp
static const MCExpr *buildSymbolDiff(MCContext &Context, const MCSymbol *A,
                                     const MCSymbol *B) {
  assert(A != nullptr && B != nullptr && "should not be empty symbol");
  MCSymbolRefExpr::VariantKind Variant = MCSymbolRefExpr::VK_None;
  const MCExpr *ARef = MCSymbolRefExpr::create(A, Variant, Context);
  const MCExpr *BRef = MCSymbolRefExpr::create(B, Variant, Context);
  const MCExpr *AddrDelta = MCBinaryExpr::create(MCBinaryExpr::Sub, ARef, BRef, Context);
  return AddrDelta;
}

void MCGoAuxData::beginModule(Module *M) {
  resetModule();
  // Per-package Go objects use the compiler's file indices. LTO instead
  // creates a global table that the Go linker maps back to compilation units.
  if (!TangoLLVMLTO) {
    if (NamedMDNode *Meta = M->getNamedMetadata("go.local.filetab")) {
      for (MDNode *D : Meta->operands()) {
        if (D->getNumOperands() % 3 != 0)
          report_fatal_error("invalid Go local file table");
        for (unsigned I = 0; I < D->getNumOperands(); I += 3) {
          StringRef Dir = cast<MDString>(D->getOperand(I))->getString();
          StringRef File = cast<MDString>(D->getOperand(I + 1))->getString();
          int32_t Index = cast<ConstantAsMetadata>(D->getOperand(I + 2))
                              ->getValue()->getUniqueInteger().getSExtValue();
          FileTable.emplace_back(Dir, File, Index);
        }
      }
    }
  }
  GoMinorVersion = TangoLLVM::goVersion(*M);
  if (GoMinorVersion >= 21) {
    FuncInfoHasStartLine = true;
  }

  // switch section after creating end symbol
  const TargetLoweringObjectFile &TLOF= Asm->getObjFileLowering();
  MCSection *ReadOnlySection = TLOF.getGoAuxDataSection();
  Asm->OutStreamer->pushSection();
  Asm->OutStreamer->switchSection(ReadOnlySection);

  // TODO: remove Go ROData
  // Previously, we create Go rodata for missing read-only symbols in LTO.
  // Later we have fixed the bug of missing symbols.
  // However, there may also be cases where a symbol is missing.
  // We have no idea whether the missing symbol can be fixed by the go.rodata here.
  // We keep this and remove go.rodata until we can fix all issues about missing symbols in LTO.
  Asm->OutStreamer->emitLabel(Asm->OutContext.getOrCreateSymbol("go.rodata.begin"));
  Asm->OutStreamer->emitInt64(0);

  for (GlobalVariable &GV : M->globals()) {
    auto *ROData = GV.getMetadata("go.rodata");
    if (!ROData)
      continue;

    if (ROData->getNumOperands() > 1)
      continue; // not pure data, additional data are relocations

    StringRef Data = cast<MDString>(ROData->getOperand(0))->getString();
    std::string Symbol = "go.rodata." + GV.getName().str();
    MCSymbol *AddrSymbol = Asm->OutContext.getOrCreateSymbol(Symbol);
    Asm->OutStreamer->emitLabel(AddrSymbol);
    uint64_t Size = (uint64_t)((uint32_t)Data.size());
    std::string s;
    raw_string_ostream OSE(s);
    OSE << uint8_t(Size&0xFF);
    Size = Size >> 8;
    OSE << uint8_t(Size&0xFF);
    Size = Size >> 8;
    OSE << uint8_t(Size&0xFF);
    Size = Size >> 8;
    OSE << uint8_t(Size&0xFF);
    Size = Size >> 8;
    OSE << Data;
    Asm->OutStreamer->emitBinaryData(s);
  }

  Asm->OutStreamer->emitLabel(Asm->OutContext.getOrCreateSymbol("go.rodata.end"));
  Asm->OutStreamer->emitInt64(0);

  Asm->OutStreamer->popSection();
}

void MCGoAuxData::endModule() {
  if (!FileTable.empty())
    return;
  const TargetLoweringObjectFile &TLOF= Asm->getObjFileLowering();
  MCSection *ReadOnlySection = TLOF.getGoAuxDataSection();
  Asm->OutStreamer->pushSection();
  Asm->OutStreamer->switchSection(ReadOnlySection);

  if (!FileTableMap.empty()) {
    std::vector<MCGoFileEntry> Entries;
    Entries.reserve(FileTableMap.size());
    for (auto I : FileTableMap) {
      auto *DF = I.first;
      StringRef Dir = DF->getDirectory();
      StringRef File = DF->getFilename();
      auto Index = I.second;
      Entries.emplace_back(Dir, File, Index);
    }
    // Sort by Index so that we can access file table entry by Index
    llvm::sort(Entries, [](const MCGoFileEntry &LHS, const MCGoFileEntry &RHS) { return LHS.Index < RHS.Index; });
    assert(Entries[0].Index == 0 && "invariant");
    assert((Entries[Entries.size() - 1].Index == Entries.size() - 1) && "invariant");
    std::string symbol = TangoLLVMLTO ? "tangollvmlto..filetable"
                                     : "tangollvm..filetable";
    MCSymbol *AddrSymbol = Asm->OutContext.getOrCreateSymbol(StringRef(symbol));
    Asm->OutStreamer->emitLabel(AddrSymbol);
    Asm->OutStreamer->emitGoFileTab(Entries);
  }
  Asm->OutStreamer->popSection();
}

// AddDiscriminatorsPass will add new DI scope to handle discriminator.
static const DISubprogram *findSubprogramFromScope(const DIScope *Scope) {
  while (Scope) {
    if (auto *SP = dyn_cast<DISubprogram>(Scope))
      return SP;

    Scope = Scope->getScope();
  }
  return nullptr;
}

// TODO: the inlined tree is not used for deoptimization and only used for symbol lookup.
// We can pick any PC that can represent the caller of the inlined call.
MCGoInlTreeNode* MCGoAuxData::addInlNode(DILocation *L, DIScope *Called) {
  auto D = FuncInfo.LocToInlNode.find(L);
  if (D != FuncInfo.LocToInlNode.end()) {
    return D->second;
  }

  MCGoInlTreeNode *Parent = nullptr;
  if (L->getInlinedAt() != nullptr) {
    Parent = addInlNode(L->getInlinedAt(), L->getScope());
  }

  MCGoInlTreeNode *Node = new MCGoInlTreeNode();
  Node->Loc = L;
  Node->Index = (int32_t) FuncInfo.LocToInlNode.size();
  FuncInfo.LocToInlNode[L] = Node;
  FuncInfo.InlTree.push_back(Node);

  Node->Line = (int32_t)L->getLine();
  Node->File = getFileIndex(L->getFile());


  if (Called != nullptr) {
    // TODO: Func symbol should be PLT??
    MCSymbolRefExpr::VariantKind Variant = MCSymbolRefExpr::VK_GOT;
    auto *Subprogram = findSubprogramFromScope(Called);
    if (Subprogram == nullptr) {
      report_fatal_error("cannot search subprogram of an inlined at location.");
    }
    Node->Func = MCSymbolRefExpr::create(Subprogram->getName(), Variant, Asm->OutContext);
  } else {
    report_fatal_error("scope is not a subprogram of an inlined at location.");
  }

  if (Parent != nullptr) {
    Node->ParentNode = Parent;
    Node->Parent = Parent->Index;
  } else {
    Node->Parent = -1;
  }

  return Node;
}

static void sortLocation(std::vector<DILocation*> &locations) {
  llvm::sort(locations, [](DILocation* LHS, DILocation* RHS) {
      return LHS->getLine() < RHS->getLine();
  });
}

void MCGoAuxData::computeFuncInfo(const MachineFunction *MF) {
  auto *FIM = MF->getFunction().getMetadata("go.funcinfo");
  if (!FIM) {
    dbgs() << "Error: missing function info for " << MF->getName() << "\n";
    report_fatal_error("missing function info");
  }

  FuncInfo.Args = uint32_t(cast<ConstantAsMetadata>(FIM->getOperand(0))->getValue()->getUniqueInteger().getZExtValue());
  FuncInfo.FuncID = uint8_t(cast<ConstantAsMetadata>(FIM->getOperand(1))->getValue()->getUniqueInteger().getZExtValue());
  FuncInfo.FuncFlag = uint8_t(cast<ConstantAsMetadata>(FIM->getOperand(2))->getValue()->getUniqueInteger().getZExtValue());

  FuncInfo.StartLine = 0;
  FuncInfo.WriteStartLine = false;
  if (FuncInfoHasStartLine) {
    FuncInfo.WriteStartLine = true;
    FuncInfo.StartLine = uint32_t(cast<ConstantAsMetadata>(FIM->getOperand(3))->getValue()->getUniqueInteger().getZExtValue());
  }

  const MachineFrameInfo &MFI = MF->getFrameInfo();
  FuncInfo.Locals = uint32_t(MFI.getStackSize());

  // for each DILocation, if it has an InlinedAt, then we need to create an inline tree node for it.
  for (const auto &MBB : *MF) {
    for (const auto &MI : MBB.instrs()) {
      if (MI.isMetaInstruction()) {
        continue;
      }
      const DebugLoc &DL = MI.getDebugLoc();
      if (DL) {
        if (DL.getInlinedAt() != nullptr) {
          addInlNode(DL.getInlinedAt(), dyn_cast<DIScope>(DL.getScope()))->addIncluded(DL.get());
        } else {
          FuncInfo.addIncluded(DL.get());
        }

        if (FuncInfo.LocToInstr[DL] == nullptr) {
          FuncInfo.LocToInstr[DL] = &MI;
        }
        // may override previously set PC;
      }
    }
  }
  // now we have built necessary mapping between Instr and DILocation.
  sortLocation(FuncInfo.Included);
  for (auto it : FuncInfo.LocToInlNode) {
    MCGoInlTreeNode *InlNode = it.second;
    sortLocation(InlNode->Included);
  }
}

using SPFlow = std::pair<int32_t, int32_t>;

void MCGoAuxData::computeFunctionSP(const MachineFunction *MF) {
  DenseMap<const MachineBasicBlock*, SPFlow> PreBBSP;
  DenseMap<const MachineBasicBlock*, SPFlow> PostBBSP;
  const MachineBasicBlock *Entry = &MF->front();
  assert(Entry->isEntryBlock() && "front is not entry??");
  int32_t step = 0;
  PreBBSP[Entry] = SPFlow(0, step);

  const MachineFrameInfo &MFI = MF->getFrameInfo();
  const MachineBasicBlock *DeferMBB = nullptr;

  bool RequireFP = true;
  assert(MF->getFunction().hasFnAttribute("go-no-frame-checked") && "See checkGoFrame in PEI");
  auto Attr = MF->getFunction().getFnAttribute("go-no-frame-checked").getValueAsString();
  if (Attr == "true") {
    RequireFP = false;
  }

  uint64_t StackSize = MFI.getStackSize();

  // DeferReturn
  for (const auto &MBB : *MF) {
    if (MBB.isEntryBlock())
      continue;
    if (MBB.pred_empty()) {
      // TODO: currently the only possible case is OPEN-CODED deferreturn
      assert(!MBB.empty() && "invariant");
      assert(MF->getFunction().hasMetadata("go.opendefer") && "must be opencoded defer");
      auto It = MBB.getFirstNonDebugInstr();
      while (It != MBB.end()) {
        if (!It->isMetaInstruction())
          break;
        ++It;
      }
      if (It == MBB.end())
        report_fatal_error("bad MBB");
      const MachineInstr &MI = *It;
      if (!TangoLLVMCodeGen::isDeferReturnCall(MI)) {
        report_fatal_error("must be defer return");
      }

      assert(RequireFP && "invariant");
      PreBBSP[&MBB] = SPFlow(StackSize, 0);
      if (DeferMBB != nullptr) {
        report_fatal_error("should have only one deferreturn");
      }
      DeferMBB = &MBB;
    }
  }

  ReversePostOrderTraversal<const MachineFunction*> RPOT(MF);
  if (MF->size() != (llvm::size(RPOT) + PreBBSP.size() - 1)) {
    report_fatal_error("dangling machine basic block");
  }

  const TargetInstrInfo &TII = *Asm->MF->getSubtarget().getInstrInfo();
  int iteration = 0;
  while (true) {
    iteration++;
    if (iteration > 100) {
      dbgs() << "Error: Oops, cannot do analysis anymore after " << iteration << " iteration\n";
      report_fatal_error("too many iterations");
      break;
    }
    bool cont = false;

    auto ProcessMBB = [&](const MachineBasicBlock *MBB) {
      SPFlow PredFlow(-1, -1);
      step ++; // make progress
      for (auto *Pred : MBB->predecessors()) {
        // find pred SP at the end of the BB
        auto it = PostBBSP.find(Pred);
        if (it != PostBBSP.end()) {
          if (PredFlow.first == -1) {
            PredFlow = it->second;
          } else if (PredFlow.first == it->second.first) {
            continue;
          } else {
            if (PredFlow.second < it->second.second) {
              PredFlow = it->second;
            }
            // incomplete data: needs to update some pred
            cont = true;
          }
        } else {
          // no data: needs to update pred
          cont = true;
        }
      }
      if (!MBB->isEntryBlock() && MBB != DeferMBB && PredFlow.first == -1) {
        dbgs() << "Error: " << *MBB << " has no pred data";
        report_fatal_error("missing predecessor SP");
        return;
      }
      {
        // find SP at the beginning of the BB
        SPFlow Flow(-1, -1);
        auto it = PreBBSP.find(MBB);
        if (it != PreBBSP.end()) {
          Flow = it->second;
        }
        if (Flow.first != -1) {
          if (Flow.first == PredFlow.first) {
            // the BB has pred and both itself and pred have been visited
            // and there are no changes.
            return;
          }

          if (Flow.second < PredFlow.second) {
            Flow = PredFlow;
          } else if (!MBB->isEntryBlock() && MBB != DeferMBB) {
            dbgs() << "Error: Oops:" << Flow.second << ", pred=" << PredFlow.second << "\n";
            report_fatal_error("missing fresh predecessor SP!");
            return;
          } // else { }
        } else {
          // PredFlow must be a valid flow
          Flow = PredFlow;
        }

        Flow.second = step;
        PreBBSP[MBB] = Flow;
        int32_t lastSP = Flow.first;

        for (auto &MI : MBB->instrs()) {
          PreSPMap[&MI] = lastSP;
          lastSP += int32_t(TII.getSPAdjust(MI));
          PostSPMap[&MI] = lastSP;
        }
        PostBBSP[MBB] = SPFlow(lastSP, step);
      }
    };

    // 1. The algorithm assumes that all predecessors have the same SP.
    // 2. SP is non-negative in a function.
    for (auto *MBB: RPOT) {
      ProcessMBB(MBB);
    }
    // DeferMBB is a dangling MBB that is not reachable from the entry.
    if (DeferMBB != nullptr)
      ProcessMBB(DeferMBB);

    if (!cont) {
      break;
    }
  }

  int32_t LastSP = 0;
  for (const auto &MBB : *MF) {
    if (PreBBSP.find(&MBB) == PreBBSP.end()) {
      report_fatal_error("cannot find MBB in PreSP\n");
    }
    int32_t PreSP = PreBBSP[&MBB].first;
    if (LastSP != PreSP) {
      assert(!MBB.isEntryBlock() && "entry should be zero sp");
      auto PrevMBB = &*std::prev(MBB.getIterator());
      while (true) {
        assert(!PrevMBB->isEntryBlock() && "entry should not be empty");
        if (!PrevMBB->empty() && llvm::any_of(PrevMBB->instrs(), [&](const MachineInstr &MI) {
                return !MI.isMetaInstruction();
              })) {
          break;
        }
        PrevMBB = &*std::prev(PrevMBB->getIterator());
      }
      assert(!PrevMBB->empty() && "TODO");
      bool IsPred = false;
      for (auto *Succ : PrevMBB->successors()) {
        if (Succ == &MBB) {
          IsPred = true;
          break;
        }
      }
      if (!IsPred) {
        if (LastSP == 0) {
          const MachineInstr *MI = nullptr;
          for (auto I = PrevMBB->rbegin(), E = PrevMBB->rend(); I != E; ++I) {
            if (I->isMetaInstruction())
              continue;
            MI = &*I;
            break;
          }
          if (MI == nullptr)
            report_fatal_error("block has only meta instructions");

          assert(PreSP != 0 && "invariant: LastSP != PreSP ");
          assert(!MI->isCall() && "invariant: only return or morestack-jmp can change non-zero to zero");
          PostSPMap[MI] = PreSP;
        } else if (PreSP == 0) {
          // must not be Entry
          assert(LastSP != 0 && "invariant: LastSP != PreSP ");
          // MBB is not entry block. It must be morestack
          assert(LastSP == int32_t(MFI.getStackSize()) && "stack size is fixed after setup");
          bool HasMoreStack = false;
          for (auto &MI : MBB) {
            if (TangoLLVMCodeGen::isMoreStackCall(MI)) {
              HasMoreStack = true;
              break;
            }
          }
          assert(HasMoreStack && "must be morestack");
          if (!HasMoreStack)
            report_fatal_error("bad interior MBB has zero preSP");
          // morestack BB has a noop that can change SP to 0 before call morestack
        } else {
          report_fatal_error("bad pcsp in control flow graph");
        }
      } else {
        // MF->dump();
        // dbgs() << "MBB: " << MBB;
        // dbgs() << "LastSP: " << LastSP << ", PreSP: " << PreSP << "\n";
        report_fatal_error("bad pcsp in control flow graph");
      }
    }

    if (PostBBSP.find(&MBB) == PostBBSP.end()) {
      report_fatal_error("cannot find MBB in PostSP");
    }
    if (PreBBSP[&MBB].first < 0) {
      dbgs() << "Error: PreSP is negative for " << MBB << "\n";
      report_fatal_error("negative PreSP");
    }
    if (PostBBSP[&MBB].first < 0) {
      dbgs() << "Error: PostSP is negative for " << MBB << "\n";
      report_fatal_error("negative PostSP");
    }
    LastSP = PostBBSP[&MBB].first;
    // TODO: check upbounds
  }
}

static StringRef GetMOName(const MachineOperand *MO) {
  if (MO->isGlobal())
    return MO->getGlobal()->getName();
  if (MO->isSymbol())
    return StringRef(MO->getSymbolName());
  return "";
}

static std::pair<bool, bool> IsBarrierCmp(const MachineInstr* MI) {
  // cmp runtime.writeBarrier.enabled 0
  // or
  // %reg = move runtime.writeBarrier.enabled
  // test %reg
  const MachineOperand *MO = nullptr;
  if (MI->isCompare() && MI->getNumOperands() > 3) {
    MO = &MI->getOperand(3);
  } else if (MI->isMoveReg() && MI->getNumOperands() > 4) {
    MO = &MI->getOperand(4);
  }
  if (MO) {
    if (GetMOName(MO) == "runtime.writeBarrier") {
      return {true, MO->getOffset() == 0};
    }
  }
  return {false, false};
}

static bool ReverseMarkBarrierInBlock(const MachineBasicBlock* MBB,
                                      DenseSet<const MachineInstr*> &UnsafePoints,
                                      std::function<void(const MachineInstr*)> Fn) {
  bool FoundBarrier = false;
  for (auto It = MBB->rbegin(), E = MBB->rend(); It != E; It++) {
    if (It->isMetaInstruction())
      continue;
    UnsafePoints.insert(&*It);
    auto [WB, Enable] = IsBarrierCmp(&*It);
    if (WB) {
      if (Enable) {
        return true;
      }
      Fn(&*It);
    }
  }
  return false;
}

void MCGoAuxData::computeFunctionUnsafePoint(const MachineFunction *MF) {
  if (!EnableUnsafePoint)
    return;

  assert(UnsafePoints.empty() && AllUnsafe);
  auto MarkAllUnsafe = [&]() {
    UnsafePoints.clear();
    AllUnsafe = true;
  };

  if (MF->getFunction().hasFnAttribute("go.allunsafe") ||
      (MF->getFunction().hasFnAttribute("go-no-split") && !MF->getFunction().hasFnAttribute("go-leaf"))) {
    MarkAllUnsafe();
    return;
  }

  const TargetInstrInfo &TII = *Asm->MF->getSubtarget().getInstrInfo();
  SmallDenseSet<const MachineBasicBlock*> MoreStackBlocks;

  if (!MF->getFunction().hasFnAttribute("go-leaf")) {
    // more-stack blocks
    auto *CheckBB = &MF->front();
    if (auto *Succ = CheckBB->getSingleSuccessor())
      CheckBB = Succ;
    assert(CheckBB->succ_size() == 2);

    auto Type = MF->getFunction()
                    .getFnAttribute("go.stack.size.type")
                    .getValueAsString();

    MachineBasicBlock *AllocBB = nullptr, *CheckBB2 = nullptr;
    bool BigStack = Type == "big";
    auto *Succ0 = *CheckBB->successors().begin();
    auto *Succ1 = *(CheckBB->successors().begin() + 1);
    if (Succ0->getSingleSuccessor() == CheckBB) {
      AllocBB = Succ0;
      if (BigStack)
        CheckBB2 = Succ1;
    } else {
      if (Succ1->getSingleSuccessor() != CheckBB) {
        MarkAllUnsafe();
        return;
      }
      AllocBB = Succ1;
      if (BigStack)
        CheckBB2 = Succ0;
    }

    MoreStackBlocks.insert(CheckBB);
    MoreStackBlocks.insert(AllocBB);

    bool Found = false;
    for (auto &MI : *CheckBB) {
      if (MI.isMetaInstruction())
        continue;
      if (Found) {
        UnsafePoints.insert(&MI);
      }
      if ((BigStack && MI.isMoveReg()) || (!BigStack && MI.isCompare()))
        Found = true;
    }
    if (CheckBB2) {
      MoreStackBlocks.insert(CheckBB2);
      for (auto &MI : *CheckBB2) {
        if (MI.isMetaInstruction())
          continue;
        UnsafePoints.insert(&MI);
      }
    }
    Found = false;
    for (auto &MI : *AllocBB) {
      if (MI.isMetaInstruction())
        continue;
      if (MI.isTerminator())
        break;
      if (MI.getOpcode() == TII.getNop().getOpcode())
        Found = true;
      if (Found) {
        UnsafePoints.insert(&MI);
      }
    }
  }

  {
    for (const auto &MBB : *MF) {
      const MachineInstr *FirstSIMD = nullptr, *LastSIMD = nullptr;
      for (const MachineInstr &MI : MBB) {
        if (MI.isMetaInstruction())
          continue;
        if (TII.isSIMD(MI)) {
          if (FirstSIMD == nullptr)
            FirstSIMD = &MI;
          LastSIMD = &MI;
        }
      }
      // TODO: async preempt at SIMD instructions may cause runtime data errors,
      // the reason is unclear.
      if (FirstSIMD != nullptr) {
        auto IterBegin =
            MachineBasicBlock::iterator(const_cast<MachineInstr *>(FirstSIMD));
        auto IterEnd = std::next(
            MachineBasicBlock::iterator(const_cast<MachineInstr *>(LastSIMD)));
        for (auto It = IterBegin; It != IterEnd; ++It) {
          UnsafePoints.insert(&*It);
        }
      }
    }
  }

  {
    SmallDenseSet<const MachineBasicBlock*> MarkedCheckBBs;
    SmallDenseSet<const MachineBasicBlock*> MarkedSuccBBs;
    SmallDenseSet<const MachineInstr*> BarrierCmps;
    for (const auto &MBB : *MF) {
      SmallDenseSet<const MachineInstr*> MIs;
      bool FoundGCWrite = false;
      for (const MachineInstr &MI : MBB) {
        if (MI.isMetaInstruction())
          continue;
        auto [WB, Enable] = IsBarrierCmp(&MI);
        if (WB) {
          if (Enable)
            BarrierCmps.insert(&MI);
          else {
            if (DebugUnsafePoint)
              dbgs() << "Func: " << MF->getName() << ", MI: " << MI
                     << ", writeBarrier.xx at BB: " << MBB.getName() << "\n";

            if (StrictUnsafePoint) {
              MarkAllUnsafe();
              return;
            }
          }
        }
        if (FoundGCWrite) {
          UnsafePoints.insert(&MI);
          continue;
        }

        MIs.insert(&MI);
        if (!MI.isCall())
          continue;
        auto &CalleeMO = MI.getOperand(0);
        if (!GetMOName(&CalleeMO).starts_with("runtime.gcWriteBarrier"))
          continue;

        UnsafePoints.insert(MIs.begin(), MIs.end());
        FoundGCWrite = true;
      }

      if (!FoundGCWrite)
        continue;

      SmallDenseSet<const MachineBasicBlock*> Candidates;
      for (auto &Pred : MBB.predecessors()) {
        if (!MarkedCheckBBs.insert(Pred).second)
          continue;
        auto *CurrMBB = const_cast<MachineBasicBlock*>(Pred);
        do {
          bool FnUnsafe = false;
          bool FoundBarrier =
              ReverseMarkBarrierInBlock(CurrMBB, UnsafePoints, [&](auto *I) {
                if (DebugUnsafePoint)
                  dbgs() << "Func: " << MF->getName()
                         << ", MI: " << *I
                         << ", writeBarrier.xx at BB: " << MBB.getName() << "\n";

                if (StrictUnsafePoint)
                  FnUnsafe = true;
              });

          if (FnUnsafe) {
            MarkAllUnsafe();
            return;
          }

          Candidates.insert(Pred);
          CurrMBB = CurrMBB->getSinglePredecessor();
        } while (!FoundGCWrite && CurrMBB);

        if (!FoundGCWrite) {
          MF->print(dbgs());
          report_fatal_error("can't find runtime.writeBarrier for MBB: " +
                             Pred->getName());
        }
      }
      if (Candidates.empty())
        continue;
      for (auto *CurrMBB : Candidates) {
        for (auto *Succ : CurrMBB->successors()) {
          if (!MarkedSuccBBs.insert(Succ).second)
            continue;
          for (auto &I : *Succ) {
            if (I.isMetaInstruction())
              continue;
            UnsafePoints.insert(&I);
          }
        }
      }
    }

    for (auto *MI : BarrierCmps)
      if (!UnsafePoints.contains(MI)) {
        auto *MBB = MI->getParent();
        if (MBB->succ_size() != 2) {
          if (DebugUnsafePoint) {
            dbgs() << "barrier cmp is not marked: " << MF->getName() << "\n";
            dbgs() << "    MI: " << *MI
                   << ", at BB: " << MI->getParent()->getName() << "\n";
            MF->print(dbgs());
          }
          MarkAllUnsafe();
          return;
        } else {
          ReverseMarkBarrierInBlock(MBB, UnsafePoints, [&](auto *){});
          for (auto *Succ : MBB->successors())
            for (auto &I : *Succ) {
              if (I.isMetaInstruction())
                continue;
              UnsafePoints.insert(&I);
            }
        }
      }
  }

  {
    if (MF->getFunction().hasMetadata("go.has.unsafepoint")) {
      // find unsafe point marked by go front end, which is opconvert now.
      DenseSet<const MachineInstr *> Flooded;
      std::function<void(const MachineBasicBlock *, const MachineInstr *)>
          Flood;
      Flood = [&](const MachineBasicBlock *MBB, const MachineInstr *MI) {
        if (MoreStackBlocks.contains(MBB))
          return;
        auto It = MI == nullptr ? MBB->rbegin()
                                : MachineBasicBlock::reverse_iterator(
                                      const_cast<MachineInstr *>(MI));
        for (auto End = MBB->rend(); It != End; It++) {
          const MachineInstr &I = *It;
          if (I.isMetaInstruction())
            continue;
          if (Flooded.contains(&I))
            // already marked
            return;
          if (I.isCall())
            return;
          Flooded.insert(&I);
        }
        for (const MachineBasicBlock *Pred : MBB->predecessors())
          Flood(Pred, nullptr);
      };

      uint64_t Got = 0;
      for (const MachineBasicBlock &MBB : *MF)
        for (const MachineInstr &MI : MBB) {
          if (MI.getFlag(MachineInstr::GoUnsafePointRange)) {
            Got++;
            Flood(&MBB, &MI);
          } else if (MI.getFlag(MachineInstr::GoUnsafePoint)) {
            Got++;
            UnsafePoints.insert(&MI);
          }
        }
      for (const MachineInstr *MI : Flooded)
        UnsafePoints.insert(MI);

      auto *MD = MF->getFunction().getMetadata("go.has.unsafepoint");
      if (MD && MD->getNumOperands() > 0) {
        uint64_t Want = mdconst::extract<ConstantInt>(MD->getOperand(0))->getLimitedValue();
        if (Got != Want) {
          if (StrictUnsafePoint) {
            MarkAllUnsafe();
            return;
          }
        }
        if (DebugUnsafePoint) {
          // GoUnsafePoint removed by passes.
          dbgs() << "Go Unsafe Point removed, Func: " << MF->getName() << ", Want: " << Want << ", Got: " << Got << "\n";
        }
      }
    }
  }

  AllUnsafe = false;
  if (UnsafePoints.empty())
    assert(MF->getFunction().hasFnAttribute("go-leaf"));
}

bool MCGoAuxData::hasPointerTy(Type* type) {
  return TangoLLVM::hasPointerTy(type);
}

unsigned MCGoAuxData::ptrDataSize(const DataLayout &DL, Type* type) {
  if (!hasPointerTy(type)) {
    return 0;
  }
  switch (type->getTypeID()) {
    case Type::FloatTyID:
    case Type::DoubleTyID:
    case Type::IntegerTyID:
      return 0;
    case Type::PointerTyID:
      return DL.getPointerSize();
    case Type::ArrayTyID: {
      int64_t numElem = type->getArrayNumElements();
      if (numElem == 0) {
        return 0;
      }
      unsigned elemPtrDataSize = ptrDataSize(DL, type->getArrayElementType());
      if (elemPtrDataSize == 0) {
        return 0;
      }
      return unsigned((numElem - 1) * (DL.getTypeAllocSize(type->getArrayElementType()).getFixedValue())) + elemPtrDataSize;
    }
    case Type::StructTyID: {
      StructType* ST = dyn_cast<StructType>(type);
      if (ST->getNumElements() == 0) {
        return 0;
      }
      const StructLayout* SL = DL.getStructLayout(ST);
      for (int i = (int)ST->getNumElements() - 1; i >= 0; i--) {
        Type* ET = ST->getElementType(i);
        unsigned elemPtrDataSize = ptrDataSize(DL, ET);
        if (elemPtrDataSize > 0) {
          return SL->getElementOffset(i) + elemPtrDataSize;
        }
      }
      return 0;
    }
    default:
      report_fatal_error("unsupported type");
      break;
  }
  return -1;
}

std::string MCGoAuxData::bitVecToString(const BitVector& Bits, unsigned PtrDataSize, unsigned PtrSize) {
  std::vector<uint8_t> PtrMask(((PtrDataSize / PtrSize) + 7) / 8, 0);
  for (int64_t i = 0; i < PtrDataSize / PtrSize; ++i) {
    if (Bits.test(i)) {
      PtrMask[i / 8] |= 1 << (i % 8);
    }
  }
  std::string HexString;
  for (auto Byte : PtrMask) {
    char Hex[3];
    std::sprintf(Hex, "%02x", Byte);
    HexString += Hex;
  }
  return "runtime.gcbits." + HexString;
}

void MCGoAuxData::typeSetBits(const DataLayout&DL, Type* T, unsigned Off, BitVector &Bits) {
  if (!hasPointerTy(T)) {
    return;
  }
  switch (T->getTypeID()) {
    case Type::FloatTyID:
    case Type::DoubleTyID:
    case Type::IntegerTyID:
      // do nothing
      return;
    case Type::PointerTyID: {
      Bits.set(Off/DL.getPointerSize());
      return;
    }
    case Type::ArrayTyID: {
      Type* ET = T->getArrayElementType();
      unsigned ETSize = DL.getTypeAllocSize(ET).getFixedValue();
      if (ETSize == 0) {
        return;
      }
      int64_t numElem = T->getArrayNumElements();
      if (numElem == 0) {
        return;
      }

      for (unsigned i = 0; i < numElem; i++) {
        typeSetBits(DL, ET, Off, Bits);
        Off += ETSize;
      }
      return;
    }
    case Type::StructTyID: {
      StructType* ST = dyn_cast<StructType>(T);
      // interface struct { Itab *tab;	void *data; }
      // The first word of an interface is a pointer, but golang don't as such.
      // The pointer points to an itab or a _type, and it won't be GCd
      if (ST->hasName() && ST->getName().starts_with("interface$")) {
        Bits.set(Off / DL.getPointerSize() + 1);
        return;
      }
      const StructLayout* SL = DL.getStructLayout(ST);
      for (unsigned i = 0; i < ST->getNumElements(); i++) {
        Type* ET = ST->getElementType(i);
        typeSetBits(DL, ET, Off + SL->getElementOffset(i), Bits);
      }
      return;
    }
    default:
      report_fatal_error("unsupported type");
  }
}

void MCGoAuxData::beginFunctionImpl(const MachineFunction *MF) {
  // reset for this function
  resetFunction();

  EntryPC = Asm->OutStreamer->getContext().createTempSymbol();
  Asm->OutStreamer->emitLabel(EntryPC);

  //
  computeFunctionSP(MF);
  computeFuncInfo(MF);
  computeFunctionUnsafePoint(MF);

  {
    // Every Go function has a PC TText
    LastSPAdjust = -1;
  }
  {
    LastFileIndex = -1;
    LastLineNumber = -1;
  }
  {
    LastStackMapIndex = -1;
  }
  {
    PreIsUnsafe = false;
  }
}

void MCGoAuxData::emitPCData(std::vector<PCData> &list, std::string &symbol) {
  MCSymbol *AddrSymbol = Asm->OutContext.getOrCreateSymbol(StringRef(symbol));
  Asm->OutStreamer->emitLabel(AddrSymbol);

  MCSymbol* PrevPC = EntryPC;
  int32_t PrevValue = -1;
  for (auto &D : list) {
    int64_t ValueDelta = D.Value - PrevValue;
    const MCExpr *AddrDelta = buildSymbolDiff(Asm->OutContext, D.PC, PrevPC);
    Asm->OutStreamer->emitGoPCDataEntry(ValueDelta, AddrDelta);
    PrevPC = D.PC;
    PrevValue = D.Value;
  }
  Asm->OutStreamer->emitGoPCDataEntry(0, nullptr);
}

// convert Type* to string
// todo temporary solution
static void collectExpandedTypes(Type *Ty, std::set<Type *> &visitedTypes, raw_ostream &OS) {
  if (visitedTypes.count(Ty)) {
    if (auto *ST = dyn_cast<StructType>(Ty)) {
      OS << "{";
      for (unsigned i = 0; i < ST->getNumElements(); ++i) {
        if (i > 0) OS << ", ";
        collectExpandedTypes(ST->getElementType(i), visitedTypes, OS);
      }
      OS << "}";
    } else if (auto *AT = dyn_cast<ArrayType>(Ty)) {
      OS << "[";
      OS << AT->getNumElements();
      OS << " x ";
      collectExpandedTypes(AT->getElementType(), visitedTypes, OS);
      OS << "]";
    } else if (Ty->isPointerTy()) {
      OS << "i8*";
    } else if (Ty->isIntegerTy()) {
      OS << "i" << Ty->getIntegerBitWidth();
    } else if (Ty->isVoidTy()) {
      OS << "void";
    } else if (Ty->isFloatTy()) {
      OS << "float";
    } else if (Ty->isDoubleTy()) {
      OS << "double";
    }
  } else {
    visitedTypes.insert(Ty);
    if (auto *ST = dyn_cast<StructType>(Ty)) {
      OS << "{";
      for (unsigned i = 0; i < ST->getNumElements(); ++i) {
        if (i > 0) OS << ", ";
        collectExpandedTypes(ST->getElementType(i), visitedTypes, OS);
      }
      OS << "}";
    } else if (auto *AT = dyn_cast<ArrayType>(Ty)) {
      OS << "[";
      OS << AT->getNumElements();
      OS << " x ";
      collectExpandedTypes(AT->getElementType(), visitedTypes, OS);
      OS << "]";
    } else if (Ty->isPointerTy()) {
      if (cast<PointerType>(Ty)->isIntegerTy()) {
        OS << "i" << cast<PointerType>(Ty)->getIntegerBitWidth() << "*";
      } else {
        OS << "i64*";
      }
    } else if (Ty->isIntegerTy()) {
      OS << "i" << Ty->getIntegerBitWidth();
    } else if (Ty->isVoidTy()) {
      OS << "void";
    } else if (Ty->isFloatTy()) {
      OS << "float";
    } else if (Ty->isDoubleTy()) {
      OS << "double";
    }
  }
}

static std::string getTypeString(Type* T) {
  std::set<Type *> visitedTypes;
  std::string typeStr;
  raw_string_ostream OS(typeStr);
  collectExpandedTypes(T, visitedTypes, OS);
  return OS.str();
}

void MCGoAuxData::endFunctionImpl(const MachineFunction *MF) {
  MCSymbol* EndFunc = Asm->OutStreamer->getContext().createTempSymbol();
  Asm->OutStreamer->emitLabel(EndFunc);

  // switch section after creating end symbol
  const TargetLoweringObjectFile &TLOF= Asm->getObjFileLowering();
  const DataLayout &DL = Asm->getDataLayout();
  MCSection *ReadOnlySection = TLOF.getGoAuxDataSection();
  Asm->OutStreamer->pushSection();
  Asm->OutStreamer->switchSection(ReadOnlySection);

  {
    // emit function attributes
    // see src/cmd/link/internal/obj/link.go
    const int AttrNoSplit = 4;
    uint64_t Attr = 0;
    if (!MF->getFunction().hasFnAttribute("go-split")) {
      Attr |= AttrNoSplit;
    }

    std::string symbol = MF->getName().str();
    symbol += "..attributes";
    MCSymbol *AddrSymbol = Asm->OutContext.getOrCreateSymbol(StringRef(symbol));
    Asm->OutStreamer->emitLabel(AddrSymbol);
    Asm->OutStreamer->emitInt64(Attr);
  }

  {
    if (auto N = MF->getFunction().getMetadata("go.marker.reloc")) {
      unsigned NumOp = N->getNumOperands();
      assert(NumOp % 4 == 0 && "must be array");

      std::string symbol = MF->getName().str();
      symbol += "..markerreloc";
      MCSymbol *AddrSymbol = Asm->OutContext.getOrCreateSymbol(StringRef(symbol));
      Asm->OutStreamer->emitLabel(AddrSymbol);

      std::string s;
      raw_string_ostream OSE(s);

      size_t Offset = 0;
      support::endian::write(OSE, uint32_t(NumOp / 4), endianness::little);
      Offset += 4;

      for (unsigned i = 0; i < NumOp; i += 4) {
        // (type, addend, abi, name)
        int64_t RType = dyn_cast<ConstantAsMetadata>(N->getOperand(i + 0))->getValue()->getUniqueInteger().getSExtValue();
        int64_t Addend = dyn_cast<ConstantAsMetadata>(N->getOperand(i + 1))->getValue()->getUniqueInteger().getSExtValue();
        int64_t ABI = dyn_cast<ConstantAsMetadata>(N->getOperand(i + 2))->getValue()->getUniqueInteger().getSExtValue();
        StringRef Name = dyn_cast<MDString>(N->getOperand(i + 3))->getString();

        // index
        support::endian::write(OSE, int32_t(RType), endianness::little);
        Offset += 4;

        support::endian::write(OSE, uint32_t(ABI), endianness::little);
        Offset += 4;

        support::endian::write(OSE, int64_t(Addend), endianness::little);
        Offset += 8;

        uint64_t NameLen = Name.size();
        support::endian::write(OSE, NameLen, endianness::little);
        Offset += 8;

        OSE.write(Name.data(), NameLen);
        Offset += NameLen;
      }

      (void)Offset;
      assert(Offset == s.size() && "invariant");

      Asm->OutStreamer->emitBinaryData(s);
    }
  }

  {
    // emit stack objects
    std::vector<MCGoStackObjectRecord> StackObjects;
    if (auto n = MF->getFunction().getMetadata("go.stackobject.gcdata")) {
      unsigned numOp = n->getNumOperands();
      // dbgs() << "Function StackObject: NumOp: " << numOp << "\n";
      assert(numOp % 4 == 0 && "must be quadruple array");

      for (unsigned i = 0; i < numOp; i += 4) {
        // (frameoffset, size, ptrdata, gcdatasym)
        int64_t offset = dyn_cast<ConstantAsMetadata>(n->getOperand(i + 0))->getValue()->getUniqueInteger().getSExtValue();
        int64_t size = dyn_cast<ConstantAsMetadata>(n->getOperand(i + 1))->getValue()->getUniqueInteger().getSExtValue();
        int64_t ptrdata = dyn_cast<ConstantAsMetadata>(n->getOperand(i + 2))->getValue()->getUniqueInteger().getSExtValue();
        StringRef gcdata = dyn_cast<MDString>(n->getOperand(i + 3))->getString();

        assert(offset == int64_t(int32_t(offset)) && "must not overflow");
        assert(size == int64_t(int32_t(size)) && "must not overflow");
        assert(ptrdata == int64_t(int32_t(ptrdata)) && "must not overflow");

        assert(offset >= 0 && "param and paramout have positive offset");

        MCSymbolRefExpr::VariantKind Variant = MCSymbolRefExpr::VK_GOT;
        auto gcdatasym = MCSymbolRefExpr::create(gcdata, Variant, Asm->OutContext);

        // dbgs() << "ParamsOrParamsOut: " << i / 4 << " " << offset << " " << size << " " << ptrdata << " " << gcdata << "\n";
        StackObjects.emplace_back(int32_t(offset), int32_t(size), int32_t(ptrdata), gcdatasym);
      }
    }
    {
      const MachineFrameInfo &MFI = MF->getFrameInfo();
      for (int i = MFI.getObjectIndexBegin(), e = MFI.getObjectIndexEnd(); i != e; ++i) {
        if (MFI.isDeadObjectIndex(i)) {
          continue;
        }
        const AllocaInst* Alloc = MFI.getObjectAllocation(i);
        if (Alloc != nullptr && Alloc->hasMetadata("go.stackobject.gcdata")) {
          auto n = Alloc->getMetadata("go.stackobject.gcdata");
          assert(n->getNumOperands() == 3 && "must be triple array");
          // (, size, ptrdata, gcdatasym)
          // TODO: why plus 16
          // 1. we only support x86-64 darwin and linux
          // 2. stack grows down
          // 3. we force store FP when doing the calculation.
          // 4. FP size + return pc size = 16
          int64_t offset = MFI.getObjectOffset(i) + 16;
          int64_t size = dyn_cast<ConstantAsMetadata>(n->getOperand(0))->getValue()->getUniqueInteger().getSExtValue();
          int64_t ptrdata = dyn_cast<ConstantAsMetadata>(n->getOperand(1))->getValue()->getUniqueInteger().getSExtValue();
          StringRef gcdata = dyn_cast<MDString>(n->getOperand(2))->getString();
          std::string bitstr;
          if (gcdata == "go.empty") {
            Type* AllocType = Alloc->getAllocatedType();
            if (!hasPointerTy(AllocType)) {
              continue;
            }
            size = MFI.getObjectSize(i);
            ptrdata = ptrDataSize(MF->getDataLayout(), AllocType);
            assert(ptrdata >= 0 && "sanity check");
            BitVector Bits;
            Bits.resize(ptrdata/DL.getPointerSize() + 1);
            typeSetBits(MF->getDataLayout(), AllocType, 0, Bits);
            if (ptrdata/DL.getPointerSize() <= 2048*8) {
              bitstr = bitVecToString(Bits, ptrdata, DL.getPointerSize());
              gcdata = StringRef(bitstr);
            } else {
              bitstr = (GoMinorVersion > 19 ? "type:.gcprog." : "type..gcprog.") + getTypeString(AllocType);
              gcdata = StringRef(bitstr);
              MCGoGCProgWriter ProgWriter;
              ProgWriter.emit(AllocType, MFI.getObjectSize(i), DL, 0);
              ptrdata = - (ProgWriter.Index * DL.getPointerSize());
            }
          } else {
            assert(size == MFI.getObjectSize(i) && "sanity check");
            assert(offset == int64_t(int32_t(offset)) && "must not overflow");
            assert(size == int64_t(int32_t(size)) && "must not overflow");
            assert(ptrdata == int64_t(int32_t(ptrdata)) && "must not overflow");
            assert(offset < 0 && "locals has negative offset");
          }

          MCSymbolRefExpr::VariantKind Variant = MCSymbolRefExpr::VK_GOT;
          auto gcdatasym = MCSymbolRefExpr::create(gcdata, Variant, Asm->OutContext);

          // dbgs() << "Alloca: " << i << " " << offset << " " << size << " " << ptrdata << " " << gcdata
          //  << " for " << MF->getFunction().getName() << "\n";
          StackObjects.emplace_back(int32_t(offset), int32_t(size), int32_t(ptrdata), gcdatasym);
        }
      }
    }

    // dbgs() << "SizeOfStackObjects: " << StackObjects.size() << "\n";
    if (!StackObjects.empty()) {
      std::string symbol = MF->getName().str();
      symbol += "..stackobjects";
      MCSymbol *AddrSymbol = Asm->OutContext.getOrCreateSymbol(StringRef(symbol));
      Asm->OutStreamer->emitLabel(AddrSymbol);
      llvm::sort(StackObjects, [&](MCGoStackObjectRecord &LHS, MCGoStackObjectRecord &RHS) { return LHS.FrameOffset < RHS.FrameOffset; });
      Asm->OutStreamer->emitGoStackObjects(ArrayRef<MCGoStackObjectRecord>(StackObjects));
    }
  }

  if (LastSPAdjust != -1) {
    appendPCData(PCSP, LastSPAdjust, EndFunc);

    std::string symbol = MF->getName().str();
    symbol += "..pcsp";
    emitPCData(PCSP, symbol);
  }
  if (FileStarted) {
    appendPCData(PCFile, LastFileIndex, EndFunc);

    std::string symbol = MF->getName().str();
    symbol += "..pcfile";
    emitPCData(PCFile, symbol);
  }
  if (LineStarted) {
    appendPCData(PCLine, LastLineNumber, EndFunc);

    std::string symbol = MF->getName().str();
    symbol += "..pcline";
    emitPCData(PCLine, symbol);
  }
  {
    if (LastStackMapIndex != -1) {
      appendPCData(PCStackMapIndex, LastStackMapIndex, EndFunc);

      if (RequireDeferReturnMap) {
        // Lets do a full scan of the PCStackMapIndex
        PCData *DeferReturnEntry = nullptr;
        int32_t MaxIndex = -2; // -1 is the first entry, other entries should be greater than -1

        for (auto &E : PCStackMapIndex) {
          if (E.Value > MaxIndex)
            MaxIndex = E.Value;
          if (E.Value == DeferReturnStackMapIndexPlaceholder) {
            if (DeferReturnEntry == nullptr)
              DeferReturnEntry = &E;
            else
              report_fatal_error("two deferreturn entries in PCStackMapIndex");
          }
        }
        assert(DeferReturnEntry != nullptr && "invariant");
        assert(MaxIndex > -2 && "invariant");
        // Various corner cases:
        // 1). PCStackMapIndex has a single entry for the runtime.deferreturn, MaxIndex should be -1
        // 2). PCStackMapIndex has two entries, for morestack and deferreturn, MaxIndex should be 0
        // 3). Other cases, MaxIndex should be the largest index in the stack map table.
        int DeferReturnIndex = MaxIndex + 1;
        // We cannot use 0 as 0 is reserved for the entry map, even sometimes we may not need the entry stack map
        // if the function is nosplit.
        if (DeferReturnIndex == 0)
          DeferReturnIndex = 1;
        DeferReturnEntry->Value = DeferReturnIndex;
      }

      std::string symbol = MF->getName().str();
      symbol += "..pcstackmapindex";
      emitPCData(PCStackMapIndex, symbol);
      //..argspointermaps and ..localspointermaps are emitted in GoGCPrinter.cpp
      // Here we only need to check whether we need an empty EntryMap
      if (!HasStatepoint) {
        std::vector<std::vector<uint8_t>> ArgsTable(1);
        std::vector<std::vector<uint8_t>> LocalsTable(1);
        uint32_t MaxArgsSize = 0, MaxLocalsSize = 0;
        const Function &Fn = MF->getFunction();
        emitFunctionStackMaps(&Fn, Asm, ArgsTable, LocalsTable, MaxArgsSize, MaxLocalsSize);
      }
    }
  }
  {
    // TODO: all functions are unsafe!
    // PCDATA_UnsafePoint values.
    // PCDATA_UnsafePointSafe   = -1 // Safe for async preemption
    // PCDATA_UnsafePointUnsafe = -2 // Unsafe for async preemption/
    if (AllUnsafe) {
      assert(UnsafePoints.empty());
      appendPCData(PCUnsafePoint, -2, EndFunc);
    } else {
      appendPCData(PCUnsafePoint, PreIsUnsafe? -2 : -1, EndFunc);
    }
    std::string symbol = MF->getName().str();
    symbol += "..pcunsafepoint";
    emitPCData(PCUnsafePoint, symbol);
  }
  if (InlineTreeStarted) {
    appendPCData(PCInline, LastInlineTree, EndFunc);

    std::string symbol = MF->getName().str();
    symbol += "..pcinline";
    emitPCData(PCInline, symbol);

    // set parent pc
    for (auto it : FuncInfo.LocToInlNode) {
      DILocation* Loc = it.first;
      MCGoInlTreeNode *Node = it.second;
      DILocation* RL = FuncInfo.findRealLocation(Loc);
      if (RL == nullptr) {
        Node->ParentPC = nullptr; // will emit 0
      } else if (FuncInfo.LocToPC.count(RL)) {
        MCSymbol* PCSymbol = FuncInfo.LocToPC[RL];
        Node->ParentPC = buildSymbolDiff(Asm->OutContext, PCSymbol, EntryPC);
      } else {
        // TODO: an empty function has been inlined?
        Node->ParentPC = nullptr; // will emit 0
      }
    }
  }

  if (auto *MD = MF->getFunction().getMetadata("go.opendefer")) {
    assert(RequireDeferReturnMap && "opendefer requires deferreturn map");
    DenseMap<const AllocaInst*, int64_t> AllocaToIdx;
    for (auto &I : instructions(MF->getFunction())) {
      const IntrinsicInst *II = dyn_cast<const IntrinsicInst>(&I);
      if (!II || II->getIntrinsicID() != Intrinsic::go_metadata)
        continue;

      GoMetadata ID = GoMetadata(cast<ConstantInt>(II->getArgOperand(1))->getZExtValue());
      if (ID != GoMetadata::OpenDefer)
        continue;

      const AllocaInst *AI = dyn_cast<const AllocaInst>(II->getArgOperand(0)->stripPointerCasts());
      if (AI == nullptr)
        continue;

      int64_t Idx = cast<const ConstantAsMetadata>(cast<const MetadataAsValue>(II->getOperand(2))->getMetadata())->getValue()->getUniqueInteger().getSExtValue();

      auto It = AllocaToIdx.find(AI);
      if (It == AllocaToIdx.end())
        AllocaToIdx[AI] = Idx;
      else if (It->second != Idx)
        report_fatal_error("mismatched idx");
    }
    const unsigned MaxBits = 8;
    // from 0 to 7
    uint64_t TotalInfo = dyn_cast<ConstantAsMetadata>(MD->getOperand(0))->getValue()->getUniqueInteger().getZExtValue();
    int Closures[MaxBits];
    unsigned Bits = 0;
    const MachineFrameInfo &MFI = MF->getFrameInfo();
    int DeferBitsTemp = MFI.getObjectIndexEnd();
    for (int I = MFI.getObjectIndexBegin(), E = MFI.getObjectIndexEnd(); I != E; ++I) {
      if (MFI.isDeadObjectIndex(I))
        continue;

      const AllocaInst* Alloc = MFI.getObjectAllocation(I);
      if (Alloc == nullptr)
        continue;

      auto It = AllocaToIdx.find(Alloc);
      if (It == AllocaToIdx.end())
        continue;

      int64_t Idx = It->second;
      if (Idx >= MaxBits || Idx < -1)
        report_fatal_error("opendefer index is too big");

      if (Idx == -1) {
        if (DeferBitsTemp == MFI.getObjectIndexEnd()) {
          DeferBitsTemp = I;
        } else {
          report_fatal_error("multiple deferbitstemp");
        }
        continue;
      }

      if ((Bits & (1 << Idx)) == 0) {
        Bits |= 1 << Idx;
      } else {
        report_fatal_error("duplicate opendefer info");
      }
      Closures[Idx] = I;
    }

    if (DeferBitsTemp == MFI.getObjectIndexEnd()) {
      report_fatal_error("no deferbitstemp");
    }

    SmallVector<int64_t, 8> ObjectOffsets(TotalInfo);
    for (unsigned I = 0; I < TotalInfo; ++I) {
      if ((Bits & (1 << I)) == 0) {
        report_fatal_error("missing opendefer info");
      }
      int Idx = Closures[I];
      int64_t Offset = MFI.getObjectOffset(Idx) + 16;
      ObjectOffsets[I] = Offset;
    }

    std::string symbol = MF->getName().str();
    symbol += "..opendefer";
    MCSymbol *AddrSymbol = Asm->OutContext.getOrCreateSymbol(StringRef(symbol));
    Asm->OutStreamer->emitLabel(AddrSymbol);
    int64_t DeferBitsTempOffset = MFI.getObjectOffset(DeferBitsTemp) + 16;
    Asm->OutStreamer->emitGoOpenDefer(DeferBitsTempOffset, ObjectOffsets, GoMinorVersion);
  }
  {
    std::string symbol = MF->getName().str();
    symbol += "..funcinfo";
    MCSymbol *AddrSymbol = Asm->OutContext.getOrCreateSymbol(StringRef(symbol));
    Asm->OutStreamer->emitLabel(AddrSymbol);
    llvm::sort(FuncInfo.File, [](uint32_t LHS, uint32_t RHS) { return LHS < RHS; });
    Asm->OutStreamer->emitGoFuncInfo(FuncInfo);
  }
  {
    appendPCData(PCIndex, LastPCIndex++, EndFunc);
    std::string symbol = MF->getName().str();
    symbol += "..pcindex";
    emitPCData(PCIndex, symbol);
  }
  {
    std::string symbol = MF->getName().str();
    symbol += "..dwarflines";
    MCSymbol *AddrSymbol = Asm->OutContext.getOrCreateSymbol(StringRef(symbol));
    Asm->OutStreamer->emitLabel(AddrSymbol);
    Asm->OutStreamer->emitGoDwarfInfo(DwarfTable);
  }
  {
    std::string symbol = MF->getName().str();
    symbol += "..endaux";
    MCSymbol *AddrSymbol = Asm->OutContext.getOrCreateSymbol(StringRef(symbol));
    Asm->OutStreamer->emitLabel(AddrSymbol);
  }
  Asm->OutStreamer->popSection();
}

// Self-growing DIFile to Index mapping
int32_t MCGoAuxData::getFileIndex(DIFile* DF) {
  auto It = FileTableMap.find(DF);
  int32_t Index;
  if (It != FileTableMap.end()) {
    Index = It->second;
  } else if (!FileTable.empty()) {
    Index = -1;
    for (const auto &F : FileTable) {
      if (F.Dir == DF->getDirectory() && F.File == DF->getFilename()) {
        Index = F.Index;
        break;
      }
    }
    if (Index < 0)
      report_fatal_error("debug file missing from Go local file table");
    FileTableMap[DF] = Index;
  } else {
    Index = FileTableMap.size();
    FileTableMap[DF] = Index;
  }
  FuncInfo.recordUsedFile(Index);
  return Index;
}

int32_t MCGoAuxData::getInlineTreeNodeIndex(DILocation* DL) {
  if (DL == nullptr || DL->getInlinedAt() == nullptr) {
    return -1;
  }
  auto D = FuncInfo.LocToInlNode.find(DL->getInlinedAt());
  if (D == FuncInfo.LocToInlNode.end()) {
    report_fatal_error("cannot find an InlTreeNode for an InlinedAt location");
  }
  return D->second->Index;
}

void MCGoAuxData::appendPCData(std::vector<PCData> &list, int32_t value, MCSymbol* PC) {
  // we record raw PC here and apply delta during emit.
  list.push_back(PCData(value, PC));
}

void MCGoAuxData::beginInstruction(const MachineInstr *MI) {
  if (MI->isMetaInstruction()) {
    return;
  }

  {
    // TODO: we simply track DebugLoc for every machine instr;
    const DebugLoc &DL = MI->getDebugLoc();
    int32_t FileIndex = -1;
    int32_t Line = 0;
    int32_t InlTreeIndex = -1;
    if (DL) {
      DILocation* L = DL.get();
      FileIndex = getFileIndex(L->getFile());
      Line = int32_t(DL.getLine());
      InlTreeIndex = getInlineTreeNodeIndex(L);

      // record InlineTreeNode PC
      if (FuncInfo.LocToInstr.find(L) != FuncInfo.LocToInstr.end()) {
        if (FuncInfo.LocToPC[L] == nullptr) {
          FuncInfo.LocToPC[L] = getOrCreatePrePCSymbol(MI);
        }
      }
    }

    if (!FileStarted || FileIndex != LastFileIndex) {
      if (FileStarted) appendPCData(PCFile, LastFileIndex, getOrCreatePrePCSymbol(MI));
      LastFileIndex = FileIndex;
      FileStarted = true;
    }
    if (!LineStarted || Line != LastLineNumber) {
      if (LineStarted) appendPCData(PCLine, LastLineNumber, getOrCreatePrePCSymbol(MI));
      LastLineNumber = Line;
      LineStarted = true;
    }

    if (!InlineTreeStarted || InlTreeIndex != LastInlineTree) {
      if (InlineTreeStarted) appendPCData(PCInline, LastInlineTree, getOrCreatePrePCSymbol(MI));
      LastInlineTree = InlTreeIndex;
      InlineTreeStarted = true;
    }
  }

  {
    if (MI->getOpcode() == TargetOpcode::STATEPOINT) {
      StatepointOpers SO(MI);
      int IdPos = SO.getIDPos();
      const MachineOperand &MO = MI->getOperand(IdPos);
      assert(MO.isImm());
      uint64_t ID = MO.getImm();
      if (ID >= MaxStatepointID) MaxStatepointID = ID + 1;
      int32_t CurrentStackMapIndex = ID - FunctionBeginID + 1; // Index 0 is reserved for entry stack map.
      if (LastStackMapIndex != CurrentStackMapIndex) {
        appendPCData(PCStackMapIndex, LastStackMapIndex, getOrCreatePrePCSymbol(MI));
      }
      LastStackMapIndex = CurrentStackMapIndex;
      HasStatepoint = true;
    } else if (MI->isCall()) {
      // A call is very rare. Most function only have zero or one call. Other call are converted into Statepoint.
      // A call inst may be a call to morestack, which is a Go target and may trigger GC and copystack.
      // A call may also be a call to an intrinsic, which is a libc function and cannot trigger any runtime.
      assert(MI->getNumOperands() > 0 && "must have callee");
      bool isMoreStackCall = TangoLLVMCodeGen::isMoreStackCall(*MI);

      if (isMoreStackCall) {
        int32_t CurrentStackMapIndex = 0; // 0 is reserved for the entry map
        if (LastStackMapIndex != CurrentStackMapIndex) {
          appendPCData(PCStackMapIndex, LastStackMapIndex, getOrCreatePrePCSymbol(MI));
        }
        LastStackMapIndex = CurrentStackMapIndex;
      } else if (TangoLLVMCodeGen::isDeferReturnCall(*MI)) {
        // only for opendefer
        // Any other call to runtime.defereturn should be a STATEPOINT.
        assert(MI->getParent()->getParent()->getFunction().hasMetadata("go.opendefer") && "invariant");
        RequireDeferReturnMap = true;
        int32_t CurrentStackMapIndex = DeferReturnStackMapIndexPlaceholder;
        if (LastStackMapIndex != CurrentStackMapIndex) {
          appendPCData(PCStackMapIndex, LastStackMapIndex, getOrCreatePrePCSymbol(MI));
        }
        LastStackMapIndex = CurrentStackMapIndex;
      }
    }
  }
  {
    // update lastsp
    if (LastSPAdjust == -1) {
      LastSPAdjust = 0;
    }
    if (PostSPMap.find(MI) == PostSPMap.end()) {
      dbgs() << "Error: Oops, no post SP data for instruction " << *MI;
      report_fatal_error("no post SP data for instruction");
    }
    int32_t PostSP = PostSPMap[MI];
    int32_t PreSP = PreSPMap[MI];
    // dbgs() << "PostSP: " << PostSP << "/" << PreSP << " for " << *MI << "\n";
    if (PreSP != LastSPAdjust && PreSP != 0)
      report_fatal_error("bad pcsp");
    // need to skip CFI instructions since they are of zero size and do not
    // advance the PC. The PostSP update on CFI instruction will be applied
    // on the next available instruction instead
    if (LastSPAdjust != PostSP && !MI->isMetaInstruction()) {
      appendPCData(PCSP, LastSPAdjust, getOrCreatePostPCSymbol(MI));
      LastSPAdjust = PostSP;
    }
  }

  {
    auto DwarfLoc = Asm->OutContext.getCurrentDwarfLoc();
    auto Line = DwarfLoc.getLine();
    if (Line != 0 && LastFileIndex != -1) {
      DwarfTable.push_back(
      MCGoDwarfEntry(
            LastFileIndex,
            LastPCIndex,
            DwarfLoc.getLine(),
            DwarfLoc.getColumn(),
            DwarfLoc.getFlags(),
            DwarfLoc.getDiscriminator()
          ));
    }

    appendPCData(PCIndex, LastPCIndex, getOrCreatePrePCSymbol(MI));
    LastPCIndex++;
  }

  {
    if (!UnsafePoints.empty())
      if (PreIsUnsafe != UnsafePoints.contains(MI)) {
        appendPCData(PCUnsafePoint, PreIsUnsafe? -2 : -1, getOrCreatePrePCSymbol(MI));
        PreIsUnsafe = UnsafePoints.contains(MI);
      }
  }

  if (PrePCMap.contains(MI)) {
    Asm->OutStreamer->emitLabel(PrePCMap[MI]);
  }

}

void MCGoAuxData::endInstruction() {
  // we emit postpc after visiting instruction
  if (PostPC != nullptr) {
    Asm->OutStreamer->emitLabel(PostPC);
    PostPC = nullptr;
  }
}

MCSymbol* MCGoAuxData::getOrCreatePrePCSymbol(const MachineInstr* MI) {
  if (!PrePCMap.contains(MI))
      PrePCMap[MI] = Asm->OutStreamer->getContext().createTempSymbol();
    return PrePCMap[MI];
}

MCSymbol* MCGoAuxData::getOrCreatePostPCSymbol(const MachineInstr* MI) {
  if (PostPC == nullptr) {
    PostPC = Asm->OutStreamer->getContext().createTempSymbol();
  }
  return PostPC;
}

static void convertToLocalBytesVector(uint32_t MaxMapSize, uint32_t MaxByteSize, std::vector<uint8_t> &Map) {
  std::vector<uint8_t> New(MaxByteSize, 0);
  for (uint32_t i = 0; i < MaxMapSize; i++) {
    uint32_t ByteIndex = i / 8;
    if (ByteIndex >= Map.size()) {
      break;
    }
    uint32_t BitIndex = i % 8;
    if ((Map[ByteIndex] & (1 << BitIndex)) != 0) {
      uint32_t NI = MaxMapSize - i - 1;
      ByteIndex = NI / 8;
      BitIndex = NI % 8;
      uint8_t B = New[ByteIndex];
      B = B | (1 << BitIndex);
      New[ByteIndex] = B;
    }
  }
  Map = New;
}

template <typename F>
static void
visitPointers(Type *T, const DataLayout &DL, int64_t BaseOffset, const F& Func) {
  if (!MCGoAuxData::hasPointerTy(T))
    return;

  Type *Int32Ty = Type::getInt32Ty(T->getContext());
  switch (T->getTypeID()) {
  case Type::PointerTyID:
    Func(BaseOffset);
    break;;
  case Type::ArrayTyID: {
    Type *ET = T->getArrayElementType();
    for (unsigned i = 0, n = T->getArrayNumElements(); i < n; ++i) {
      Value *ivals[2] = { ConstantInt::get(Int32Ty, 0),
                          ConstantInt::get(Int32Ty, i) };
      ArrayRef<Value*> Idx = ArrayRef(ivals, 2);
      uint64_t Offset = DL.getIndexedOffsetInType(T, Idx);
      visitPointers(ET, DL, BaseOffset+Offset, Func);
    }
    break;
  }
  case Type::FixedVectorTyID: {
    auto *VT = llvm::cast<llvm::FixedVectorType>(T);
    Type *ET = T->getScalarType();
    for (unsigned i = 0, n = VT->getNumElements(); i < n; ++i) {
      Value *ivals[2] = { ConstantInt::get(Int32Ty, 0),
                          ConstantInt::get(Int32Ty, i) };
      ArrayRef<Value*> Idx = ArrayRef(ivals, 2);
      uint64_t Offset = DL.getIndexedOffsetInType(T, Idx);
      visitPointers(ET, DL, BaseOffset+Offset, Func);
    }
    break;
  }
  case Type::StructTyID: {
    for (unsigned i = 0, n = T->getStructNumElements(); i < n; ++i) {
      Type *ET = T->getStructElementType(i);
      if (!MCGoAuxData::hasPointerTy(ET))
        continue;
      Value *ivals[2] = { ConstantInt::get(Int32Ty, 0),
                          ConstantInt::get(Int32Ty, i) };
      ArrayRef<Value*> Idx = ArrayRef(ivals, 2);
      uint64_t Offset = DL.getIndexedOffsetInType(T, Idx);
      visitPointers(ET, DL, BaseOffset+Offset, Func);
    }
    break;
  }
  case Type::ScalableVectorTyID:
    assert(false && "not expecting ScalableVectorTyID");
    break;
  default:
    break;
  }
}

static void setBitAtOffset(int64_t Offset, std::vector<uint8_t> &Map, int64_t &MaxIndex) {
  assert(Offset >= 0 && "invariant");
  // 8 is PtrSize
  int64_t Index = Offset / 8;
  if (Index > MaxIndex)
    MaxIndex = Index;
  int64_t ByteIndex = Index / 8;
  int64_t BitIndex = Index % 8;
  if (ByteIndex >= (int64_t)Map.size()) {
    Map.resize(ByteIndex + 1, 0);
  }
  Map[ByteIndex] = Map[ByteIndex] | (1 << BitIndex);
}

static void emitDeferStackMap(const Function *Fn, AsmPrinter *Asm, std::vector<std::vector<uint8_t>> &ArgsTable,
    std::vector<std::vector<uint8_t>> &LocalsTable, uint32_t &MaxArgsSize, uint32_t &MaxLocalsSize) {
  const DataLayout &DL = Asm->getDataLayout();

  std::vector<uint8_t> &DeferArgsMap = ArgsTable[ArgsTable.size() - 1];
  std::vector<uint8_t> &DeferLocalsMap = LocalsTable[LocalsTable.size() - 1];

  int64_t MaxLocalIndex = -1;
  int64_t MaxArgIndex = -1;
  auto consumeLocalOffset = [&](int64_t Offset){
    setBitAtOffset(Offset, DeferLocalsMap, MaxLocalIndex);
  };
  auto consumeArgOffset = [&](int64_t Offset){
    setBitAtOffset(Offset, DeferArgsMap, MaxArgIndex);
  };

  // Fn should be immutable during GC printer.
  // We should not call harvestGoMetadata to collect metadata.
  // Collect all such Alloca into the AlwaysLive.
  SmallSetVector<const AllocaInst*, 8> AlwaysLive;
  for (auto &I : instructions(*Fn)) {
    const IntrinsicInst *II = dyn_cast<const IntrinsicInst>(&I);
    if (!II || II->getIntrinsicID() != Intrinsic::go_metadata)
      continue;

    GoMetadata ID = GoMetadata(cast<const ConstantInt>(II->getArgOperand(1))->getZExtValue());
    if (ID != GoMetadata::AlwaysLive)
      continue;

    if (const AllocaInst *AI = dyn_cast<const AllocaInst>(II->getArgOperand(0)->stripPointerCasts())) {
      int64_t Offset = cast<ConstantInt>(cast<ConstantAsMetadata>(cast<const MetadataAsValue>(II->getArgOperand(2))->getMetadata())->getValue())->getSExtValue();
      assert(Offset <= 0 && "stack grows down");
      Offset = -Offset - 16 - 8;
      Type* Ty = AI->getAllocatedType();
      visitPointers(Ty, DL, Offset, consumeLocalOffset);
    } else if (const IntrinsicInst *PII = dyn_cast<const IntrinsicInst>(II->getArgOperand(0)->stripPointerCasts())) {
      int64_t Offset = cast<ConstantInt>(PII->getArgOperand(1))->getSExtValue();
      Type *Ty = PII->getArgOperand(3)->getType();
      visitPointers(Ty, DL, Offset, consumeArgOffset);
    } else {
      report_fatal_error("unknown type");
    }
  }

  if (MaxLocalIndex + 1 > MaxLocalsSize)
    MaxLocalsSize = MaxLocalIndex + 1;

  if (MaxArgIndex + 1 > MaxArgsSize)
    MaxArgsSize = MaxArgIndex + 1;
}

static void emitEntryStackMap(const Function *Fn, const DataLayout &DL, uint32_t &MaxSize, std::vector<uint8_t> &Map) {
  if (!Fn->hasFnAttribute("go-split"))
    return;
  int64_t MaxIndex = -1;
  const unsigned PtrSize = DL.getPointerSize();
  if (PtrSize != 8)
    report_fatal_error("ptr size must be 8");

  // Use constant PtrSize for better performance
  auto consumeOffset = [&](int64_t Offset){
    setBitAtOffset(Offset, Map, MaxIndex);
  };

  if (auto n = Fn->getMetadata("go.stack.param")) {
    unsigned numOp = n->getNumOperands();
    if (numOp > 0) {
      assert((numOp % 2 == 0) && "invariant");
      for (unsigned i = 0; i < numOp; i += 2) {
        // (type, offset)
        Type *Ty = dyn_cast<ConstantAsMetadata>(n->getOperand(i + 0))->getValue()->getType();
        assert(Ty != nullptr);
        int64_t Offset = dyn_cast<ConstantAsMetadata>(n->getOperand(i + 1))->getValue()->getUniqueInteger().getSExtValue();

        visitPointers(Ty, DL, Offset, consumeOffset);
      }
    }
  }

  if (auto n = Fn->getMetadata("go.caller.args.area")) {
    unsigned numOp = n->getNumOperands();
    if (numOp > 2) {
      assert(((numOp - 2) % 4 == 0) && "must be 4 quadruple array");
      for (unsigned i = 2; i < numOp; i += 4) {
        // (regidx, offset, size, ptr)
        int64_t IsPtr = dyn_cast<ConstantAsMetadata>(n->getOperand(i + 3))->getValue() ->getUniqueInteger().getSExtValue();
        if (IsPtr == 0) {
          continue;
        }
        int64_t Offset = dyn_cast<ConstantAsMetadata>(n->getOperand(i + 1))->getValue()->getUniqueInteger().getSExtValue();
#if !defined(NDEBUG)
        int64_t Size = dyn_cast<ConstantAsMetadata>(n->getOperand(i + 2))->getValue()->getUniqueInteger().getSExtValue();
        assert(Size == 8 && "amd64 must be 8 bytes ptr");
        assert(Offset >= 0 && Offset % 8 == 0 && "must be 8 byte aligned");
#endif
        Offset -= 8; // Offset is relative to FP. We need to subtract slot for PC. See llvm.go
        consumeOffset(Offset);
      }
    }
  }

  if (MaxIndex + 1 > MaxSize)
    MaxSize = MaxIndex + 1;
}

static void
emitStackMaps(uint32_t MaxMapSize, std::vector<std::vector<uint8_t>> MapTable, MCStreamer &OS, bool isLocal) {
  const int PtrSize = 8; // TODO: get from target info
  OS.emitIntValue(MapTable.size(), 4); // number of maps
  OS.emitIntValue(MaxMapSize, 4); // number of bits in each map
  uint32_t MaxBytes = (MaxMapSize + (PtrSize - 1)) / PtrSize;
#if 0
  dbgs() << "Before TableSize: " << MapTable.size() << "\n";
  dbgs() << "Before MaxMapSize: " << MaxMapSize << "\n";
  dbgs() << "Before MaxBytes: " << MaxBytes << "\n";
  int I = 0;
  for (auto &Map : MapTable) {
    uint32_t Index = 0;
    dbgs() << I << " ";
    I++;
    for (uint8_t Byte : Map) {
      Index++;
      dbgs() << int(Byte) << " ";
    }
    for (; Index < MaxBytes; Index++) {
      dbgs() << 0 << " ";
    }
    dbgs() << "\n";
  }
#endif
  for (auto &Map : MapTable) {
    uint32_t Index = 0;
    if (isLocal) {
      convertToLocalBytesVector(MaxMapSize, MaxBytes, Map);
    }
    for (uint8_t Byte : Map) {
      Index++;
      OS.emitIntValue(Byte, 1);
    }
    for (; Index < MaxBytes; Index++) {
      OS.emitIntValue(0, 1); // fill with zero bytes
    }
  }
#if 0
  dbgs() << "After MaxMapSize: " << MaxMapSize << "\n";
  dbgs() << "After MaxBytes: " << MaxBytes << "\n";
  for (auto &Map : MapTable) {
    uint32_t Index = 0;
    for (uint8_t Byte : Map) {
      Index++;
      dbgs() << int(Byte) << " ";
    }
    for (; Index < MaxBytes; Index++) {
      dbgs() << 0 << " ";
    }
    dbgs() << "\n";
  }
#endif
}

void MCGoAuxData::emitFunctionStackMaps(const Function *Fn, AsmPrinter *Asm, std::vector<std::vector<uint8_t>> &ArgsTable,
    std::vector<std::vector<uint8_t>> &LocalsTable, uint32_t MaxArgsSize, uint32_t MaxLocalsSize) {
  if (Fn->hasMetadata("go.opendefer")) {
    // We need to add one more entry to the ArgsTable and LocalsTable
    int Size = ArgsTable.size();
    assert(Size >= 1 && "must not be empty");
    ArgsTable.resize(Size + 1);
    LocalsTable.resize(Size + 1);
    emitDeferStackMap(Fn, Asm, ArgsTable, LocalsTable, MaxArgsSize, MaxLocalsSize);
  }
  {
    std::string symbol = Fn->getName().str();
    symbol += "..argspointermaps";
    MCSymbol *AddrSymbol = Asm->OutContext.getOrCreateSymbol(StringRef(symbol));
    Asm->OutStreamer->emitLabel(AddrSymbol);
    std::vector<uint8_t> &EntryMap = ArgsTable[0];
    emitEntryStackMap(Fn, Asm->getDataLayout(), MaxArgsSize, EntryMap);
    emitStackMaps(MaxArgsSize, ArgsTable, *Asm->OutStreamer, false);
  }
  {
    std::string symbol = Fn->getName().str();
    symbol += "..localspointermaps";
    MCSymbol *AddrSymbol = Asm->OutContext.getOrCreateSymbol(StringRef(symbol));
    Asm->OutStreamer->emitLabel(AddrSymbol);
    emitStackMaps(MaxLocalsSize, LocalsTable, *Asm->OutStreamer, true);
  }

}
