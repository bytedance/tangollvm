//===- X86GoFixupPreRegAlloc.cpp - optimize usage of LEA instructions -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 ByteDance Ltd. and/or its affiliates.
//===----------------------------------------------------------------------===//
//
// This files defines fixups for Go code.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/X86BaseInfo.h"
#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/StackMaps.h"
#include "llvm/CodeGen/TargetOpcodes.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include <cassert>
#include <cstdint>
#include <iterator>
#include <set>
#include <deque>

#define DEBUG_TYPE "x86-go-fixup-preregalloc"

using namespace llvm;

static cl::opt<bool>
    CloneStackPointers("x86-gogc-clone-stack-pointers", cl::Hidden, cl::init(true),
                     cl::desc("Clone LEA stack address to avoid spill and restore"));

static cl::opt<bool>
    CheckHoistedPointers("x86-gogc-check-hoisted-pointers", cl::Hidden, cl::init(true),
                     cl::desc("Check and fixup hoisted pointers"));
static cl::opt<bool>
    CheckDerivedPointers("x86-gogc-check-derived-pointers", cl::Hidden, cl::init(true),
                         cl::desc("Check and fixup derived pointers"));
static cl::opt<bool>
    DebugCheckDerivedPointers("debug-x86-gogc-check-derived-pointers", cl::Hidden, cl::init(false),
                         cl::desc("Debug 'Check and fixup derived pointers'"));


namespace {

class X86GoFixupPreRegAllocPass : public MachineFunctionPass {
public:
  X86GoFixupPreRegAllocPass() : MachineFunctionPass(ID) {
    initializeX86GoFixupPreRegAllocPassPass(*PassRegistry::getPassRegistry());
  }

  StringRef getPassName() const override { return "X86 Go Fixup PreRegAlloc"; }

  bool runOnMachineFunction(MachineFunction &MF) override;

  static char ID;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    MachineFunctionPass::getAnalysisUsage(AU);
  }

private:
  MachineFunction *MF = nullptr;
  const X86RegisterInfo *TRI = nullptr;
  const X86InstrInfo * TII = nullptr;
  MachineRegisterInfo *MRI = nullptr;

  bool isGoStackPointerRegister(Register Reg) {
    StringRef RegName = TRI->getRegAsmName(Reg);
    return RegName == "RSP";
  }

  void AddOperandToStatePoint(MachineInstr& SO,
                              const SmallSetVector<unsigned, 8>& Reg,
                              bool isRegIndex);
  bool optimizeOpSP();
  bool cloneStackPointers();
  bool cloneRdiIfNeeded();
  bool checkHoistedPointers();
  bool checkDerivedPointers();

};

} // end anonymous namespace

char X86GoFixupPreRegAllocPass::ID = 0;

FunctionPass *llvm::createX86GoFixupPreRegAlloc() { return new X86GoFixupPreRegAllocPass(); }
INITIALIZE_PASS(X86GoFixupPreRegAllocPass, DEBUG_TYPE, "X86 Fixup prealloc pass", false,
                false)

struct RemoveRedundantCopy {
  MachineInstr * Copy;
  Register Src; // SP
  Register Dst; // to be replaced

  bool replace(MachineInstr &MI);
};

bool RemoveRedundantCopy::replace(MachineInstr &MI) {
  bool Changed = false;
  for (MachineOperand &MO : MI.operands()) {
    if (MO.isReg() && MO.getReg() == Dst) {
      if (MO.isDef()) {
        report_fatal_error("cannot define in function body");
      }
      MO.setReg(Src);
      Changed = true;
    }
  }
  return Changed;
}

bool X86GoFixupPreRegAllocPass::runOnMachineFunction(MachineFunction &fn) {
  MF = &fn;
  // This is used for Go function only.
  if (!MF->getFunction().hasGC() || MF->getFunction().getGC() != "go") {
    return false;
  }

  TRI = MF->getSubtarget<X86Subtarget>().getRegisterInfo();
  TII = MF->getSubtarget<X86Subtarget>().getInstrInfo();
  MRI = &MF->getRegInfo();

  bool Changed = false;

  Changed |= optimizeOpSP();
  Changed |= cloneStackPointers();
  Changed |= cloneRdiIfNeeded();
  Changed |= checkHoistedPointers();
  Changed |= checkDerivedPointers();

  return Changed;
}

static bool isTwoAddrUse(MachineInstr &MI, Register Reg, Register &DstReg) {
  for (unsigned i = 0, NumOps = MI.getNumOperands(); i != NumOps; ++i) {
    const MachineOperand &MO = MI.getOperand(i);
    if (!MO.isReg() || !MO.isUse() || MO.getReg() != Reg)
      continue;
    unsigned ti;
    if (MI.isRegTiedToDefOperand(i, &ti)) {
      DstReg = MI.getOperand(ti).getReg();
      return true;
    }
  }
  return false;
}

static bool usedInPhi(MachineInstr &MI, std::set<MachineInstr*> &visited) {
  if (!visited.insert(&MI).second) {
    return false;
  }
  bool result = false;
  assert(MI.getNumDefs() > 0 && "COPY must have 1 def.");

  unsigned Reg = MI.getOperand(0).getReg();
  if (Register::isPhysicalRegister(Reg)) {
    return false;
  }
  llvm::MachineFunction *MF = MI.getParent()->getParent();
  llvm::MachineRegisterInfo &MRI = MF->getRegInfo();

  for (auto &Use : MRI.use_nodbg_instructions(Reg)) {
    if (Use.isPHI()) {
      return true;
    }
  }
  return result;
}

