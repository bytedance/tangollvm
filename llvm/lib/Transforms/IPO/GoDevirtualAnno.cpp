// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 ByteDance Ltd. and/or its affiliates.

//===----------------------------------------------------------------------===//
//
// This LLVM pass performs Go interface call devirtualization.
//
// In Go's runtime, interface method dispatch is indirect via "itab" tables.
// This pass identifies such indirect callsites (tagged via metadata),
// extracts the concrete target function if statically known, and rewrites
// the call to a direct call with conditional checks on the itab.
//
// The goal is to improve optimization opportunities (e.g., inlining) and
// allow downstream passes to see through the indirection.
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/IPO/GoDevirtualAnno.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/PatternMatch.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils/Local.h"
#include <algorithm>
#include <map>
#include <queue>
#include <unordered_set>
#include <vector>

namespace llvm {

#define DEBUG_TYPE "go-devirt"

// Statistics counters for tracking optimization effectiveness
STATISTIC(NumDirectRewrites, "Number of indirect calls rewritten to direct");
STATISTIC(NumFortRewrites, "Number of indirect calls rewritten to new branch");

// Command line options for controlling optimization behavior
static cl::opt<bool> DevirtualSingleCaseOnly("devirtual-single-case-only",
                                             cl::Hidden, cl::init(true));
static cl::opt<int32_t>
    DevirtualItemCandidateMaxCount("devirtual-item-candidate-max-count",
                                   cl::Hidden, cl::init(2));
static cl::opt<int32_t> DevirtualItemMaxCount("devirtual-item-max-count",
                                              cl::Hidden, cl::init(1));

// Debug output control - enables verbose logging when true
static constexpr bool VerboseDebug = false;
#define DEBUG_LOG(x)                                                           \
  do {                                                                         \
    if (VerboseDebug) {                                                        \
      dbgs() << "[GoDevirt] " << x << "\n";                                    \
    }                                                                          \
  } while (0)

// Go version-dependent itab symbol prefix (changes between Go versions)
// "go.itab" for older versions, "go:itab" for newer versions (1.22+)
static std::string GoItabPrefix;

// Enumeration for different devirtualization strategies
enum class DevirtualType {
  Replace, // Direct replacement of indirect call with direct call
  Fork     // Create conditional branches for multiple possible targets
};

// Core data structure representing a mapping between an itab anchor point
// and the concrete functions it can resolve to
struct ItabSlotMapping {
  Value *Anchor; // The anchor value (usually a load from itab)
  int SlotIndex; // Method slot index in the itab
  std::unordered_set<GlobalVariable *> Itabs; // Set of possible itab globals
  CallInst *Call;     // The indirect call to be devirtualized
  DevirtualType Type; // Strategy to use for devirtualization
};

// Represents a single path from an itab global to a value through the IR
struct ItabPath {
  GlobalVariable *Itab;       // Source itab global variable
  int32_t Slot = -1;          // Method slot index (-1 if unknown)
  Value *Leaf = nullptr;      // Final value in the propagation chain
  Value *Anchor = nullptr;    // Anchor point for the path
  std::vector<Value *> Chain; // Complete propagation chain (for debugging)
};

// Type aliases for commonly used maps
using ItabMap = DenseMap<Value *, std::vector<ItabPath>>;
using ItabPathMap =
    DenseMap<std::pair<Value *, int64_t>, std::vector<GlobalValue *>>;

// Utility function to check if a global variable represents a Go itab
// Go itabs have names like "go.itab.main.MyType.main.MyInterface" or
// "go:itab...."
static bool checkGoItab(GlobalValue &V) {
  return V.getName().starts_with(GoItabPrefix);
}

// Debug utility: Print detailed information about an ItabSlotMapping
static void printItabSlotMapping(const ItabSlotMapping &Entry) {
  dbgs() << "Anchor: ";
  if (Entry.Anchor)
    Entry.Anchor->print(dbgs());
  else
    dbgs() << "<nullptr>";
  dbgs() << " devirtual type: "
         << (Entry.Type == DevirtualType::Fork ? " FORK " : " REPLACE ") << " ";
  dbgs() << ", slot: " << Entry.SlotIndex << ", itabs: ";
  for (auto *GV : Entry.Itabs)
    dbgs() << (GV ? GV->getName() : "<null>") << ", ";
  dbgs() << "\n";
}

// Debug utility: Print information about a single ItabPath
static void printItabPath(Value *V, const ItabPath &P) {
  dbgs() << "  - Itab: " << (P.Itab ? P.Itab->getName() : "<null>");
  dbgs() << ":" << P.Slot;
  dbgs() << "\nAnchor: ";
  if (P.Anchor == nullptr) {
    dbgs() << "<null>\n";
  } else {
    P.Anchor->print(dbgs());
    dbgs() << "\n";
  }
  if (P.Leaf && P.Leaf != V) {
    dbgs() << "Leaf: ";
    P.Leaf->print(dbgs());
  }
  if (!P.Chain.empty()) {
    dbgs() << ", Chain:";
    for (Value *C : P.Chain) {
      dbgs() << "\n    -> ";
      C->print(dbgs());
    }
    dbgs() << "\n    -> ";
    V->print(dbgs());
  }
}

// Debug utility: Print all ItabPaths for a given value
static void printItabPaths(Value *V, const std::vector<ItabPath> &Paths) {
  dbgs() << "[ItabInfo] For value: ";
  V->print(dbgs());
  dbgs() << "len(paths): " << Paths.size();
  dbgs() << "\n";
  for (const auto &P : Paths) {
    printItabPath(V, P);
    dbgs() << "\n";
  }
}

// Debug utility: Print the entire ItabMap
static void printItabMap(ItabMap &ItabMap) {
  for (auto entry : ItabMap) {
    printItabPaths(entry.getFirst(), entry.getSecond());
  }
}

// Compute the "anchor" value for a given value
// The anchor is typically a load instruction or the base of a GEP
// This helps identify the root source of an itab-derived value
static Value *computeAnchor(Value *V) {
  if (V == nullptr) {
    return V;
  }
  // Load instructions serve as anchors since they read from itab globals
  if (isa<LoadInst>(V)) {
    return V;
  }
  // For GEP instructions, the base operand is the anchor
  if (auto *GEP = dyn_cast<GetElementPtrInst>(V)) {
    if (GEP->getNumOperands() >= 2) {
      return GEP->getOperand(0);
    }
  }
  return nullptr;
}

// Extract the method slot index from an anchor value
// In Go itabs, methods are stored at specific offsets (slots)
// This function tries to determine which slot is being accessed
static int32_t computeSlotFromAnchor(GlobalVariable *GV, Value *V) {
  if (V == nullptr) {
    return -1;
  }

  // Case 1: Direct GEP with constant index
  if (auto *GEP = dyn_cast<GetElementPtrInst>(V)) {
    if (GEP->getNumOperands() >= 2) {
      if (auto *CI = dyn_cast<ConstantInt>(GEP->getOperand(1))) {
        auto SlotVal = CI->getZExtValue();
        return SlotVal;
      }
    }
  }

  // Case 2: Load from constant expression GEP
  if (auto *IL = dyn_cast<LoadInst>(V)) {
    if (auto *CE = dyn_cast<ConstantExpr>(IL->getPointerOperand())) {
      if (CE->getOpcode() == Instruction::GetElementPtr &&
          CE->getNumOperands() >= 2) {
        if (auto *G = dyn_cast<GlobalVariable>(CE->getOperand(0))) {
          if (G != GV) {
            return -1;
          }
          if (auto *CI = dyn_cast<ConstantInt>(CE->getOperand(1))) {
            auto SlotVal = CI->getZExtValue();
            return SlotVal;
          }
        }
      }
    }
  }
  return -1;
}

// Determine the devirtualization strategy for a given itab and anchor
// Replace: Direct substitution (anchor is a direct load from the itab)
// Fork: Conditional branching (anchor is derived but not direct)
static DevirtualType computeType(GlobalVariable *GV, Value *V) {
  assert(V != nullptr && "V must be non-null");

  // If this is a direct load from the itab global, we can do direct replacement
  if (auto *IL = dyn_cast<LoadInst>(V)) {
    if (auto *CE = dyn_cast<ConstantExpr>(IL->getPointerOperand())) {
      if (CE->getOpcode() == Instruction::GetElementPtr &&
          CE->getNumOperands() >= 2) {
        if (auto *G = dyn_cast<GlobalVariable>(CE->getOperand(0))) {
          if (G == GV) {
            return DevirtualType::Replace;
          }
        }
      }
    }
  }

  // Otherwise, we need conditional branching
  return DevirtualType::Fork;
}

// Main function to build the complete itab propagation graph
// This performs a breadth-first search from each itab global to find
// all values that might be derived from that itab
static void buildItabGraph(Module &M, ItabMap &ResultMap) {
  // Process each Go itab global variable
  for (GlobalVariable &GV : M.globals()) {
    if (!checkGoItab(GV))
      continue;

    // BFS data structures
    using Elem = std::tuple<Value *, Value *, std::vector<Value *>>;
    std::queue<Elem> Q;
    std::unordered_set<Value *> Visited;

    // Start BFS from the itab global
    auto *Start = &GV;
    Q.push({Start, nullptr, {Start}});
    Visited.insert({Start, nullptr});

    while (!Q.empty()) {
      auto [V, Anchor, Path] = Q.front();
      Q.pop();

      // Check if we already have this path recorded
      bool AlreadyExists = llvm::any_of(ResultMap[V], [&](const ItabPath &P) {
        return P.Itab == &GV && P.Leaf == V && P.Anchor == Anchor;
      });

      // Helper functions for propagating to new values
      auto PropagateItab = [&](Value *U, Value *NewAnchor) {
        if (!Visited.insert(U).second) {
          return;
        }
        auto NewPath = Path;
        NewPath.push_back(U);
        Q.push({U, NewAnchor, NewPath});
      };
      auto PropagateItabNoOff = [&](Value *U) { PropagateItab(U, Anchor); };

      // Skip non-itab globals (they shouldn't reference other itabs)
      if (auto *G = dyn_cast<GlobalVariable>(V)) {
        if (!checkGoItab(*G) || &GV != G) {
          continue;
        }
      }
      // Skip constant data values (they don't propagate itab information)
      else if (isa<ConstantData>(V)) {
        continue;
      }

      // Record this path if it's new
      if (!AlreadyExists) {
        ResultMap[V].push_back({
            &GV,
            computeSlotFromAnchor(&GV, Anchor),
            V,
            computeAnchor(Anchor),
            Path,
        });
      }

      // Process all users of the current value
      for (User *U : V->users()) {
        if (auto *LI = dyn_cast<LoadInst>(U)) {
          PropagateItabNoOff(LI);
        } else if (auto *RT = dyn_cast<ReturnInst>(U)) {
          PropagateItabNoOff(RT);
        } else if (auto *CE = dyn_cast<ConstantExpr>(U)) {
          if (CE->getOpcode() == Instruction::GetElementPtr &&
              CE->getNumOperands() >= 2) {
            if (auto *G = dyn_cast<GlobalVariable>(CE->getOperand(0))) {
              if (G != &GV) {
                continue;
              }
              // Skip the first few slots (they contain type information, not
              // methods)
              if (auto *CI = dyn_cast<ConstantInt>(CE->getOperand(1))) {
                auto SlotVal = CI->getZExtValue();
                if (SlotVal < 24) { // Method slots start after type information
                  continue;
                }
              }
            }
            // Propagate through loads of this constant expression
            for (User *L : CE->users()) {
              if (isa<LoadInst>(L))
                PropagateItab(L, L);
            }
          }
        } else if (auto *CA = dyn_cast<ConstantAggregate>(U)) {
          for (unsigned i = 0; i < CA->getNumOperands(); ++i) {
            if (CA->getOperand(i) == &GV) {
              for (User *U2 : CA->users()) {
                // Skip pointer-to-integer conversions (not relevant for method
                // calls)
                if (isa<PtrToIntInst>(U2)) {
                  continue;
                }
                PropagateItabNoOff(U2);
              }
            }
          }
        } else if (auto *GEP = dyn_cast<GetElementPtrInst>(U)) {
          if (GEP->getNumOperands() >= 2) {
            if (auto *CI = dyn_cast<ConstantInt>(GEP->getOperand(1))) {
              auto SlotVal = CI->getZExtValue();
              if (SlotVal < 24) { // Skip non-method slots
                continue;
              }
              PropagateItab(GEP, GEP); // GEP becomes new anchor
            }
          }
        } else if (auto *SI = dyn_cast<SelectInst>(U)) {
          for (Value *Vsel : {SI->getTrueValue(), SI->getFalseValue()}) {
            PropagateItabNoOff(Vsel);
          }
        } else if (auto *SI = dyn_cast<StoreInst>(U)) {
          continue; // TODO: Handle store operations carefully
        } else if (auto *ICMP = dyn_cast<ICmpInst>(U)) {
          for (unsigned i = 0; i < ICMP->getNumOperands(); ++i) {
            auto Op = ICMP->getOperand(i);
            if (Op != &GV) {
              PropagateItabNoOff(Op);
            }
          }
        } else if (auto *CI = dyn_cast<CallInst>(U)) {
          Function *Callee = CI->getCalledFunction();
          if (!Callee || Callee->isDeclaration())
            continue;

          // Find which argument position contains our itab
          for (unsigned idx = 0; idx < CI->arg_size(); ++idx) {
            if (CI->getArgOperand(idx) != &GV)
              continue;
            Argument *Formal = Callee->getArg(idx);
            PropagateItabNoOff(Formal);
          }
        } else {
          PropagateItabNoOff(U);
        }
      }
    }
  }
}

/// Utility function for converting LLVM values to strings for debugging
static std::string valueToString(Value *V) {
  if (!V)
    return "<nullptr>";
  std::string S;
  raw_string_ostream RSO(S);
  V->print(RSO);
  return RSO.str();
}

/// Scan for interface method calls and match them with their itab information
/// Interface calls in Go are tagged with metadata and can be identified
/// by their indirect call pattern through function pointers loaded from itabs
static void collectItabCallsites(Module &M,
                                 std::vector<ItabSlotMapping> &Mappings,
                                 ItabMap &ItabMap) {
  // Scan all instructions in all functions
  for (Function &F : M) {
    for (Instruction &I : instructions(F)) {
      auto *CI = dyn_cast<CallInst>(&I);
      if (!CI || !CI->getCalledOperand())
        continue;

      // Get the function pointer being called (strip away casts)
      Value *Callee = CI->getCalledOperand()->stripPointerCasts();

      // Check if this call target has itab information
      auto It = ItabMap.find(Callee);
      if (It == ItabMap.end()) {
        continue;
      }
      auto paths = It->getSecond();
      if (paths.empty()) {
        continue;
      }

      if (VerboseDebug) {
        printItabPaths(CI, It->second);
      }

      // Group paths by (anchor, slot) pairs to create mappings
      DenseMap<std::pair<Value *, int64_t>, ItabSlotMapping> MappingMap;

      auto FillMappings = [&](Value *Anchor, int32_t Slot, GlobalVariable *Itab,
                              DevirtualType Type) {
        if (Type == DevirtualType::Fork && !Anchor->getType()->isPointerTy()) {
          return;
        }

        // Create or update mapping for this (anchor, slot) pair
        auto &SlotMap = MappingMap[{Anchor, Slot}];
        SlotMap.Anchor = Anchor;
        SlotMap.SlotIndex = Slot;
        SlotMap.Itabs.emplace(Itab);
        SlotMap.Call = CI;
        SlotMap.Type = Type;
      };

      // Generate mappings from all possible itab paths
      for (auto &P : paths) {
        if (P.Anchor == nullptr || P.Itab == nullptr) {
          continue;
        }
        FillMappings(P.Anchor, P.Slot, P.Itab, computeType(P.Itab, P.Anchor));
      }

      // Add all mappings to the result list
      for (auto &E : MappingMap)
        Mappings.push_back(std::move(E.second));
    }
  }
}

// Rewrite a call site that has only one possible target (direct replacement)
// This is the most aggressive optimization - completely replace indirect call
// with direct call
static bool rewriteSingleTarget(
    const ItabSlotMapping &Entry,
    const std::map<std::pair<std::string, int>, Function *> &ItabSlotToImpl) {
  auto *GV = *Entry.Itabs.cbegin();
  CallInst *CI = Entry.Call;

  // Look up the concrete implementation function
  auto Key = std::make_pair(GV->getName().str(), Entry.SlotIndex);
  auto It = ItabSlotToImpl.find(Key);
  if (It == ItabSlotToImpl.end()) {
    if (VerboseDebug) {
      dbgs() << "(maybe in non-lto): Cannot find impl function: ("
             << GV->getName() << ":" << Entry.SlotIndex << ")\n";
    }
    return false;
  }

  Function *Target = It->second;
  if (VerboseDebug) {
    dbgs() << "Find impl function: (" << GV->getName() << ":" << Entry.SlotIndex
           << ") => " << Target->getName() << "\n";
  }

  // Create the direct call replacement
  IRBuilder<> B(CI);
  SmallVector<Value *, 8> Args(CI->arg_begin(), CI->arg_end());

  auto *NewCall = B.CreateCall(Target->getFunctionType(), Target, Args);
  NewCall->setCallingConv(CI->getCallingConv());
  NewCall->setDebugLoc(CI->getDebugLoc());

  // Insert NOP for Go backend compatibility
  ++NumDirectRewrites;

  // Replace all uses of the old call with the new direct call
  if (!CI->use_empty())
    CI->replaceAllUsesWith(NewCall);

  // Clean up the anchor load instruction (no longer needed)
  auto *Load = dyn_cast<Instruction>(Entry.Anchor);
  Load->replaceAllUsesWith(UndefValue::get(Load->getType()));
  Load->setMetadata(LLVMContext::MD_dbg, nullptr);
  Load->setOperand(0, UndefValue::get(Load->getOperand(0)->getType()));
  Load->eraseFromParent();

  // Remove the original indirect call
  CI->eraseFromParent();
  return true;
}

// Rewrite a call site with multiple possible targets using conditional
// branching This creates a if-else statement based on itab pointer value to
// dispatch to direct calls
static void rewriteMultiTarget(
    Module &M, const ItabSlotMapping &Entry,
    const std::map<std::pair<std::string, int>, Function *> &ItabSlotToImpl) {
  CallInst *CI = Entry.Call;

  // Find all available implementation functions
  SmallVector<std::pair<GlobalVariable *, Function *>, 4> AvailableImplFunc;
  AvailableImplFunc.reserve(Entry.Itabs.size());
  std::transform(
      Entry.Itabs.begin(), Entry.Itabs.end(),
      std::back_inserter(AvailableImplFunc),
      [&](GlobalVariable *GV) -> std::pair<GlobalVariable *, Function *> {
        auto Key = std::make_pair(GV->getName().str(), Entry.SlotIndex);
        auto It = ItabSlotToImpl.find(Key);
        if (It != ItabSlotToImpl.end()) {
          if (VerboseDebug) {
            dbgs() << "Find impl function: (" << GV->getName() << ":"
                   << Entry.SlotIndex << ")\n";
          }
          return std::make_pair(GV, It->second);
        }
        if (VerboseDebug) {
          dbgs() << "(maybe in non-lto): Cannot find impl function: ("
                 << GV->getName() << ":" << Entry.SlotIndex << ")\n";
        }
        return std::make_pair(nullptr, nullptr);
      });

  // Remove entries where we couldn't find the implementation
  AvailableImplFunc.erase(
      std::remove_if(AvailableImplFunc.begin(), AvailableImplFunc.end(),
                     [](const std::pair<GlobalVariable *, Function *> &p) {
                       return p.first == nullptr || p.second == nullptr;
                     }),
      AvailableImplFunc.end());

  // Skip if no-candidate | available candidate > max count.
  if (AvailableImplFunc.empty() ||
      AvailableImplFunc.size() > DevirtualItemCandidateMaxCount) {
    return;
  }

  // Limit the number of cases to prevent code bloat
  ArrayRef<std::pair<GlobalVariable *, Function *>> FirstN =
      ArrayRef(AvailableImplFunc).take_front(DevirtualItemMaxCount);

  // Set up basic blocks for the if-else statement
  BasicBlock *OrigBB = CI->getParent();
  Function *Fn = OrigBB->getParent();

  BasicBlock *MergeBB =
      OrigBB->splitBasicBlock(BasicBlock::iterator(CI), "devirt.merge");

  // CI->removeFromParent();
  Instruction *OldTerm = OrigBB->getTerminator();
  if (OldTerm)
    OldTerm->eraseFromParent();

  BasicBlock *CurBB = OrigBB;
  std::vector<std::pair<BasicBlock *, Value *>> Results;

  for (auto [GV, Target] : FirstN) {
    if (Target->hasFnAttribute(llvm::Attribute::NoInline)) {
      continue;
    }
    IRBuilder<> CondBuilder(CurBB);
    Value *Cond = CondBuilder.CreateICmpEQ(
        Entry.Anchor, CondBuilder.CreateBitCast(GV, Entry.Anchor->getType()));

    BasicBlock *ThenBB =
        BasicBlock::Create(M.getContext(), "devirt.direct", Fn);
    IRBuilder<> ThenBuilder(ThenBB);
    std::vector<Value *> Args(CI->arg_begin(), CI->arg_end());
    CallInst *DirectCall = ThenBuilder.CreateCall(Target, Args);
    DirectCall->setCallingConv(CI->getCallingConv());
    DirectCall->setDebugLoc(CI->getDebugLoc());
    ThenBuilder.CreateBr(MergeBB);

    ++NumFortRewrites;

    if (!CI->getType()->isVoidTy())
      Results.emplace_back(ThenBB, DirectCall);

    BasicBlock *NextBB = BasicBlock::Create(M.getContext(), "devirt.next", Fn);
    CondBuilder.CreateCondBr(Cond, ThenBB, NextBB);
    CurBB = NextBB;
  }

  // Fallback: original indirect call
  IRBuilder<> FallbackBuilder(CurBB);
  CallInst *FallbackCall = dyn_cast<CallInst>(CI->clone());
  FallbackBuilder.Insert(FallbackCall);
  FallbackBuilder.CreateBr(MergeBB);
  if (!CI->getType()->isVoidTy())
    Results.emplace_back(CurBB, FallbackCall);

  // Merge: insert PHI if needed
  if (!CI->getType()->isVoidTy()) {
    IRBuilder<> MergeBuilder(&*MergeBB->getFirstInsertionPt());
    PHINode *PHI =
        MergeBuilder.CreatePHI(CI->getType(), Results.size(), "devirt.result");
    for (auto &[BB, Val] : Results) {
      PHI->addIncoming(Val, BB);
    }
    CI->replaceAllUsesWith(PHI);
  }

  CI->eraseFromParent();
}

/// Main function for rewriting devirtualized calls
/// Processes all identified callsites and applies appropriate optimization
/// strategy
static void rewriteDevirtualCalls(
    Module &M, const std::vector<ItabSlotMapping> &Mappings,
    const std::map<std::pair<std::string, int>, Function *> &ItabSlotToImpl) {

  if (VerboseDebug) {
    DEBUG_LOG("=== Summary of ItabSlotMappings ===");
    for (const auto &Entry : Mappings) {
      printItabSlotMapping(Entry);
    }
  }

  // First pass: Handle single-target replacements (most aggressive
  // optimization)
  for (const auto &Entry : Mappings) {
    CallInst *CI = Entry.Call;
    if (!Entry.Anchor || Entry.SlotIndex < 0 ||
        Entry.Type != DevirtualType::Replace || !CI || !CI->getParent())
      continue;
    rewriteSingleTarget(Entry, ItabSlotToImpl);
  }

  // Early exit if only single-case optimization is enabled
  if (DevirtualSingleCaseOnly) {
    return;
  }

  // Second pass: Handle multi-target cases with conditional branching
  for (const auto &Entry : Mappings) {
    CallInst *CI = Entry.Call;
    if (!Entry.Anchor || Entry.SlotIndex < 0 ||
        Entry.Type != DevirtualType::Fork || !CI || !CI->getParent())
      continue;

    rewriteMultiTarget(M, Entry, ItabSlotToImpl);
  }
}

/// Main entry point for the Go devirtualization pass
/// This is called by the LLVM pass manager
PreservedAnalyses GoDevirtualAnnotatorPass::run(Module &M,
                                                ModuleAnalysisManager &AM) {
  DEBUG_LOG("Running GoDevirtualAnnotator Pass");

  // Step 1: Detect Go version and set appropriate itab prefix
  // Different Go versions use different naming conventions for itab symbols
  if (NamedMDNode *CUNodes = M.getNamedMetadata("go.version")) {
    StringRef GoVersion =
        cast<MDString>(CUNodes->getOperand(0)->getOperand(0))->getString();
    int GoMinorVersion = 0;
    if (std::sscanf(GoVersion.data(), "go1.%d", &GoMinorVersion)) {
      GoItabPrefix = (GoMinorVersion >= 22) ? "go:itab" : "go.itab";
    }
  }

  // Step 2: Build mapping from (itab_name, slot_index) to implementation
  // function This information comes from metadata attached to itab globals
  std::map<std::pair<std::string, int>, Function *> ItabSlotToImpl;
  for (GlobalVariable &GV : M.globals()) {
    if (!checkGoItab(GV))
      continue;

    // Look for metadata containing method offset information
    unsigned kind = M.getContext().getMDKindID("go.inter.offsets");
    if (!GV.hasMetadata(kind))
      continue;

    MDNode *OffsetsMD = GV.getMetadata(kind);
    for (const auto &Op : OffsetsMD->operands()) {
      if (auto *Entry = dyn_cast<MDNode>(Op)) {
        if (Entry->getNumOperands() != 2)
          continue;

        // Extract function name and offset from metadata
        auto *FnNameMD = dyn_cast<MDString>(Entry->getOperand(0));
        auto *OffsetCI =
            mdconst::dyn_extract<ConstantInt>(Entry->getOperand(1));
        if (!FnNameMD || !OffsetCI)
          continue;

        std::string FnName = FnNameMD->getString().str();
        int Offset = OffsetCI->getSExtValue();
        Function *F = M.getFunction(FnName);
        if (!F) {
          if (VerboseDebug) {
            dbgs() << "[WARN] Cannot find Function for name: " << FnName
                   << "\n";
          }
          continue;
        }

        // Register the mapping
        std::string ItabName = GV.getName().str();
        ItabSlotToImpl[{ItabName, Offset}] = F;

        DEBUG_LOG("Registered impl via itab metadata: itab="
                  << ItabName << ", slot=" << Offset
                  << ", func=" << F->getName());
      }
    }
  }

  // Step 3: Analyze the module to find itab propagation patterns
  std::vector<ItabSlotMapping> Mappings;
  ItabMap ItabMap;
  buildItabGraph(M, ItabMap); // Build complete itab->value mapping

  // Step 4: Find interface method call sites and match with itab information
  collectItabCallsites(M, Mappings, ItabMap);

  // Step 5: Rewrite the calls using the collected information
  rewriteDevirtualCalls(M, Mappings, ItabSlotToImpl);

  if (VerboseDebug) {
    dbgs() << "\n[GoDevirt Statistics]\n";
    dbgs() << "  Calls rewritten to direct call:   " << NumDirectRewrites
           << "\n";
    dbgs() << "  Calls rewritten with branching:   " << NumFortRewrites << "\n";
    dbgs() << "\n";
  }

  return PreservedAnalyses::all();
}

} // namespace llvm