void X86GoFixupPreRegAllocPass::AddOperandToStatePoint(
    MachineInstr& MI, const SmallSetVector<unsigned, 8>& Regs, bool isRegIndex) {
  assert(MI.getOpcode() == TargetOpcode::STATEPOINT);
  StatepointOpers SO(&MI);
  int64_t NumDeoptArgs = SO.getNumDeoptArgs();
  int64_t NumAddedArgs = int64_t(Regs.size());
  SmallVector<MachineOperand, 4> SavedOps;
  SavedOps.reserve(MI.getNumOperands() - SO.getNumGCPtrIdx());

  // E must be pre-computed because we have modify MI. SO may not work properly.
  unsigned E = SO.getNumGCPtrIdx() -  1;
  for (unsigned Idx = MI.getNumOperands() - 1; Idx >= E; Idx--) {
    SavedOps.push_back(MI.getOperand(Idx));
    MI.removeOperand(Idx);
  }
  for (auto R : Regs) {
    if (isRegIndex)
      R = Register::index2VirtReg(R);
    MI.addOperand(MachineOperand::CreateReg(R, false));
  }
  for (auto &MO : llvm::reverse(SavedOps))
    MI.addOperand(MO);

  MI.getOperand(SO.getNumDeoptArgsIdx()).setImm(NumDeoptArgs + NumAddedArgs);
}

bool X86GoFixupPreRegAllocPass::optimizeOpSP() {
  bool Changed = false;

  while (true) {
    std::vector<RemoveRedundantCopy> RemoveOpSP;

    for (auto &MBB : *MF) {
      for (auto &MI : MBB) {
        if (MI.isCopy()) {
          const MachineOperand &Dst = MI.getOperand(0);
          const MachineOperand &Src = MI.getOperand(1);
          if (Src.isReg() && Register::isPhysicalRegister(Src.getReg()) && isGoStackPointerRegister(Src.getReg())) {
            if (!Dst.isReg() || !Register::isVirtualRegister(Dst.getReg())) {
              continue;
            }
            std::set<MachineInstr*> visited;
            if (usedInPhi(MI, visited)) {
              continue;
            }
            RemoveOpSP.push_back({&MI, Src.getReg(), Dst.getReg()});
          }
          continue;
        }
      }
    }

    if (RemoveOpSP.empty()) {
      break;
    }

    for (auto & Op : RemoveOpSP) {
      for (auto &MBB : *MF) {
        std::vector<MachineInstr*> ToBeReplaced;
        for (auto &MI : MBB) {
          if (Op.Copy == &MI) {
            continue;
          }
          Register DstReg;
          if (isTwoAddrUse(MI, Op.Dst, DstReg)) {
            switch (MI.getOpcode()) {
              case X86::ADD64ri8:
              case X86::ADD64ri32: {
                // TODO: duffzero may produce -0x20, rsp
                // cannot access out-of-bounds value with OpSP
                // MachineOperand &Imm = MI.getOperand(2);
                // assert(Imm.isImm() && Imm.getImm() >= 0 && "invariant: must be positive imm");
                break;
              }
              case X86::SUB64ri8:
              case X86::SUB64ri32: {
                // MachineOperand &Imm = MI.getOperand(2);
                // // cannot access out-of-bounds value with OpSP
                // assert(Imm.isImm() && Imm.getImm() <= 0 && "invariant: must be negative imm");
                break;
              }
              case X86::INC64r:
                break;
              default:
                dbgs() << "cannot handle MI: " << MI << "\n";
                MF->dump();
                report_fatal_error("TODO: do not know how to handle two address");
            }

            // we cannot define RSP here so we cannot simply replace rsp
            ToBeReplaced.push_back(&MI);
            continue;
          }
          // other cases:
          Op.replace(MI);
        }
        for (MachineInstr *MI : ToBeReplaced) {
          // TODO: it is very dangerous here because ADD/SUB may redefine eflags
          // but LEAQ does not. Fortunately, OpSP should only be used compute address.
          // Nevertheless, we still need add some check here.
          MachineInstr *NewMI = TII->convertToThreeAddress(*MI, nullptr, nullptr);
          if (!NewMI)
            report_fatal_error("cannot convert to three address");
          Op.replace(*NewMI);
          MI->eraseFromParent();
        }
      }
      Op.Copy->eraseFromParent();
    }

    Changed = true;
  }

  return Changed;
}

// If a stack address load instruction lives through a statepoint,
// we should copy it to a position after the statepoint.
// By this case, we can avoid spill and restore stack locations.
// LEAQ has no side-effects. We should be able to insert LEAQ in any places.
bool X86GoFixupPreRegAllocPass::cloneStackPointers() {
  if (!CloneStackPointers)
    return false;

  DenseMap<unsigned, MachineInstr*> RegIdx2StackAddrLoad;
  int FrameIndex;
  for (MachineBasicBlock &MBB : *MF)
    for (MachineInstr &MI : MBB)
      if (auto Reg = TII->isLoadStackSlotAddress(MI, FrameIndex))
        if (Register::isVirtualRegister(Reg) && !MRI->use_empty(Reg)) {
          unsigned RegIdx = Register::virtReg2Index(Reg);
          RegIdx2StackAddrLoad[RegIdx] = &MI; // record for late copy
        }

  if (RegIdx2StackAddrLoad.empty())
    return false;

  bool Changed = false;

  // Map to frame
  DenseMap<MachineBasicBlock*, DenseMap<unsigned, unsigned>> LiveOutValues;

  // We need to record the first use point of a reset stack address.
  // An instruction may access multiple pointers, e.g., move a stack
  // pointer into another stack pointer. So we need to use a map.
  DenseMap<MachineInstr *, DenseMap<unsigned, unsigned>> Redefine;

  std::deque<MachineBasicBlock*> Worklist;
  BitVector InList(MF->getNumBlockIDs());

  auto JoinDataFlow = [&](MachineBasicBlock *MBB, DenseMap<unsigned, unsigned> &Live) {
    for (MachineBasicBlock *Pred : MBB->predecessors()) {
      DenseMap<unsigned, unsigned> &PredLiveOut = LiveOutValues[Pred];
      for (auto It : PredLiveOut) {
        auto It2 = Live.find(It.first);
        // should not exist conflict
        if (It2 != Live.end()) {
          if (It2->second != It.second) {
            // Conflicts may happen. If happened, we must introduce a new virtual register
            // here to resolve the conflict. Here we insert 0 and let following uses to
            // to create a new virtual register.
            It2->second = 0;
          }
        } else {
          Live.insert(It);
        }
      }
    }
  };

  auto ProcessMBB = [&](MachineBasicBlock *MBB) {
    // Step 0: initialize the LiveIn of the first instruction.
    DenseMap<unsigned, unsigned> Live;
    JoinDataFlow(MBB, Live);

    for (auto &MI : *MBB) {
      // PHI is handled differently. If a stack address is used by PHI, we may need
      // to store its value to stack or copy to some physical address to support PHI.
      if (MI.isPHI())
        continue;

      // Live is the LiveIn of MI.
      if (auto Reg = TII->isLoadStackSlotAddress(MI, FrameIndex)) {
        unsigned RegIdx = Register::virtReg2Index(Reg);
        Live.insert(std::make_pair(RegIdx, RegIdx));
        continue;
      }

      // Add uses
      for (const MachineOperand &MO : MI.operands()) {
        if (MO.isReg() && MO.isUse() && Register::isVirtualRegister(MO.getReg())) {
          unsigned RegIdx = Register::virtReg2Index(MO.getReg());
          if (RegIdx2StackAddrLoad.count(RegIdx)) {
            assert(Live.count(RegIdx) && "must be defined");
            unsigned NewRegIdx = Live[RegIdx];

            if (NewRegIdx == 0) {
              DenseMap<unsigned, unsigned> &RDefs = Redefine[&MI];
              if (RDefs.count(RegIdx)) {
                NewRegIdx = RDefs[RegIdx];
                assert(NewRegIdx != 0 && "invariant");
              } else {
                // TODO: assert 64-bit
                unsigned NewReg = MRI->createVirtualRegister(MRI->getRegClass(MO.getReg()));
                NewRegIdx = Register::virtReg2Index(NewReg);
                RDefs[RegIdx] = NewRegIdx;
              }
              Live[RegIdx] = NewRegIdx;
            }
          }
        }
      }

      // add kill
      if (MI.getOpcode() == TargetOpcode::STATEPOINT) {
        // A statepoint resets all defined stack address.
        for (auto &It : Live) {
          It.second = 0;
        }
      }
    }

    // Update Worklist
    DenseMap<unsigned, unsigned> &LiveOut = LiveOutValues[MBB];
    if (Live != LiveOut) {
      LiveOut = Live;
      for (MachineBasicBlock * Succ: MBB->successors()) {
        if (InList.test(Succ->getNumber()))
          continue;
        Worklist.push_back(Succ);
        InList.set(Succ->getNumber());
      }
    }
  };

  // inverse post order. Here, post_order should be efficient.
  std::vector<MachineBasicBlock*> IPO;
  IPO.reserve(MF->getNumBlockIDs());
  for (MachineBasicBlock *MBB : post_order(MF)) {
    IPO.push_back(MBB);
  }

  // forward data flow analysis
  for (MachineBasicBlock *MBB : llvm::reverse(IPO)) {
    ProcessMBB(MBB);
  }

  // solving: propagate until reaching the fixed point
  while (!Worklist.empty()) {
    MachineBasicBlock *MBB = Worklist.front();
    Worklist.pop_front();
    InList.reset(MBB->getNumber());
    ProcessMBB(MBB);
  }

  // Map new reg to the defining instruction
  DenseMap<unsigned, MachineInstr*> Cloned;
  // epilogue: insert LEAQ
  for (MachineBasicBlock *MBB : llvm::reverse(IPO)) {
    // still compute a valid live-in
    DenseMap<unsigned, unsigned> Live;
    JoinDataFlow(MBB, Live);

    for (MachineInstr &MI : *MBB) {
      if (MI.isPHI())
        continue;

      // Live is the LiveIn of MI.
      if (auto Reg = TII->isLoadStackSlotAddress(MI, FrameIndex)) {
        unsigned RegIdx = Register::virtReg2Index(Reg);
        Live.insert(std::make_pair(RegIdx, RegIdx));
      } else {
        // Now insert redefine introduced by use
        if (Redefine.count(&MI)) {
          DenseMap<unsigned, unsigned> &RDefs = Redefine[&MI];
          for (auto It : RDefs) {
            Live[It.first] = It.second;
          }
        }
      }

      // Check use
      for (MachineOperand &MO : MI.operands()) {
        if (MO.isReg() && MO.isUse() && Register::isVirtualRegister(MO.getReg())) {
          unsigned RegIdx = Register::virtReg2Index(MO.getReg());
          if (RegIdx2StackAddrLoad.count(RegIdx)) {
            assert(Live.count(RegIdx) && "must be defined");
            unsigned NewRegIdx = Live[RegIdx];
            assert(NewRegIdx != 0 && "must create first");
            if (NewRegIdx != RegIdx) {
              Register NewReg = Register::index2VirtReg(NewRegIdx);
              if (!Cloned.count(NewRegIdx)) {
                MachineInstr *OldMI = RegIdx2StackAddrLoad[RegIdx];
                assert(OldMI != nullptr && "invariant");
                MachineInstr &NewMI = TII->duplicate(*MBB, MI.getIterator(), *OldMI);
                // LEAQ
                assert(NewMI.getOperand(0).isReg() && "invariant");
                assert(NewMI.getOperand(0).getReg() == MO.getReg() && "invariant");

                // update the cloned Reg to new reg
                NewMI.getOperand(0).setReg(NewReg);
                Cloned[NewRegIdx] = &NewMI;
              }
              // Now update MO
              unsigned SubIdx = MO.getSubReg();
              MO.setReg(NewReg);
              // MO.ChangeToRegister(NewReg, false);
              if (SubIdx) {
                MO.setSubReg(SubIdx);
                // dbgs() << "SubIdx: " << SubIdx << "\n";
                // MF->dump();
                // llvm_unreachable("cannot use subreg");
              }
              Changed = true;
            }
          }
        }
      }

      // update KILL
      if (MI.getOpcode() == TargetOpcode::STATEPOINT) {
        // A statepoint kills all defined stack address.
        for (auto &It : Live) {
          It.second = 0;
        }
      }
    }
  }

  // For PHI
  for (MachineBasicBlock &MBB : *MF) {
    for (MachineInstr &MI : MBB) {
      if (!MI.isPHI())
        continue;

      for (unsigned I = 1, E = MI.getNumOperands(); I != E; I += 2) {
        Register Reg = MI.getOperand(I).getReg();
        if (!Register::isVirtualRegister(Reg))
          continue;
        unsigned RegIdx = Register::virtReg2Index(Reg);
        if (!RegIdx2StackAddrLoad.count(RegIdx)) {
          continue;
        }
        MachineBasicBlock *PreMBB = MI.getOperand(I + 1).getMBB();
        DenseMap<unsigned, unsigned> &LiveOut = LiveOutValues[PreMBB];
        auto It = LiveOut.find(RegIdx);
        if (It != LiveOut.end() && It->second != 0) {
          MI.getOperand(I).setReg(Register::index2VirtReg(It->second));
        } else {
          // insert a new cloned LEAQ
          MachineInstr *OldMI = RegIdx2StackAddrLoad[RegIdx];
          assert(OldMI != nullptr && "invariant");

          MachineInstr *Pos = nullptr;
          for (auto &MI : llvm::reverse(*PreMBB)) {
            if (MI.isPHI() || MI.getOpcode() == TargetOpcode::STATEPOINT) {
              break;
            }
            if (OldMI == &MI) {
              Pos = nullptr;
              break;
            }
            Pos = &MI;
            if (MI.isBranch())
              continue;
          }

          if (Pos != nullptr) {
            Register NewReg = MRI->createVirtualRegister(MRI->getRegClass(Reg));
            unsigned NewRegIdx = Register::virtReg2Index(NewReg);
            MachineInstr &NewMI = TII->duplicate(*PreMBB, Pos->getIterator(), *OldMI);
            // LEAQ
            assert(NewMI.getOperand(0).isReg() && "invariant");
            assert(NewMI.getOperand(0).getReg() == MI.getOperand(I).getReg() && "invariant");

            // update the cloned Reg to new reg
            NewMI.getOperand(0).setReg(NewReg);
            Changed = true;

            // Now update MO
            MI.getOperand(I).setReg(NewReg);
            // Modifies LiveOut anyway
            LiveOut[RegIdx] = NewRegIdx;
          }
        }
      }
    }
  }

  if (Changed) {
    for (auto &It : RegIdx2StackAddrLoad) {
      Register Reg = It.second->getOperand(0).getReg();
      if (MRI->use_empty(Reg)) {
        // Remove the unused original LEA
        It.second->eraseFromParent();
      }
    }
  }

  return Changed;
}

// entry:
//   lea 0x40(%rsp),%rcx
//   add $-0x20,%rcx
//   mov %rcx,0x1c8(%rsp)
//...
// loope_header:
//   statepoint             // 0x1c8(%rsp) not get updated when copystack
//   mov 0x1c8(%rsp), rdi
//   duffzero
//   ...
// loop_body:
//   mov 0xd0(rsp), rax
bool X86GoFixupPreRegAllocPass::cloneRdiIfNeeded() {
  bool Changed = false;
  uint16_t ChangedNum = 0;

  auto GetFuncName = [](MachineInstr* MI) -> StringRef {
    if (MI->isCall()) {
      for (const MachineOperand &MO : MI->operands()) {
        if (MO.isGlobal()) {
          const GlobalValue *GV = MO.getGlobal();
          if (const Function *F = dyn_cast<Function>(GV)) {
            return F->getName();
          }
        }
      }
    }
    return "";
  };
  // duffzero -> rdi
  DenseMap<MachineInstr*, MachineInstr*> Calls;
  for (MachineBasicBlock &MBB : *MF) {
    for (MachineInstr &MI : MBB) {
      if (GetFuncName(&MI) == "runtime.duffzero") {
        MachineInstr *Pre = MI.getPrevNode();
        if (MachineOperand &MO = Pre->getOperand(0);
            Pre->isCopy() && MO.isReg() && MO.getReg() == X86::RDI &&
            Register::isVirtualRegister(Pre->getOperand(1).getReg()))
          Calls.insert({&MI, Pre});
      }
    }
  }
  if (Calls.empty())
    return false;

  auto GetDef = [&](MachineInstr* MI, uint8_t& Found) -> MachineInstr* {
    int FrameIndex;
    Register Reg = TII->isLoadStackSlotAddress(*MI, FrameIndex);
    if (Reg) {
      Found |= 0x2;
      return nullptr;
    }
    if (MI->isCopy() && MI->getOperand(1).isReg()) {
      Reg = MI->getOperand(1).getReg();
    } else if (MI->getOpcode() == X86::ADD64ri32 ||
               MI->getOpcode() == X86::ADD64ri8 ||
               MI->getOpcode() == X86::SUB64ri32 ||
               MI->getOpcode() == X86::SUB64ri8) {
      Found |= 0x1;
      Reg = MI->getOperand(1).getReg();
    }
    if (Register::isVirtualRegister(Reg))
      return MRI->getVRegDef(Reg);

    return nullptr;
  };

  for (auto &P : Calls) {
    SmallVector<MachineInstr*> Stack;
    MachineInstr* Src = P.second;
    // todo cross state point check
    uint8_t Found = 0x0;
    while (MachineInstr* Def = GetDef(Src, Found)) {
      Stack.push_back(Def);
      Src = Def;
    }
    if (Found != 0x3)
      continue;

    Changed = true;
    ChangedNum++;

    Register PreReg = MCRegister::NoRegister;
    SmallVector<MachineInstr*> OldMIs;
    while (!Stack.empty()) {
      MachineInstr *OldMI = Stack.pop_back_val();
      OldMIs.push_back(OldMI);
      Register OldReg = OldMI->getOperand(0).getReg();
      unsigned OldSubReg = OldMI->getOperand(0).getSubReg();
      Register NewReg = MRI->createVirtualRegister(MRI->getRegClass(OldReg));
      MachineInstr &NewMI = TII->duplicate(*P.first->getParent(), P.second, *OldMI);
      NewMI.getOperand(0).setReg(NewReg);
      if (OldSubReg)
        NewMI.getOperand(0).setSubReg(OldSubReg);
      if (Register::isVirtualRegister(PreReg)) {
        unsigned OldPreSubReg = OldMI->getOperand(1).getSubReg();
        NewMI.getOperand(1).setReg(PreReg);
        if (OldPreSubReg)
          NewMI.getOperand(1).setSubReg(OldPreSubReg);
      }
      PreReg = NewReg;
      if (Stack.empty())
        P.second->getOperand(1).setReg(NewReg);
    }
    while (!OldMIs.empty()) {
      MachineInstr* MI = OldMIs.pop_back_val();
      if (MRI->use_empty(MI->getOperand(0).getReg()))
        MI->eraseFromParent();
    }
  }

  return Changed;
}

// This should be implemented as a target independent pass.
// However, we need to place it before RA because other
// machine optimization may move pointers. During RA, we should
// carefully place spilled values.
bool X86GoFixupPreRegAllocPass::checkHoistedPointers() {
  if (!CheckHoistedPointers)
    return false;

  bool Changed = false;

  DenseMap<MachineBasicBlock*, SmallSetVector<unsigned, 8>> LiveInValues;

  std::deque<MachineBasicBlock*> Worklist;
  BitVector InList(MF->getNumBlockIDs());

  auto ProcessMBB = [&](MachineBasicBlock *MBB) {
    // Step 0: initialize the LiveOut of the last instruction.
    SmallSetVector<unsigned, 8> Live;
    for (MachineBasicBlock *Succ : MBB->successors()) {
      SmallSetVector<unsigned, 8> &SuccLiveIn = LiveInValues[Succ];
      Live.set_union(SuccLiveIn);
    }

    // Collected generated pointers
    for (auto &MI : llvm::reverse(*MBB)) {
      // Live is the LiveOut of MI.
      for (const MachineOperand &MO : MI.operands()) {
        if (MO.isReg() && MO.isDef() && Register::isVirtualRegister(MO.getReg())) {
          unsigned RegIdx = Register::virtReg2Index(MO.getReg());
          Live.remove(RegIdx);
        }
      }

      // After kill, Live now is the LiveIn of MI. We need to add
      // all tracked GC pointers to make Live become the LiveOut of
      // the next instruction.
      if (MI.getOpcode() == TargetOpcode::STATEPOINT) {
        StatepointOpers SO(&MI);
        for (unsigned Idx = SO.getNumDeoptArgsIdx(), E = SO.getNumGCPtrIdx(); Idx < E;
            ++Idx) {
          const MachineOperand &MO = MI.getOperand(Idx);
          if (!MO.isReg())
            continue;
          if (MO.isDef())
            continue;
          if (!Register::isVirtualRegister(MO.getReg())) {
            continue;
          }

          unsigned RegIdx = Register::virtReg2Index(MO.getReg());
          Live.insert(RegIdx);
        }
      }
    }

    // Update Worklist
    SmallSetVector<unsigned, 8> &LiveIn = LiveInValues[MBB];
    if (Live.size() != LiveIn.size()) {
      LiveIn = Live;
      for (MachineBasicBlock * Pred : MBB->predecessors()) {
        if (InList.test(Pred->getNumber()))
          continue;
        Worklist.push_back(Pred);
        InList.set(Pred->getNumber());
      }
    }
  };

  // prologue: collect initial data flow facts
  for (MachineBasicBlock *MBB : post_order(MF)) {
    ProcessMBB(MBB);
  }

  // solving: propagate until reaching the fixed point
  while (!Worklist.empty()) {
    MachineBasicBlock *MBB = Worklist.front();
    Worklist.pop_front();
    InList.reset(MBB->getNumber());
    ProcessMBB(MBB);
  }

  // epilogue: add missing live pointers
  for (MachineBasicBlock &MBB : *MF) {
    SmallSetVector<unsigned, 8> Live;
    for (MachineBasicBlock *Succ : MBB.successors()) {
      SmallSetVector<unsigned, 8> &SuccLiveIn = LiveInValues[Succ];
      Live.set_union(SuccLiveIn);
    }
    // Live now is stable.
    // Collected generated pointers
    for (auto &MI : llvm::reverse(MBB)) {
      // Process killed pointers
      for (const MachineOperand &MO : MI.operands()) {
        if (MO.isReg() && MO.isDef() && Register::isVirtualRegister(MO.getReg())) {
          unsigned RegIdx = Register::virtReg2Index(MO.getReg());
          Live.remove(RegIdx);
        }
      }

      if (MI.getOpcode() == TargetOpcode::STATEPOINT) {
        StatepointOpers SO(&MI);
        SmallSetVector<unsigned, 8> MILive;
        for (unsigned Idx = SO.getNumDeoptArgsIdx(), E = SO.getNumGCPtrIdx(); Idx < E;
            ++Idx) {
          const MachineOperand &MO = MI.getOperand(Idx);
          if (!MO.isReg())
            continue;
          if (MO.isDef())
            continue;
          if (!Register::isVirtualRegister(MO.getReg())) {
            continue;
          }

          unsigned RegIdx = Register::virtReg2Index(MO.getReg());
          MILive.insert(RegIdx);
        }
        // Live should be equal to MILive. If not, we need to add missing pointers.
        // Here, we only need to check missing pointers.
        Live.set_subtract(MILive);
        if (!Live.empty()) {
          // MachineInstr does not have an API that we can insert an MO.
          // We have to remove all trailing MO and save them in a list,
          // create and add new MO, and then add previously saved MOs.
          AddOperandToStatePoint(MI, Live, true);
          Changed = true;
        }
        Live.set_union(MILive);
      }
    }
  }

  return Changed;
}


// there will be pointers created in machine code than in IR, so GoStatePoint
// can't indentify them. these pointers generally created by LEA/ADD/SUB, we
// need to add them to the StatePoints which between the def and the use if
// the Base pointers are already in the StatePoints
bool X86GoFixupPreRegAllocPass::checkDerivedPointers() {
  if (!CheckDerivedPointers)
    return false;

  bool Changed = false;
  DenseSet<unsigned> CandidateRegs;
  DenseMap<unsigned, SmallSetVector<unsigned, 8>> BaseToDerived;
  DenseMap<unsigned, SmallSetVector<unsigned, 8>> DerivedToBase;
  DenseMap<MachineBasicBlock*, SetVector<unsigned>> LiveInValues;
  DenseMap<MachineBasicBlock*, SetVector<unsigned>> LiveOutValues;
  DenseMap<MachineBasicBlock*, SetVector<unsigned>> UsedInBlock;
  DenseMap<MachineBasicBlock*, SetVector<unsigned>> DefinedInBlock;

  ////// Debug only
  auto RegName = [&](unsigned Reg) -> std::string {
    if (Register::isVirtualRegister(Reg)) {
      auto Name = MRI->getVRegName(Reg).str();
      if (Name == "") {
        Name = std::to_string(Register::virtReg2Index(Reg));
      }
      return "%" + Name;
    } else {
      return "$" + std::string(TRI->getName(Reg));
    }
  };
  auto PrintLive = [&](MachineBasicBlock* MBB, std::string Title,
                       SetVector<unsigned>& Live) {
    if (!DebugCheckDerivedPointers)
      return;
    dbgs() << "# " << Title << " - " << MBB->getName() << ": ";
    for (auto C : Live) {
      dbgs() << " " << RegName(C);
    }
    dbgs() << "\n";
  };
  ////// Debug only

  auto GetBaseReg = [&](MachineInstr &MI) -> Register {
    Register Result = MCRegister::NoRegister;
    auto Opcode = MI.getOpcode();
    // just handle these three cases now. we don't care if the derived/base
    // pointer is a valid pointer or not, because we only add it into
    // state point when the base/derived pointer is in the state point
    switch (Opcode) {
    case X86::LEA64r: {
      MachineOperand &MOBase = MI.getOperand(1 + X86::AddrBaseReg);
      MachineOperand &MOScale = MI.getOperand(1 + X86::AddrScaleAmt);
      MachineOperand &MOIndex = MI.getOperand(1 + X86::AddrIndexReg);
      MachineOperand &MODisp = MI.getOperand(1 + X86::AddrDisp);
      if (MOBase.isReg() && Register::isVirtualRegister(MOBase.getReg()) &&
          (MOIndex.getReg() == MCRegister::NoRegister ||
           (MOScale.isImm() && MOScale.getImm() != 1)) &&
          MODisp.isImm() && MODisp.getImm() >= 0)
        Result = MOBase.getReg();
      break;
    }
    case X86::ADD64ri32:
    case X86::ADD64ri8: {
      if (Register::isVirtualRegister(MI.getOperand(1).getReg()) &&
          MI.getOperand(2).getImm() >= 0)
        Result = MI.getOperand(1).getReg();
      break;
    }
    case X86::SUB64ri32:
    case X86::SUB64ri8: {
      if (Register::isVirtualRegister(MI.getOperand(1).getReg()) &&
          MI.getOperand(2).getImm() <= 0)
        Result = MI.getOperand(1).getReg();
      break;
    }
    default: {
      // anything else?
      break;
    }
    }
    return Result;
  };
  std::function<void(unsigned,MachineInstr&,unsigned,MachineInstr&)> CollectUse;
  CollectUse = [&](unsigned Reg, MachineInstr &MI, unsigned Base, MachineInstr& BaseMI) {
    if (Register::isVirtualRegister(Reg) && !MRI->use_empty(Reg)) {
      DerivedToBase[Reg].insert(Base);
      DefinedInBlock[MI.getParent()].insert(Reg);
      DefinedInBlock[BaseMI.getParent()].insert(Base);
      Register Regs[2] = {Reg, Base};
      for (int Idx = 0; Idx < 2; Idx++) {
        Register Tmp = Regs[Idx];
        for (auto I = MRI->use_instr_begin(Tmp), E = MRI->use_instr_end();
             I != E; I++) {
          auto UsedMBB = I->getParent();
          if (I->isPHI()) {
            // don't consider Phi's incoming value as real use, adding it
            // to predBB's LiveOut
            for (unsigned i = 1, e = I->getNumOperands(); i < e; i += 2) {
              if (!I->getOperand(i).isReg())
                continue;
              auto IncomingReg = I->getOperand(i).getReg();
              auto IncomingBlock = I->getOperand(i + 1).getMBB();
              if (IncomingReg == Tmp)
                LiveOutValues[IncomingBlock].insert(Tmp);
            }
          } else if(I->getOpcode() == TargetOpcode::STATEPOINT) {
            StatepointOpers SO(&*I);
            for (unsigned  i = 0; i < SO.getCCIdx(); i++)
              if (I->getOperand(i).isReg() && I->getOperand(i).getReg() == Tmp)
                UsedInBlock[UsedMBB].insert(Tmp);
          } else {
            UsedInBlock[UsedMBB].insert(Tmp);
          }
        }
      }
    }
  };
  // find registers created by LEA/ADD/SUB
  for (MachineBasicBlock &MBB : *MF) {
    for (MachineInstr &MI : MBB) {
      if (auto Base = GetBaseReg(MI)) {
        MachineInstr &BaseMI = *MRI->def_instructions(Base).begin();
        CollectUse(MI.getOperand(0).getReg(), MI, Base, BaseMI);
      }
    }
  }

  if (DerivedToBase.empty()) {
    return false;
  }

  // propagate Bases
  bool BaseAdded = true;
  while (BaseAdded) {
    BaseAdded = false;
    for (auto& Pair : DerivedToBase) {
      SmallSetVector<unsigned, 8> NewElements;
      for (auto Base : Pair.second) {
        if (!DerivedToBase.count(Base))
          continue;
        auto& Ancestors = DerivedToBase[Base];
        for (auto Ancestor : Ancestors) {
          if (!Pair.second.count(Ancestor)) {
            NewElements.insert(Ancestor);
          }
        }
      }
      if (!NewElements.empty()) {
        Pair.second.set_union(NewElements);
        BaseAdded = true;
      }
    }
  }
  for (auto& Pair : DerivedToBase) {
    unsigned Derived = Pair.first;
    CandidateRegs.insert(Derived);
    for (unsigned Base : Pair.second) {
      BaseToDerived[Base].insert(Derived);
      CandidateRegs.insert(Base);
    }
  }

  if (DebugCheckDerivedPointers) {
    dbgs() << "### Candidates: ";
    for (unsigned Reg : CandidateRegs)
      dbgs() << RegName(Reg) << " ";
    dbgs() << "\n";
    for (auto &P : DerivedToBase) {
      dbgs() << "### Reg-Bases: " << RegName(P.first) << "\n";
      for (auto &C : P.second)
        dbgs() << "       Bases: " << RegName(C) << " ";
      dbgs() << "\n";
    }
  }

  // find the blocks which should keep these registers live by a
  // 'liveness-like' anlaysis
  SmallSetVector<MachineBasicBlock *, 32> Worklist;
  for (MachineBasicBlock &MBB : *MF) {
    PrintLive(&MBB, "Used-init", UsedInBlock[&MBB]);
    PrintLive(&MBB, "Defed-init", DefinedInBlock[&MBB]);
    LiveInValues[&MBB].set_union(UsedInBlock[&MBB]);
    LiveInValues[&MBB].set_subtract(DefinedInBlock[&MBB]);
    if (!LiveInValues[&MBB].empty())
      Worklist.insert(MBB.predecessors().begin(), MBB.predecessors().end());
    PrintLive(&MBB, "LiveIn-init", LiveInValues[&MBB]);
  }

  while (!Worklist.empty()) {
    MachineBasicBlock* MBB = Worklist.pop_back_val();
    if (DebugCheckDerivedPointers)
      dbgs() << "\n### analyzing MBB: " << MBB->getName() << "\n";
    auto OldSize = LiveOutValues[MBB].size();
    for (MachineBasicBlock* Succ : MBB->successors()) {
      if (LiveInValues.count(Succ)) {
        PrintLive(Succ, "LiveIn-succ", LiveInValues[Succ]);
        LiveOutValues[MBB].set_union(LiveInValues[Succ]);
      }
    }
    PrintLive(MBB, "LiveOut", LiveOutValues[MBB]);
    if (OldSize == LiveOutValues[MBB].size())
      continue;

    auto LiveTmp = LiveOutValues[MBB];
    PrintLive(MBB, "LiveIn(before)", LiveInValues[MBB]);
    PrintLive(MBB, "Used", UsedInBlock[MBB]);
    PrintLive(MBB, "Defined", DefinedInBlock[MBB]);
    LiveTmp.set_union(UsedInBlock[MBB]);
    LiveTmp.set_subtract(DefinedInBlock[MBB]);
    if (LiveTmp != LiveInValues[MBB]) {
      LiveInValues[MBB] = LiveTmp;
      Worklist.insert(MBB->predecessors().begin(), MBB->predecessors().end());
    }
    PrintLive(MBB, "LiveIn(after)", LiveInValues[MBB]);
  }

  if (DebugCheckDerivedPointers)
    for (auto& P : LiveOutValues)
      PrintLive(P.first, "LiveOut-result", P.second);

  size_t ChangedSO = 0;
  size_t AddedPointer = 0;
  for (MachineBasicBlock &MBB : *MF) {
    SetVector<unsigned> LiveTmp = LiveOutValues[&MBB];
    for (MachineInstr &MI : llvm::reverse(MBB)) {
      unsigned UseEnd = MI.getNumOperands();
      if (MI.getOpcode() == TargetOpcode::STATEPOINT) {
        StatepointOpers SO(&MI);
        SmallSet<unsigned, 8> AlreadyLivedReg;
        for (auto Idx = SO.getNumDeoptArgsIdx(), E = SO.getNumGCPtrIdx();
             Idx < E; ++Idx) {
          auto &MO = MI.getOperand(Idx);
          if (MO.isReg()) {
            AlreadyLivedReg.insert(MO.getReg());
          }
        }

        SmallSetVector<unsigned, 8> NeedAddedReg;
        for (auto Reg : LiveTmp) {
          if (!CandidateRegs.contains(Reg))
            continue;

          // if the Base/Derived exists in StatePoint, we need to add this Reg
          // into StatePoint too.
          if (!AlreadyLivedReg.contains(Reg)) {
            const auto& Bases = DerivedToBase[Reg];
            const auto& Deriveds = BaseToDerived[Reg];
            SmallSetVector<unsigned, 8> All(Bases);
            All.set_union(Deriveds);
            for (auto Tmp : All) {
              if (AlreadyLivedReg.contains(Tmp)) {
                NeedAddedReg.insert(Reg);
                break;
              }
            }
          }
        }

        if (!NeedAddedReg.empty()) {
          Changed |= true;
          ChangedSO++;
          AddedPointer += NeedAddedReg.size();
          AddOperandToStatePoint(MI, NeedAddedReg, false);
        }

        UseEnd = SO.getCCIdx();
      }
      for (unsigned Idx = 0; Idx < UseEnd; Idx++) {
        MachineOperand& MO = MI.getOperand(Idx);
        if (MO.isReg() && Register::isVirtualRegister(MO.getReg()) &&
            CandidateRegs.count(MO.getReg())) {
          if (MO.isDef())
            LiveTmp.remove(MO.getReg());
          else
            LiveTmp.insert(MO.getReg());
        }
      }
    }
  }

  if (DebugCheckDerivedPointers && ChangedSO > 0)
    dbgs() << "Add " << AddedPointer << " pointers to " << ChangedSO
           << " StatePoints in " << MF->getName() << "\n";

  return Changed;
}
