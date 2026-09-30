// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: (Apache-2.0 WITH LLVM-exception) AND BSD-3-Clause AND NCSA
//
// Copyright 2018 The Go Authors. All rights reserved.
// Copyright (c) 2025 ByteDance Ltd. and/or its affiliates.
// This file has been modified by ByteDance Ltd. and/or its affiliates on 2025-01-28.
// Statepoint rewriting is adapted from passes/GoStatepoints.cpp of gollvm
// (https://go.googlesource.com/gollvm/). getPtrBitmapForTypeHelper is
// adapted from that project's passes/Util.cpp. The historical NCSA notice below is
// retained for LLVM-derived portions of the original implementation.
// The gollvm-derived portions and their modifications retain BSD 3-Clause
// terms in third-party/gollvm/LICENSE. Original LLVM-derived portions
// retain the historical NCSA terms in LICENSE_UPSTREAM.TXT. Other TangoLLVM
// code uses Apache-2.0 WITH LLVM-exception; see LICENSE.TXT.

//===- GoStatepoints.cpp - Insert statepoints for Go GC -------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE_UPSTREAM.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Rewrite call/invoke instructions so as to record live variables on
// stack for the use of garbage collector.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/MapVector.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/SmallSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/Analysis/DomTreeUpdater.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CallingConv.h"
#include "llvm/IR/Constant.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/MDBuilder.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Statepoint.h"
#include "llvm/TangoLLVM/TangoLLVM.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/User.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/ValueHandle.h"
#include "llvm/Pass.h"
#include "llvm/InitializePasses.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/Transforms/Utils/PromoteMemToReg.h"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <set>
#include <string>
#include <utility>
#include <vector>

#define DEBUG_TYPE "go-statepoints"

using namespace llvm;

namespace llvm {

class DominatorTree;
class Function;
class Module;
class TargetTransformInfo;
class TargetLibraryInfo;


void initializeGoStatepointsLegacyPassPass(PassRegistry&);
ModulePass *createGoStatepointsLegacyPass();

struct GoStatepoints : public PassInfoMixin<GoStatepoints> {
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);

  bool runOnFunction(Function &F, DominatorTree &, LoopInfo &, TargetTransformInfo &,
                     const TargetLibraryInfo &);
};

} // namespace llvm

// STATISTIC(NumGCToZero, "Number of ToZero stores");
static int NumGCToZero = 0;

// Print the ToZero found at the insert location
static cl::opt<bool> PrintToZero("gogc-print-tozero", cl::Hidden,
                                  cl::init(false));

// Find a better place for inserting zeroing
static cl::opt<bool> OptToZeroPosition("gogc-optimize-tozero-position", cl::Hidden,
                                  cl::init(true));

// Only zeroing necessary bits
static cl::opt<bool> OptToZeroPattern("gogc-optimize-tozero-pattern", cl::Hidden,
                                  cl::init(true));

// Print the liveset found at the insert location
static cl::opt<bool> PrintLiveSet("gogc-print-liveset", cl::Hidden,
                                  cl::init(false));

// Print the liveset only for the specified function.
static cl::opt<std::string> PrintFunc("gogc-print-func", cl::Hidden,
                                      cl::init(""));

// At each statepoint, clobber all the stack slots that are considered
// dead, for debugging purposes.
static cl::opt<bool> ClobberNonLive("gogc-clobber-non-live",
                                    cl::Hidden, cl::init(false));

// Check bitcast of Alloca
static cl::opt<bool> VerifyAllocaCast("gogc-verify-alloca-cast", cl::Hidden,
                                  cl::init(false));

// Move GEP after some statepoints
static cl::opt<bool> MoveGEPBackward("gogc-move-gep-backward", cl::Hidden,
                                  cl::init(true));

// Move ITP to Def
static cl::opt<bool> MoveITPToDef("gogc-move-itp-to-def", cl::Hidden,
                                  cl::init(true));

// Move PTI to Use
static cl::opt<bool> MovePTIToUse("gogc-move-pti-to-use", cl::Hidden,
                                  cl::init(false));

static cl::opt<bool> RehoistGEP("gogc-rehoist-gep", cl::Hidden,
                                  cl::init(true));

static cl::opt<bool> PrintMoved("gogc-print-moved", cl::Hidden,
                                  cl::init(false));

// Print the liveset found at the insert location
static cl::opt<bool> PrintObsoleted("gogc-print-obsoleted", cl::Hidden,
                                  cl::init(false));

// Check store of ill-formed pointers
static cl::opt<bool> CheckIllFormedPointerStore("gogc-check-ifp-store", cl::Hidden,
                                  cl::init(false));


static cl::opt<std::string>
    ReHoistDebugFunc("debug-re-hoist-func", cl::Hidden, cl::init(""),
               cl::desc("print analysis and move progress"));

// Statepoint ID. TODO: this is not thread safe.
static uint64_t ID = 0;

/// The IR fed into this pass may have had attributes and
/// metadata implying dereferenceability that are no longer valid/correct after
/// this pass has run. This is because semantically, after
/// this pass runs, all calls to gc.statepoint "free" the entire
/// heap. stripNonValidData (conservatively) restores
/// correctness by erasing all attributes in the module that externally imply
/// dereferenceability. Similar reasoning also applies to the noalias
/// attributes and metadata. gc.statepoint can touch the entire heap including
/// noalias objects.
/// Apart from attributes and metadata, we also remove instructions that imply
/// constant physical memory: llvm.invariant.start.
//
// TODO: revisit this. For a non-moving GC some attributes may still be valid.
// It probably doesn't really matter, as we run this pass at the end of
// optimization pipeline.
static void stripNonValidData(Module &M);

static bool shouldRewriteStatepointsIn(Function &F);

static bool needsRewriteForGo(CallBase *Call);

static bool isGoStackAddr(Value * V);
static bool isGoPreAllocatedParamOut(Value * V);
static bool isGoReadRegister(Value * V);
static bool isGoPreAllocated(Value * V);
static bool hasStackMap(const CallBase *Call);
static bool callsInlineAsm(const CallBase* Call);

static bool isGoConstantPointer(Value *V) {
  return isGoReadRegister(V) && isGoStackAddr(V);
}

static bool isConstant(Value *V) {
  if (CastInst *CI = dyn_cast<CastInst>(V)) {
    return isConstant(CI->getOperand(0));
  }
  return isa<Constant>(V);
}

// Those value are not part of data flow analysis since
// they are constants.
static bool isGoConstantLikeValue(Value *V) {
  if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(V)) {
    return isa<GlobalVariable>(GEP->stripPointerCasts());
  }
  return isConstant(V) || isa<InlineAsm>(V)
          || isGoConstantPointer(V);
}

static bool isGoGCPointerOrPointerLikeValue(Value *V);

static bool isGoPreAllocatedParamOut(Value * V) {
  return TangoLLVM::isGoPreAllocatedParamOut(V);
}

static bool isStackConstantPointer(Value *V) {
  if (isa<AllocaInst>(V))
    return true;
  if (isGoPreAllocated(V))
    return true;

  if (BitCastInst *BC = dyn_cast<BitCastInst>(V))
    return isStackConstantPointer(BC->getOperand(0));

  if (IntToPtrInst *ITP = dyn_cast<IntToPtrInst>(V))
    return isStackConstantPointer(ITP->getOperand(0));

  if (PtrToIntInst *PTI = dyn_cast<PtrToIntInst>(V))
    return isStackConstantPointer(PTI->getOperand(0));

  if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(V))
    if (isStackConstantPointer(GEP->getOperand(0))) {
      if (GEP->hasAllConstantIndices())
        return true;

      return false;
    }

  return false;
}

/**
 * Get the element type of a Value in LLVM IR with opaque pointer support.
 * This function handles different instructions and attempts to determine the actual
 * element type that the Value is working with.
 *
 * @param V The Value to analyze
 * @return The element Type, or the original type if cannot determine
 */
static Type* getValueElementType(Value *V) {
  // Handle different instruction types
  if (auto I = dyn_cast<Instruction>(V)) {
    if (Type *Ty = TangoLLVM::getGoPreAllocatedElementType(V)) {
      assert(isGoPreAllocated(V) && "only go.preallocated has go type");
      return Ty;
    }

    // For alloca, return the allocated type
    if (auto AI = dyn_cast<AllocaInst>(I)) {
      return AI->getAllocatedType();
    }

    // For GEP, return the result element type
    if (auto GEP = dyn_cast<GetElementPtrInst>(I)) {
      return GEP->getResultElementType();
    }
  }

  dbgs() << "cannot determine result type of " << *V << "\n";
  llvm_unreachable("cannot determine element type");
}

// Intrinsics don't work with statepoints. Convert them to calls to make sure that
// their stackmaps will be generated. They may be treated as an intrinsic in DAG
// later.
static bool convertIntrinsicsToCalls(Function &F) {
  bool Changed = false;
  SmallVector<IntrinsicInst*, 4> Intrinsics;
  SmallVector<IntrinsicInst*, 4> LifetimeIntrinsics;
  SmallVector<MemCpyInst*, 8> MemCpy;
  for (Instruction &I : instructions(F)) {
    if (IntrinsicInst *II = dyn_cast<IntrinsicInst>(&I)) {
      switch (II->getIntrinsicID()) {
        case Intrinsic::lifetime_start:
        case Intrinsic::lifetime_end:
          LifetimeIntrinsics.push_back(II);
          break;
        case Intrinsic::memmove:
          Intrinsics.push_back(II);
          break;
        case Intrinsic::memcpy: {
          MemCpyInst *M = dyn_cast<MemCpyInst>(&I);
          assert(M && "must be MemCpyInst");
          if (isa<ConstantInt>(M->getLength()))
            MemCpy.push_back(M);
          else
            Intrinsics.push_back(II);
          break;
        }
        case Intrinsic::memcpy_inline:
          // Do nothing.
          // The only call case is duffcopy. Go compiler doesn't
          // generate a stackmap either.
          break;
        case Intrinsic::memset: {
          if (auto *C = dyn_cast<Constant>(II->getArgOperand(1))) {
            if (!C->isZeroValue()) {
              llvm_unreachable("Only memclr is available in Golang.");
            }
          }
          // A memset intrinsic with a constant size: 1) comes from
          // OpZero; 2) can be correctly lowered in DAG later.
          // Convert the others to calls.
          if (!isa<Constant>(II->getArgOperand(2))) {
            Intrinsics.push_back(II);
          }
          break;
        }
        default:
          // Do nothing.
          break;
      }
    }
  }

  if (!LifetimeIntrinsics.empty()) {
    Changed = true;
    for (auto *II : LifetimeIntrinsics) {
      II->eraseFromParent();
    }
  }

  if (!MemCpy.empty()) {
    Changed = true;
    for (auto *M : MemCpy) {
      IRBuilder<> Builder(M);
      Instruction *NewM = Builder.CreateMemCpyInline(
          M->getRawDest(), M->getDestAlign(), M->getRawSource(),
          M->getSourceAlign(), M->getLength(), M->isVolatile());
      if (!M->use_empty())
        M->replaceAllUsesWith(NewM);
      M->eraseFromParent();
    }
  }

  if (!Intrinsics.empty()) {
    for (IntrinsicInst *II : Intrinsics) {
      TangoLLVM::lowerIntrinsic(II);
    }
    return true;
  }

  return Changed;
}

PreservedAnalyses GoStatepoints::run(Module &M,
                                     ModuleAnalysisManager &AM) {

  bool Changed = false;
  auto &FAM = AM.getResult<FunctionAnalysisManagerModuleProxy>(M).getManager();
  for (Function &F : M) {
    // Nothing to do for declarations.
    if (F.isDeclaration() || F.empty())
      continue;

    // Policy choice says not to rewrite - the most common reason is that we're
    // compiling code without a GCStrategy.
    if (!shouldRewriteStatepointsIn(F))
      continue;

    auto &DT = FAM.getResult<DominatorTreeAnalysis>(F);
    auto &LI = FAM.getResult<LoopAnalysis>(F);
    auto &TTI = FAM.getResult<TargetIRAnalysis>(F);
    auto &TLI = FAM.getResult<TargetLibraryAnalysis>(F);
    Changed |= runOnFunction(F, DT, LI, TTI, TLI);
  }
  if (!Changed)
    return PreservedAnalyses::all();

  // stripNonValidData asserts that shouldRewriteStatepointsIn
  // returns true for at least one function in the module.  Since at least
  // one function changed, we know that the precondition is satisfied.
  stripNonValidData(M);

  PreservedAnalyses PA;
  PA.preserve<TargetIRAnalysis>();
  PA.preserve<TargetLibraryAnalysis>();
  return PA;
}


// Utils
static bool hasPointerTy(Type *T) {
  return TangoLLVM::hasPointerTy(T);
}

static bool ignorePointer(Type *T, unsigned Idx) {
  StructType *ST = dyn_cast<StructType>(T);
  if (!ST)
    return false;
  return Idx == 0 && ST->hasName() && ST->getName().starts_with("interface$");
}

static void
getPtrBitmapForTypeHelper(Type *T, const DataLayout &DL, uint64_t BaseOffset, BitVector &BV) {
  if (!hasPointerTy(T))
    return;

  const unsigned PtrSize = DL.getPointerSize();
  Type *Int32Ty = Type::getInt32Ty(T->getContext());
  switch (T->getTypeID()) {
  case Type::PointerTyID:
    BV.set(BaseOffset / PtrSize);
    break;;
  case Type::ArrayTyID: {
    Type *ET = T->getArrayElementType();
    for (unsigned i = 0, n = T->getArrayNumElements(); i < n; ++i) {
      Value *ivals[2] = { ConstantInt::get(Int32Ty, 0),
                          ConstantInt::get(Int32Ty, i) };
      ArrayRef<Value*> Idx = ArrayRef(ivals, 2);
      uint64_t Offset = DL.getIndexedOffsetInType(T, Idx);
      getPtrBitmapForTypeHelper(ET, DL, BaseOffset+Offset, BV);
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
      getPtrBitmapForTypeHelper(ET, DL, BaseOffset+Offset, BV);
    }
    break;
  }
  case Type::StructTyID: {
    for (unsigned i = 0, n = T->getStructNumElements(); i < n; ++i) {
      Type *ET = T->getStructElementType(i);
      if (!hasPointerTy(ET) || ignorePointer(T, i))
        continue;
      Value *ivals[2] = { ConstantInt::get(Int32Ty, 0),
                          ConstantInt::get(Int32Ty, i) };
      ArrayRef<Value*> Idx = ArrayRef(ivals, 2);
      uint64_t Offset = DL.getIndexedOffsetInType(T, Idx);
      getPtrBitmapForTypeHelper(ET, DL, BaseOffset+Offset, BV);
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

// Compute the pointer bitmap for type T, stored into Words.
void
getPtrBitmapForType(Type *T, const DataLayout &DL,
                                    SmallVectorImpl<Value *> &Words) {
  // TODO: this function is silly -- BitVector internally has
  // a bitmap storage, but it is private. Can we do better?
  const unsigned PtrSize = DL.getPointerSize();
  Type *Int32Ty = Type::getInt32Ty(T->getContext());
  uint64_t Size = DL.getTypeStoreSize(T);
  BitVector BV(Size/PtrSize);

  getPtrBitmapForTypeHelper(T, DL, 0, BV);

  if (BV.none())
    return;
  unsigned last = BV.find_last();
  if (last == 0) // a single pointer field, no need of a bitmap
    return;
  //Words.reserve(last/32 + 1);
  for (unsigned i = 0; i <= last; i += 32) {
    uint32_t w = 0;
    for (unsigned j = 0; j < 32 && i+j <= last; j++)
      w |= BV[i+j] ? 1<<j : 0;
    Words.push_back(ConstantInt::get(Int32Ty, w));
  }
}


namespace {

class GoStatepointsLegacyPass : public ModulePass {
  GoStatepoints Impl;

public:
  static char ID; // Pass identification, replacement for typeid

  GoStatepointsLegacyPass() : ModulePass(ID), Impl() {
    initializeGoStatepointsLegacyPassPass(
        *PassRegistry::getPassRegistry());
  }

  bool runOnModule(Module &M) override {
    bool Changed = false;
    for (Function &F : M) {
      // Nothing to do for declarations.
      if (F.isDeclaration() || F.empty())
        continue;

      // Policy choice says not to rewrite - the most common reason is that
      // we're compiling code without a GCStrategy.
      if (!shouldRewriteStatepointsIn(F))
        continue;

      const TargetLibraryInfo &TLI =
          getAnalysis<TargetLibraryInfoWrapperPass>().getTLI(F);

      TargetTransformInfo &TTI =
          getAnalysis<TargetTransformInfoWrapperPass>().getTTI(F);
      auto &DT = getAnalysis<DominatorTreeWrapperPass>(F).getDomTree();
      auto &LI = getAnalysis<LoopInfoWrapperPass>(F).getLoopInfo();
      Changed |= Impl.runOnFunction(F, DT, LI, TTI, TLI);
    }

    if (!Changed)
      return false;

    if (PrintToZero)
      dbgs() << "NumGCToZero: " << M.getName() << " " << NumGCToZero << "\n";

    // stripNonValidData asserts that shouldRewriteStatepointsIn
    // returns true for at least one function in the module.  Since at least
    // one function changed, we know that the precondition is satisfied.
    stripNonValidData(M);
    return true;
  }

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    // We add and rewrite a bunch of instructions, but don't really do much
    // else.  We could in theory preserve a lot more analyses here.
    AU.addRequired<DominatorTreeWrapperPass>();
    AU.addRequired<LoopInfoWrapperPass>();
    AU.addRequired<TargetTransformInfoWrapperPass>();
    AU.addRequired<TargetLibraryInfoWrapperPass>();
  }
};

} // end anonymous namespace

char GoStatepointsLegacyPass::ID = 0;

ModulePass *llvm::createGoStatepointsLegacyPass() {
  return new GoStatepointsLegacyPass();
}

INITIALIZE_PASS_BEGIN(GoStatepointsLegacyPass,
                      "go-statepoints",
                      "Insert statepoints for Go GC", false, false)
INITIALIZE_PASS_DEPENDENCY(DominatorTreeWrapperPass)
INITIALIZE_PASS_DEPENDENCY(LoopInfoWrapperPass)
INITIALIZE_PASS_DEPENDENCY(TargetTransformInfoWrapperPass)
INITIALIZE_PASS_END(GoStatepointsLegacyPass,
                    "go-statepoints",
                    "Insert statepoints for Go GC", false, false)

namespace {

struct ToZeroInfo {
  BitVector    State;
  BasicBlock  *Pos;
};

// Liveness tracking has three parts: in-register values, non-address-taken
// allocas (stack slots), and address-taken allocas.
//
// In-register values are live since it is defined, until it has no more
// use. At statepoints they are spilled so the runtime can find them on
// the stack.
//
// Non-address-taken allocas are live since it is initialized, until it
// has no more use. At statepoints they are not spilled but its stack
// location is recorded.
//
// Address-taken allocas are live since it is initialized and remain live
// thereafter, unless an explicit lifetime.end is seen. As above, they
// are not spilled at statepoints but has its stack location recorded.
//
// In the data structure, there are some overlap. The live sets are used
// for both in-register values and non-address-taken allocas, but not
// for address-taken allocas. The alloca def/kill sets are used for both
// kinds of allocas.
struct GCPtrLivenessData {
  // In-register value and non-address-taken alloca.

  /// Values defined in this block.
  MapVector<BasicBlock *, SetVector<Value *>> KillSet;
  /// Values used in this block (and thus live); does not included values
  /// killed within this block.
  MapVector<BasicBlock *, SetVector<Value *>> LiveSet;
  /// Values live into this basic block (i.e. used by any
  /// instruction in this basic block or ones reachable from here)
  MapVector<BasicBlock *, SetVector<Value *>> LiveIn;
  /// Values live out of this basic block (i.e. live into
  /// any successor block)
  MapVector<BasicBlock *, SetVector<Value *>> LiveOut;

  // On-Stack PARAMOUT
  SetVector<Value*> ParamOut;
  SetVector<Value*> AlwaysLive;

  // Alloca liveness.

  DenseMap<Type*, BitVector> TypeToPtrMap;
  DenseMap<BasicBlock *, DenseMap<Value *, BitVector>> InAllocaDefStates; // the must initialized bits at the begin of BB
  DenseMap<BasicBlock *, DenseMap<Value *, BitVector>> OutAllocaDefStates; // the must initialized bits at the end of BB

  // Used by findLiveSetAtInst
  MapVector<Instruction*, SetVector<Value *>> GenAtInst;  // recorded used pointers
  MapVector<Instruction*, SetVector<Value *>> LiveAtInst; // Livein for each instruction
};

// The type of the internal cache used inside the findBasePointers family
// of functions.  From the callers perspective, this is an opaque type and
// should not be inspected.
//
// In the actual implementation this caches two relations:
// - The base relation itself (i.e. this pointer is based on that one)
// - The base defining value relation (i.e. before base_phi insertion)
// Generally, after the execution of a full findBasePointer call, only the
// base relation will remain.  Internally, we add a mixture of the two
// types, then update all the second type to the first type
using DefiningValueMapTy = MapVector<Value *, Value *>;
using StatepointLiveSetTy = SetVector<Value *>;
using RematerializedValueMapTy =
    MapVector<AssertingVH<Instruction>, AssertingVH<Value>>;

struct PartiallyConstructedSafepointRecord {
  /// The set of values known to be live across this safepoint
  StatepointLiveSetTy LiveSet;

  /// Mapping from live pointers to a base-defining-value
  MapVector<Value *, Value *> PointerToBase;

  /// The *new* gc.statepoint instruction itself.  This produces the token
  /// that normal path gc.relocates and the gc.result are tied to.
  GCStatepointInst *StatepointToken;

  /// Instruction to which exceptional gc relocates are attached
  /// Makes it easier to iterate through them during relocationViaAlloca.
  Instruction *UnwindToken;

  /// Record live values we are rematerialized instead of relocating.
  /// They are not included into 'LiveSet' field.
  /// Maps rematerialized copy to it's original value.
  RematerializedValueMapTy RematerializedValues;
};

} // end anonymous namespace

/// Compute the live-in set for every basic block in the function
static void computeLiveInValues(DominatorTree &DT, LoopInfo &LI, Function &F,
                                GCPtrLivenessData &Data,
                                DenseMap<Value *, ToZeroInfo> &ToZero,
                                DefiningValueMapTy &DVCache);

/// Given results from the dataflow liveness computation, find the set of live
/// Values at a particular instruction.
static void findLiveSetAtInst(Instruction *inst, GCPtrLivenessData &Data,
                              StatepointLiveSetTy &out,
                              SetVector<Value *> &AllAllocas,
                              DefiningValueMapTy &DVCache);

// TODO: Once we can get to the GCStrategy, this becomes
// Optional<bool> isGCManagedPointer(const Type *Ty) const override {

static bool isGCPointerType(Type *T) {
  return isa<PointerType>(T);
}

// Return true if this type is one which a) is a gc pointer or contains a GC
// pointer and b) is of a type this code expects to encounter as a live value.
// (The insertion code will assert that a type which matches (a) and not (b)
// is not encountered.)
static bool isHandledGCPointerType(Type *T) {
  // We fully support gc pointers
  if (isGCPointerType(T))
    return true;
  // We partially support vectors of gc pointers. The code will assert if it
  // can't handle something.
  if (auto VT = dyn_cast<VectorType>(T))
    if (isGCPointerType(VT->getElementType()))
      return true;
  // FCA is supported.
  if (T->isStructTy() || T->isArrayTy())
    return hasPointerTy(T);
  return false;
}

static bool isGoGCPointerLikeValue(Value *V) {
  if (isa<PtrToIntInst>(V))
    return true;

  if (isa<Instruction>(V) &&
      cast<Instruction>(V)->hasMetadata("go.pointerlike"))
    return true;

  return false;
}

// We need to track all Go GC pointer like value
static bool isGoGCPointerOrPointerLikeValue(Value *V) {
  if (isGoConstantLikeValue(V))
    return false; // no track of constant value

  if (isHandledGCPointerType(V->getType()))
    return true; // it is a value of GC type

  if (isa<PtrToIntInst>(V))
    return true;

  if (isa<Instruction>(V) &&
      cast<Instruction>(V)->hasMetadata("go.pointerlike"))
    return true;

  return false;
}

#ifndef NDEBUG
/// Returns true if this type contains a gc pointer whether we know how to
/// handle that type or not.
static bool containsGCPtrType(Type *Ty) {
  if (isGCPointerType(Ty))
    return true;
  if (VectorType *VT = dyn_cast<VectorType>(Ty))
    return isGCPointerType(VT->getScalarType());
  if (ArrayType *AT = dyn_cast<ArrayType>(Ty))
    return containsGCPtrType(AT->getElementType());
  if (StructType *ST = dyn_cast<StructType>(Ty))
    return llvm::any_of(ST->subtypes(), containsGCPtrType);
  return false;
}

// Returns true if this is a type which a) is a gc pointer or contains a GC
// pointer and b) is of a type which the code doesn't expect (i.e. first class
// aggregates).  Used to trip assertions.
static bool isUnhandledGCPointerType(Type *Ty) {
  return containsGCPtrType(Ty) && !isHandledGCPointerType(Ty);
}
#endif

// Helper function to print a live set, for debugging.
static void
printLiveSet(SetVector<Value *> &LiveSet) {
  for (Value *V : LiveSet)
    dbgs() << "\t" << *V << "\n";
}

// Conservatively identifies any definitions which might be live at the
// given instruction. The  analysis is performed immediately before the
// given instruction. Values defined by that instruction are not considered
// live.  Values used by that instruction are considered live.
static void
analyzeParsePointLiveness(DominatorTree &DT,
                          GCPtrLivenessData &OriginalLivenessData,
                          CallBase *Call,
                          PartiallyConstructedSafepointRecord &Result,
                          SetVector<Value *> &AllAllocas,
                          DefiningValueMapTy &DVCache) {
  StatepointLiveSetTy LiveSet;
  findLiveSetAtInst(Call, OriginalLivenessData, LiveSet, AllAllocas, DVCache);

  if (PrintLiveSet) {
    dbgs() << "Live Variables at " << *Call << ":\n";
    printLiveSet(LiveSet);
  }
  Result.LiveSet = LiveSet;
}


static Value* findBaseDefiningValue(Value *I);

/// Return a base defining value for the 'Index' element of the given vector
/// instruction 'I'.  If Index is null, returns a BDV for the entire vector
/// 'I'.  As an optimization, this method will try to determine when the
/// element is known to already be a base pointer.  If this can be established,
/// the second value in the returned pair will be true.  Note that either a
/// vector or a pointer typed value can be returned.  For the former, the
/// vector returned is a BDV (and possibly a base) of the entire vector 'I'.
/// If the later, the return pointer is a BDV (or possibly a base) for the
/// particular element in 'I'.
static Value*
findBaseDefiningValueOfVector(Value *I) {
  // Each case parallels findBaseDefiningValue below, see that code for
  // detailed motivation.

  if (isa<Argument>(I))
    // An incoming argument to the function is a base pointer
    return I;

  if (isa<Constant>(I))
    // Base of constant vector consists only of constant null pointers.
    // For reasoning see similar case inside 'findBaseDefiningValue' function.
    return ConstantAggregateZero::get(I->getType());

  if (isa<LoadInst>(I))
    return I;

  if (isa<InsertElementInst>(I))
    // We don't know whether this vector contains entirely base pointers or
    // not.  To be conservatively correct, we treat it as a BDV and will
    // duplicate code as needed to construct a parallel vector of bases.
    return I;

  if (auto *FI = dyn_cast<FreezeInst>(I))
    return findBaseDefiningValueOfVector(FI->getOperand(0));

  if (isa<ShuffleVectorInst>(I))
    return I;

  // The behavior of getelementptr instructions is the same for vector and
  // non-vector data types.
  if (auto *GEP = dyn_cast<GetElementPtrInst>(I))
    return findBaseDefiningValue(GEP->getPointerOperand());

  // If the pointer comes through a bitcast of a vector of pointers to
  // a vector of another type of pointer, then look through the bitcast
  if (auto *BC = dyn_cast<BitCastInst>(I))
    return findBaseDefiningValue(BC->getOperand(0));

  // We assume that functions in the source language only return base
  // pointers.  This should probably be generalized via attributes to support
  // both source language and internal functions.
  if (isa<CallInst>(I) || isa<InvokeInst>(I) || isa<IntToPtrInst>(I))
    return I;

  // A PHI or Select is a base defining value.  The outer findBasePointer
  // algorithm is responsible for constructing a base value for this BDV.
  assert((isa<SelectInst>(I) || isa<PHINode>(I)) &&
         "unknown vector instruction - no base found for vector element");
  return I;
}

/// Helper function for findBasePointer - Will return a value which either a)
/// defines the base pointer for the input, b) blocks the simple search
/// (i.e. a PHI or Select of two derived pointers), or c) involves a change
/// from pointer to vector type or back.
static Value* findBaseDefiningValue(Value *I) {
  if (I->getType()->isStructTy())
    // Assuming FCA is always base.
    // FCAs appear mostly in the call sequnce where we pass/return multiple
    // values in registers, e.g. { i8*, i64 }. If it contains the address of
    // an alloca, the alloca should already be address taken (at least when
    // creating the FCA), so we don't need to link the FCA back to the alloca.
    // It is also unlikely to contain past-the-end pointer (we cannot do
    // pointer arithmetic directly with FCA). So it is safe to treat FCA as
    // base.
    return I;

  if (I->getType()->isVectorTy())
    return findBaseDefiningValueOfVector(I);

  if (isa<Argument>(I))
    // An incoming argument to the function is a base pointer
    // We should have never reached here if this argument isn't an gc value
    return I;

  if (isa<Constant>(I)) {
    // We assume that objects with a constant base (e.g. a global) can't move
    // and don't need to be reported to the collector because they are always
    // live. Besides global references, all kinds of constants (e.g. undef,
    // constant expressions, null pointers) can be introduced by the inliner or
    // the optimizer, especially on dynamically dead paths.
    // Here we treat all of them as having single null base. By doing this we
    // trying to avoid problems reporting various conflicts in a form of
    // "phi (const1, const2)" or "phi (const, regular gc ptr)".
    // See constant.ll file for relevant test cases.
    return ConstantPointerNull::get(cast<PointerType>(I->getType()));
  }

  if (CastInst *CI = dyn_cast<CastInst>(I)) {
    Value *Def = CI->stripPointerCasts();
    // "%1 = inttoptr (ptrtoint ptr %alloca to i64) to ptr"
    // points to the alloca base and will be converted to StackMaps::Direct
    // which lacks the pointer bitmap, which will cause some ptr fields of
    // alloca not to be marked by GC. Currently we only support this simplest
    // situation, need to extended in the future
    if (IntToPtrInst* ITP = dyn_cast<IntToPtrInst>(Def)) {
      if (PtrToIntInst *PTI = dyn_cast<PtrToIntInst>(ITP->getOperand(0)))
        return findBaseDefiningValue(PTI->getOperand(0));
      return Def;
    } else if (PtrToIntInst* PTI = dyn_cast<PtrToIntInst>(Def)) {
      return findBaseDefiningValue(PTI->getOperand(0));
    }

    // Pointer-to-pointer and int-to-pointer casts are handled above.
    // We don't know how to handle other type of casts.
    assert(!isa<CastInst>(Def) && "shouldn't find another cast here");
    return findBaseDefiningValue(Def);
  }

  if (isa<AllocaInst>(I))
    // alloca is a gc base
    return I;

  if (isa<LoadInst>(I))
    // The value loaded is a gc base itself
    return I;

  if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(I)) {
    // If the base is Alloca, we can always return the Alloca as the base.
    // The GEP must be within bounds since the address of an Alloca is determined
    // during layout frames.
    // Later in MIR, we will clone each Alloca in MIR (e.g., LEA64r FrameIndex) after
    // each state point before each use.
    Value* PtrBase = GEP->getPointerOperand();
    PtrBase = PtrBase->stripPointerCasts();
    if (isa<AllocaInst>(PtrBase) || isGoPreAllocated(PtrBase))
      return findBaseDefiningValue(PtrBase);

    if (isa<GetElementPtrInst>(PtrBase)) {
      Value *Base = findBaseDefiningValue(PtrBase);
      if (isa<AllocaInst>(Base) || isGoPreAllocated(Base))
        return Base;
    }

    // The base of this GEP is the base if all indices are constant. Otherwise
    // we cannot simply return the base pointer; instead, we have to treat
    // the GEP as a new base value
    if (GEP->hasAllConstantIndices())
      return findBaseDefiningValue(GEP->getPointerOperand());
    else
      return I;
  }

  if (IntrinsicInst *II = dyn_cast<IntrinsicInst>(I)) {
    switch (II->getIntrinsicID()) {
    default:
      // fall through to general call handling
      break;
    case Intrinsic::experimental_gc_statepoint:
      llvm_unreachable("statepoints don't produce pointers");
    case Intrinsic::experimental_gc_relocate:
      // Rerunning safepoint insertion after safepoints are already
      // inserted is not supported.  It could probably be made to work,
      // but why are you doing this?  There's no good reason.
      llvm_unreachable("repeat safepoint insertion is not supported");
    case Intrinsic::gcroot:
      // Currently, this mechanism hasn't been extended to work with gcroot.
      // There's no reason it couldn't be, but I haven't thought about the
      // implications much.
      llvm_unreachable(
          "interaction with the gcroot mechanism is not supported");
    }
  }
  // We assume that functions in the source language only return base
  // pointers.  This should probably be generalized via attributes to support
  // both source language and internal functions.
  if (isa<CallInst>(I) || isa<InvokeInst>(I))
    return I;

  // TODO: I have absolutely no idea how to implement this part yet.  It's not
  // necessarily hard, I just haven't really looked at it yet.
  assert(!isa<LandingPadInst>(I) && "Landing Pad is unimplemented");

  if (isa<AtomicCmpXchgInst>(I))
    // A CAS is effectively a atomic store and load combined under a
    // predicate.  From the perspective of base pointers, we just treat it
    // like a load.
    return I;

  assert(!isa<AtomicRMWInst>(I) && "Xchg handled above, all others are "
                                   "binary ops which don't apply to pointers");

  // The aggregate ops.  Aggregates can either be in the heap or on the
  // stack, but in either case, this is simply a field load.  As a result,
  // this is a defining definition of the base just like a load is.
  if (isa<ExtractValueInst>(I))
    return I;

  // We should never see an insert vector since that would require we be
  // tracing back a struct value not a pointer value.
  assert(!isa<InsertValueInst>(I) &&
         "Base pointer for a struct is meaningless");

  // An extractelement produces a base result exactly when it's input does.
  // We may need to insert a parallel instruction to extract the appropriate
  // element out of the base vector corresponding to the input. Given this,
  // it's analogous to the phi and select case even though it's not a merge.
  if (isa<ExtractElementInst>(I))
    // Note: There a lot of obvious peephole cases here.  This are deliberately
    // handled after the main base pointer inference algorithm to make writing
    // test cases to exercise that code easier.
    return I;

  if (auto *FI = dyn_cast<FreezeInst>(I)) {
    auto *V = FI->getOperand(0);
    return findBaseDefiningValue(V);
  }

  // The last two cases here don't return a base pointer.  Instead, they
  // return a value which dynamically selects from among several base
  // derived pointers (each with it's own base potentially).  It's the job of
  // the caller to resolve these.
  assert((isa<SelectInst>(I) || isa<PHINode>(I)) &&
         "missing instruction case in findBaseDefiningValue");
  return I;
}

/// Returns the base defining value for this value.
static Value *findBaseDefiningValueCached(Value *I, DefiningValueMapTy &Cache) {
  Value *&Cached = Cache[I];
  if (!Cached) {
    Cached = findBaseDefiningValue(I);
    LLVM_DEBUG(dbgs() << "fBDV-cached: " << I->getName() << " -> "
                      << Cached->getName() << "\n");
  }
  assert(Cache[I] != nullptr);
  return Cached;
}

/// Return a base pointer for this value if known.  Otherwise, return it's
/// base defining value.
static Value *findBaseOrBDV(Value *I, DefiningValueMapTy &Cache) {
  Value *Def = findBaseDefiningValueCached(I, Cache);
  auto Found = Cache.find(Def);
  if (Found != Cache.end()) {
    // Either a base-of relation, or a self reference.  Caller must check.
    return Found->second;
  }
  // Only a BDV available
  return Def;
}


/// For a given value or instruction, figure out what base ptr its derived from.
/// For gc objects, this is simply itself.  On success, returns a value which is
/// the base pointer.  (This is reliable and can be used for relocation.)  On
/// failure, returns nullptr.
static Value *findBasePointer(Value *I, DefiningValueMapTy &Cache) {
  // We now track pointer-like values.
  // Those values do not have a base.
  // We only care about the source of the root PtrToInt.
  if (!isHandledGCPointerType(I->getType())) {
    assert(isa<Instruction>(I) && "only instruction allowed");
    if (auto *PTI = dyn_cast<PtrToIntInst>(I))
      return findBasePointer(PTI->getOperand(0), Cache);

    MDNode *MD = cast<Instruction>(I)->getMetadata("go.pointerlike");
    if (!MD) {
      dbgs() << "try to find the base pointer for "
        << (MD ? "pointer-like values" : "non-pointer values") << *I << "\n";
      report_fatal_error("must be a pointer-like non-pointer value");
    }

    if (isa<PHINode>(I) || isa<SelectInst>(I) || isa<ShuffleVectorInst>(I)) {
      return I;
    }

    if (FreezeInst *FI = dyn_cast<FreezeInst>(I))
      return findBasePointer(FI->getOperand(0), Cache);

    if (auto *BI = dyn_cast<BinaryOperator>(I)) {
      if (isGoGCPointerLikeValue(BI->getOperand(0))) {
        return BI->getOperand(0);
      }
      if (isGoGCPointerLikeValue(BI->getOperand(1))) {
        return BI->getOperand(1);
      }
      return BI;
    }

    return I;
  }

  Value *Def = findBaseOrBDV(I, Cache);
  return Def;
}

// For a set of live pointers (base and/or derived), identify the base
// pointer of the object which they are derived from.  This routine will
// mutate the IR graph as needed to make the 'base' pointer live at the
// definition site of 'derived'.  This ensures that any use of 'derived' can
// also use 'base'.  This may involve the insertion of a number of
// additional PHI nodes.
//
// preconditions: live is a set of pointer type Values
//
// side effects: may insert PHI nodes into the existing CFG, will preserve
// CFG, will not remove or mutate any existing nodes
//
// post condition: PointerToBase contains one (derived, base) pair for every
// pointer in live.  Note that derived can be equal to base if the original
// pointer was a base pointer.
static void
findBasePointers(const StatepointLiveSetTy &live,
                 MapVector<Value *, Value *> &PointerToBase,
                 DominatorTree *DT, DefiningValueMapTy &DVCache) {
  for (Value *ptr : live) {
    Value *base = findBasePointer(ptr, DVCache);
    assert(base && "failed to find base pointer");
    PointerToBase[ptr] = base;
    assert((!isa<Instruction>(base) || !isa<Instruction>(ptr) ||
            DT->dominates(cast<Instruction>(base)->getParent(),
                          cast<Instruction>(ptr)->getParent())) &&
           "The base we found better dominate the derived pointer");
  }
}

/// Find the required based pointers (and adjust the live set) for the given
/// parse point.
static void findBasePointers(DominatorTree &DT, DefiningValueMapTy &DVCache,
                             CallBase *Call,
                             PartiallyConstructedSafepointRecord &result) {
  MapVector<Value *, Value *> PointerToBase;
  findBasePointers(result.LiveSet, PointerToBase, &DT, DVCache);

  result.PointerToBase = PointerToBase;
}

// List of all function attributes which must be stripped when lowering from
// abstract machine model to physical machine model.  Essentially, these are
// all the effects a safepoint might have which we ignored in the abstract
// machine model for purposes of optimization.  We have to strip these on
// both function declarations and call sites.
static constexpr Attribute::AttrKind FnAttrsToStrip[] = {
    Attribute::ReadNone,
    Attribute::ReadOnly,
    Attribute::WriteOnly,
    Attribute::Memory,
    // llvm::MemoryEffects::argMemOnly(),
    // llvm::MemoryEffects::inaccessibleMemOnly(),
    // llvm::MemoryEffects::inaccessibleMemOrArgMemOnly(),
    Attribute::NoSync,
    Attribute::NoFree
};

// List of all parameter and return attributes which must be stripped when
// lowering from the abstract machine model.  Note that we list attributes
// here which aren't valid as return attributes, that is okay.  There are
// also some additional attributes with arguments which are handled
// explicitly and are not in this list.
static constexpr Attribute::AttrKind ParamAttrsToStrip[] =
  {Attribute::ReadNone, Attribute::ReadOnly, Attribute::WriteOnly,
   Attribute::NoAlias, Attribute::NoFree};

// Create new attribute set containing only attributes which can be transferred
// from original call to the safepoint.
static AttributeList legalizeCallAttributes(CallBase *Call, CallInst *State, LLVMContext &Ctx) {
  AttributeList AL = Call->getAttributes();
  AttributeList NewAL = State->getAttributes();
  if (AL.isEmpty())
    return NewAL;

    // Remove the readonly, readnone, and statepoint function attributes.
  AttrBuilder FnAttrs(Ctx, AL.getFnAttrs());
  for (auto Attr : FnAttrsToStrip)
    FnAttrs.removeAttribute(Attr);

  for (Attribute A : AL.getFnAttrs()) {
    if (isStatepointDirectiveAttr(A))
      FnAttrs.removeAttribute(A);
  }

  // // Just skip parameter and return attributes for now
  // return AttributeList::get(Ctx, AttributeList::FunctionIndex,
  //                           AttributeSet::get(Ctx, FnAttrs));

  NewAL = NewAL.addFnAttributes(Ctx, FnAttrs);
  // for (unsigned I : llvm::seq(State->arg_size())) {
  //   if (I >= GCStatepointInst::CallArgsBeginPos) {
  //     break;
  //   }
  //   NewAL = NewAL.addParamAttributes(
  //       Ctx, I,
  //       AttrBuilder(Ctx, NewAL.getParamAttrs(I)));
  // }

  for (unsigned I : llvm::seq(Call->arg_size()))
    NewAL = NewAL.addParamAttributes(
        Ctx, GCStatepointInst::CallArgsBeginPos + I,
        AttrBuilder(Ctx, AL.getParamAttrs(I)));

  // Just skip parameter and return attributes for now
  return NewAL;
}

namespace {

/// This struct is used to defer RAUWs and `eraseFromParent` s.  Using this
/// avoids having to worry about keeping around dangling pointers to Values.
class DeferredReplacement {
  AssertingVH<Instruction> Old;
  AssertingVH<Instruction> New;
  bool IsDeoptimize = false;

  DeferredReplacement() = default;

public:
  static DeferredReplacement createRAUW(Instruction *Old, Instruction *New) {
    assert(Old != New && Old && New &&
           "Cannot RAUW equal values or to / from null!");

    DeferredReplacement D;
    D.Old = Old;
    D.New = New;
    return D;
  }

  static DeferredReplacement createDelete(Instruction *ToErase) {
    DeferredReplacement D;
    D.Old = ToErase;
    return D;
  }

  static DeferredReplacement createDeoptimizeReplacement(Instruction *Old) {
#ifndef NDEBUG
    auto *F = cast<CallInst>(Old)->getCalledFunction();
    assert(F && F->getIntrinsicID() == Intrinsic::experimental_deoptimize &&
           "Only way to construct a deoptimize deferred replacement");
#endif
    DeferredReplacement D;
    D.Old = Old;
    D.IsDeoptimize = true;
    return D;
  }

  /// Does the task represented by this instance.
  void doReplacement() {
    Instruction *OldI = Old;
    Instruction *NewI = New;

    assert(OldI != NewI && "Disallowed at construction?!");
    assert((!IsDeoptimize || !New) &&
           "Deoptimize intrinsics are not replaced!");

    Old = nullptr;
    New = nullptr;

    if (NewI)
      OldI->replaceAllUsesWith(NewI);

    if (IsDeoptimize) {
      // Note: we've inserted instructions, so the call to llvm.deoptimize may
      // not necessarily be followed by the matching return.
      auto *RI = cast<ReturnInst>(OldI->getParent()->getTerminator());
      new UnreachableInst(RI->getContext(), RI);
      RI->eraseFromParent();
    }

    OldI->eraseFromParent();
  }
};

} // end anonymous namespace

/// A unique function which doesn't require we sort the input vector.
template <typename T> static void unique_unsorted(SmallVectorImpl<T> &Vec) {
  SmallSet<T, 8> Seen;
  Vec.erase(remove_if(Vec, [&](const T &V) { return !Seen.insert(V).second; }),
            Vec.end());
}

static bool needsToExtractPointer(Type *T) {
  return T->isStructTy() || T->isArrayTy(); // || T->isVectorTy();
}

// Extract pointer fields from an FCA.
// Note that VectorTy is not a FCA
static void
extractPointerFromFCA(Value *V, IRBuilder<> &Builder,
                      SmallVectorImpl<Value *> &PtrFields) {
  Type *T = V->getType();

  if (T->isStructTy()) {
    for (unsigned i = 0, e = T->getStructNumElements(); i < e; ++i) {
      Type *ElemT = T->getStructElementType(i);
      if (ElemT->isPointerTy() && !ignorePointer(T, i)) {
        Value *Field = Builder.CreateExtractValue(V, {i});
        PtrFields.push_back(Field);
      } else
        assert(!hasPointerTy(ElemT) && "nested FCA is not supported");
    }
  } else if (T->isArrayTy()) {
#if !defined(NDEBUG)
    Type *ElemT = T->getArrayElementType();
    assert(ElemT->isPointerTy() && "nested FCA is not supported");
#endif
    for (unsigned i = 0, e = T->getArrayNumElements(); i < e; ++i) {
      Value *Elem = Builder.CreateExtractValue(V, {i});
      PtrFields.push_back(Elem);
    }
  } else {
    assert(T->isVectorTy() && "invariant");
    assert(isa<FixedVectorType>(T) && "todo: scalable vector type is not supported");
    assert(isGoGCPointerOrPointerLikeValue(V) && "invariant");
    FixedVectorType *VT = dyn_cast<FixedVectorType>(T);
    if (VT) {
      for (unsigned i = 0, e = VT->getNumElements(); i < e; ++i) {
        Value *Elem = Builder.CreateExtractElement(V, uint64_t(i));
        PtrFields.push_back(Elem);
      }
    }
  }
}

#if !defined(NDEBUG)
static Value *phiHasConstantValue(PHINode *Phi0);
#endif

static void
makeStatepointExplicitImpl(CallBase *Call, /* to replace */
                           SmallVectorImpl<Value *> &BasePtrs,
                           SmallVectorImpl<Value *> &LiveVariables,
                           PartiallyConstructedSafepointRecord &Result,
                           std::vector<DeferredReplacement> &Replacements) {
  assert(BasePtrs.size() == LiveVariables.size());

  // Then go ahead and use the builder do actually do the inserts.  We insert
  // immediately before the previous instruction under the assumption that all
  // arguments will be available here.  We can't insert afterwards since we may
  // be replacing a terminator.
  const DataLayout &DL = Call->getModule()->getDataLayout();
  Type *PtrIntTy = Type::getInt64Ty(Call->getContext());
  assert(DL.getPointerSize() == 8 && "invariant");
  IRBuilder<> Builder(Call);

  unique_unsorted(BasePtrs);

  // For aggregate typed stack slots, attach a bitmap identifying its
  // pointer fields.
  // PtrFields contains both Base and Derived Pointers.
  // Why?
  // 1. If we only need to keep objects live, we only need to track base pointers
  // and bitmap for the type. However, we have some ongoing task that aims to record
  // all bit maps. A case that we have not supported yet is LLVM-created address taken alloca.
  // 2. The Go GC does not copy objects in heap. However, it does copy objects on stack,
  // which means we should also track all pointers, include base and derived pointers.
  SmallVector<Value *, 64> PtrFields;

  std::function<bool(Value*)> IsIllFormed;
  IsIllFormed = [&](Value *V) {
    if (Instruction *I = dyn_cast<Instruction>(V)) {
      if (I->hasMetadata("go.pointerlike.illformed"))
        return true;
      if (ExtractValueInst *EVI = dyn_cast<ExtractValueInst>(V))
        if (IsIllFormed(EVI->getAggregateOperand()))
          return true;
    }
    return false;
  };

  SmallVector<Value *, 8> IllFormedPtr;

  // Step 1: Add all base pointers
  for (Value *V : BasePtrs) {
#if !defined(NDEBUG)
    if (auto *Phi = dyn_cast<PHINode>(V))
      assert(!phiHasConstantValue(Phi) && "constant phi should not be in liveset");
#endif
    if (isa<AllocaInst>(V)) {
      Type *T = cast<AllocaInst>(V)->getAllocatedType();
      // avoid [0 x type] in `stack-map`
      assert((!hasPointerTy(T) || DL.getTypeSizeInBits(T) > 0) && "pointer types must be non-zero");
      if (hasPointerTy(T) && DL.getTypeSizeInBits(T) > 0) {
        PtrFields.push_back(V);
        getPtrBitmapForType(T, DL, PtrFields);
      }
    } else if (isGoPreAllocated(V)) {
      // TODO: the type of a preallocated is always i8* now.
      // Use the real type in the future.
      Type* T = getValueElementType(V);
      assert(T != nullptr && "must have a Go type");
      // assert(isa<PointerType>(T) && "only go.preallocated has goType");
      assert((!hasPointerTy(T) || DL.getTypeSizeInBits(T) > 0) && "pointer types must be non-zero");
      if (hasPointerTy(T) && DL.getTypeSizeInBits(T) > 0) {
        // This is the same as Alloca. Since stack pointers are always live, we have no need to spill it to stack.
        // We only need to record the pointers inside an alloca.
        PtrFields.push_back(V);
        getPtrBitmapForType(T, DL, PtrFields);
      }
    } else if (needsToExtractPointer(V->getType())) {
      // Statepoint lowering doesn't handle FCA. So we do it ourselves by
      // extracting all the pointer fields and letting the statepoint lowering
      // spill them.
      extractPointerFromFCA(V, Builder, PtrFields);
      if (IsIllFormed(V))
        IllFormedPtr.push_back(V);
    } else {
      // V is not a known pointer on stack.
      // If V is not live, we record the derived pointers instead.
      if (Result.LiveSet.count(V)) {
        PtrFields.push_back(V);
      }
    }
  }

  // Step 2: CopyStack needs to identify all pointers.
  // TODO: record all.
  // Currently we only exclude **derived** stack pointers since
  // they should not be spilled on stack since we can
  // always calculate its address through RSP. Note that all stack
  // pointers's base pointers have been recorded in Step 1.
  SmallVector<Value*, 64> NonStackLiveVars;
  const auto &PointerToBase = Result.PointerToBase;
  for (Value *V : LiveVariables) {
    Value *Base = PointerToBase.find(V)->second;
    if (V->stripPointerCasts() == Base->stripPointerCasts() && Result.LiveSet.count(Base))
      continue; // V is Base and Base has been recorded in Step 1.
    if (isStackConstantPointer(Base))
      continue; // reg alloc will not spill stack locations
    // dbgs() << "Record: " << *Base << ": " << *V << "\n";
    // Record V to support copy stack.
    // TODO: in theory we should only record those have been spilled
    // at statepoints. However, here, we currently record all. Some
    // pointers can be obtained via rematerialization. If we record them,
    // they will be spilled on stack.
    NonStackLiveVars.push_back(V);
  }

  unique_unsorted(NonStackLiveVars);
  for (Value *V : NonStackLiveVars) {
    if (needsToExtractPointer(V->getType())) {
      // Statepoint lowering doesn't handle FCA. So we do it ourselves by
      // extracting all the pointer fields and letting the statepoint lowering
      // spill them.
      extractPointerFromFCA(V, Builder, PtrFields);
      if (IsIllFormed(V))
        IllFormedPtr.push_back(V);
    } else {
      PtrFields.push_back(V);
    }
  }

  if (PrintLiveSet)  {
    dbgs() << "Dump GCArgs for function " << Call->getParent()->getParent()->getName() << "\n";
    dbgs() << "Call: " << *Call << "\n";
    for (Value *V : BasePtrs)
      dbgs() << "BasePtr: " << *V << "\n";

    for (Value *V : PtrFields)
      dbgs() << "Final GCArg: " << *V << "\n";
  }

  // Check Ill-formed pointer-like value
  for (Value *V : PtrFields) {
    Instruction *I = dyn_cast<Instruction>(V);
    if (!I)
      continue;

    Type * T = I->getType()->getScalarType();
    if (!T->isPointerTy() && T != PtrIntTy) {
      assert(I->hasMetadata("go.pointerlike.illformed") && "invariant");
      IllFormedPtr.push_back(V);
    } else {
      if (I->hasMetadata("go.pointerlike.illformed")) {
        IllFormedPtr.push_back(V);
      } else if (I->hasMetadata("go.maybe.outofbounds")) {
        // Function *F = Call->getParent()->getParent();
        // dbgs() << "Maybe out of bounds: " << *I << " in " << F->getName() << "\n";
        IllFormedPtr.push_back(V);
      }
    }
  }

  // TODO: We need to remove any ill-formed pointer like value with unsupported type
  Function *F = Call->getParent()->getParent();
  // assume that 'specialized' functions do not actually contain ill-formed pointers
  if (!IllFormedPtr.empty() && !F->hasMetadata("go.specialized") && !F->hasMetadata("go.spec.cond")) {  
    if (!TangoLLVM::isObsoleted(F)) {
      if (PrintObsoleted) {
        dbgs() << F->getName() << ": mark function obsolete due to ill-formed pointer\n";
        for(Value *V : IllFormedPtr)
          dbgs() << "    " << *V << "\n";
#if !defined(NDEBUG) || defined(LLVM_ENABLE_DUMP)
        F->dump();
#endif
      }
      TangoLLVM::markObsoleted(F);
    }
    // Now, we need to remove all ill-formed pointers
    for (auto I = PtrFields.begin(); I != PtrFields.end();) {
      if (IsIllFormed(*I))
        I = PtrFields.erase(I);
      else
        ++I;
    }
  }

  Value *CallTarget = Call->getCalledOperand();
  ArrayRef<Value *> GCArgs(PtrFields);

  uint64_t StatepointID = ID;
  ID++;
  uint32_t NumPatchBytes = 0;

  ArrayRef<Use> CallArgs(Call->arg_begin(), Call->arg_end());


  // Create the statepoint given all the arguments
  GCStatepointInst *Token = nullptr;
  if (isa<CallInst>(Call)) {
    //// We should have converted all statepoints to invoke.
    //assert(false && "statepoint is not an invoke");
    CallInst *ToReplace = cast<CallInst>(Call);
    // Note (Go specific):
    // Here we attach GCArgs actually to the "deopt arg" slots, instead of
    // the "gc arg" slots, of the statepoint. Both are recorded in the stack
    // map the same way. The difference is that "deopt arg" doesn't need
    // relocation. We're implementing non-moving GC (for now).
    FunctionCallee Callee(Call->getFunctionType(), CallTarget);
    CallInst *Call = Builder.CreateGCStatepointCall(
        StatepointID, NumPatchBytes, Callee, CallArgs, GCArgs, ArrayRef<Value*>(),
        "statepoint_token");

    Call->setCallingConv(ToReplace->getCallingConv());

    // Currently we will fail on parameter attributes and on certain
    // function attributes.  In case if we can handle this set of attributes -
    // set up function attrs directly on statepoint and return attrs later for
    // gc_result intrinsic.
    Call->setAttributes(
        legalizeCallAttributes(ToReplace, Call, Call->getContext()));

    Token = cast<GCStatepointInst>(Call);
  } else {
    llvm_unreachable("statepoint is an invoke");
  }
  assert(Token && "Should be set in one of the above branches!");

  {
    Token->setName("statepoint_token");
    if (!Call->getType()->isVoidTy() && !Call->use_empty()) {
      StringRef Name =
          Call->hasName() ? Call->getName() : "";
      CallInst *GCResult = Builder.CreateGCResult(Token, Call->getType(), Name);
      GCResult->setAttributes(
          AttributeList::get(GCResult->getContext(), AttributeList::ReturnIndex,
                             Call->getAttributes().getRetAttrs()));

      // We cannot RAUW or delete CS.getInstruction() because it could be in the
      // live set of some other safepoint, in which case that safepoint's
      // PartiallyConstructedSafepointRecord will hold a raw pointer to this
      // llvm::Instruction.  Instead, we defer the replacement and deletion to
      // after the live sets have been made explicit in the IR, and we no longer
      // have raw pointers to worry about.
      Replacements.emplace_back(
          DeferredReplacement::createRAUW(Call, GCResult));
    } else {
      Replacements.emplace_back(
          DeferredReplacement::createDelete(Call));
    }
  }

  Result.StatepointToken = Token;
}

// Replace an existing gc.statepoint with a new one and a set of gc.relocates
// which make the relocations happening at this safepoint explicit.
//
// WARNING: Does not do any fixup to adjust users of the original live
// values.  That's the callers responsibility.
static void
makeStatepointExplicit(DominatorTree &DT, CallBase *Call,
                       PartiallyConstructedSafepointRecord &Result,
                       std::vector<DeferredReplacement> &Replacements) {
  const auto &LiveSet = Result.LiveSet;
  const auto &PointerToBase = Result.PointerToBase;

  // Convert to vector for efficient cross referencing.
  SmallVector<Value *, 64> BaseVec, LiveVec;
  LiveVec.reserve(LiveSet.size());
  BaseVec.reserve(LiveSet.size());
  for (Value *L : LiveSet) {
    LiveVec.push_back(L);
    assert(PointerToBase.count(L));
    Value *Base = PointerToBase.find(L)->second;
    BaseVec.push_back(Base);
  }
  assert(LiveVec.size() == BaseVec.size());

  // Do the actual rewriting and delete the old statepoint
  makeStatepointExplicitImpl(Call, BaseVec, LiveVec, Result, Replacements);
}

static void findLiveReferences(
    Function &F, DominatorTree &DT, LoopInfo &LI, ArrayRef<CallBase *> toUpdate,
    MutableArrayRef<struct PartiallyConstructedSafepointRecord> records,
    DenseMap<Value *, ToZeroInfo> &ToZero,
    DefiningValueMapTy &DVCache) {
  GCPtrLivenessData OriginalLivenessData;

  // Find all allocas.
  SetVector<Value *> AllAllocas;
  if (ClobberNonLive)
    for (Instruction &I : F.getEntryBlock())
      if (isa<AllocaInst>(I) && hasPointerTy(getValueElementType(&I)))
        AllAllocas.insert(&I);

  computeLiveInValues(DT, LI, F, OriginalLivenessData,
                      ToZero, DVCache);
  for (size_t i = 0; i < records.size(); i++) {
    struct PartiallyConstructedSafepointRecord &info = records[i];
    analyzeParsePointLiveness(DT, OriginalLivenessData,
                              toUpdate[i], info, AllAllocas, DVCache);
  }
}


// Zero ambigously lived stack slots. We insert zeroing at lifetime
// start (or the entry block), so the GC won't see uninitialized
// content. We also insert zeroing at kill sites, to ensure the GC
// won't see a dead slot come back to life.
// We also conservatively extend the lifetime of address-taken slots,
// to prevent the slot being reused while it is still recorded live.
static void
zeroAmbiguouslyLiveSlots(Function &F, DenseMap<Value *, ToZeroInfo> &ToZero) {
  if (ToZero.empty())
    return;

  if (PrintToZero)
    dbgs() << "Print zeroing:\n";

  const DataLayout &DL = F.getParent()->getDataLayout();
  const unsigned PtrSize = DL.getPointerSize();

  if (PtrSize != 8) {
    report_fatal_error("non-64bit is not supported");
  }
  // Use 64bit integer
  Type *PtrIntTy = Type::getInt64Ty(F.getContext());
  Type *PtrPtrIntTy = PtrIntTy->getPointerTo();
  Type *IdxTy = PtrIntTy;

  for (auto &It : ToZero) {
    Instruction *SO = cast<Instruction>(It.first);
    Instruction *Pos;
    if (OptToZeroPosition == false || It.second.Pos == nullptr
        || SO->hasMetadata("go.alwayslive")) {
      Pos = SO->getNextNode();
    } else {
      Pos = It.second.Pos->getFirstNonPHI();
    }
    BitVector &State = It.second.State;
    assert(SO->getParent() == &F.getEntryBlock() && "all stack objects are allocated in entry block in Go");

    IRBuilder<> Builder(Pos);
    Value *Dst = SO;
    Type *ElemTyp = getValueElementType(SO);
    assert(ElemTyp != nullptr && "ElemType cannot be null");

    size_t BitsSet = State.count();
    size_t BitsAll = State.size();

    if (PrintToZero) {
      dbgs() << "Zeroing " << BitsSet << " pointers " << " in " << BitsAll << " slots\n";
    }

    // TODO: tuning conditions that can trigger optimizations
    if (OptToZeroPattern && BitsSet <= 4 && BitsAll - BitsSet >= 4) {
      if (Dst->getType() != PtrPtrIntTy) {
        Dst = Builder.CreateBitCast(Dst, PtrPtrIntTy);
      }
      // Use three null value for the type
      // We could combine two adjacent pointers to 16
      int I = State.find_first();
      while (I != -1) {
        Value *GEP;
        if (I == 0) {
          GEP = Dst;
        } else {
          GEP = Builder.CreateGEP(PtrIntTy, Dst, ConstantInt::get(IdxTy, I));
        }
        Value *Zero = Constant::getNullValue(PtrIntTy);
        // volatile
        auto * Zeroing = Builder.CreateStore(Zero, GEP, true);
        if (PrintToZero)
          dbgs() << "Zeroing: " << *Zeroing << " (offset=" << I * PtrSize << ") for " << I << "\n";

        I = State.find_next(I);
      }
    } else {
      Value *Zero = Constant::getNullValue(ElemTyp);
      // volatile
      auto * Zeroing = Builder.CreateStore(Zero, Dst, true);
      if (PrintToZero)
        dbgs() << "Zeroing: " << *Zeroing << "\n";
    }

    ++NumGCToZero;
    ToZero.erase(SO);
  }

#if !defined(NDEBUG) || defined(LLVM_ENABLE_DUMP)
  if (PrintToZero)
    F.dump();
#endif
  assert(ToZero.empty());
}

// Detect degenerate Phi.
// Try harder to handle mutually dependent case, like
//   a = phi(null, b)
// (in a different block)
//   b = phi(a, null)
#if !defined(NDEBUG)
static Value *
phiHasConstantValue(PHINode *Phi0) {
  Value *V = Phi0->hasConstantValue();
  if (V && isa<Constant>(V))
    return V;

  V = nullptr;

  // Visit all the Phi inputs. Discover new Phis on the go, and visit them.
  // Early exit if we see a non-constant or two different constants.
  SmallSet<PHINode *, 4> Phis, Visited;
  Phis.insert(Phi0);
  while (!Phis.empty()) {
    PHINode *Phi = *Phis.begin();
    Visited.insert(Phi);
    for (Value *Operand : Phi->incoming_values()) {
      if (PHINode *P = dyn_cast<PHINode>(Operand)) {
        if (!Visited.count(P))
          Phis.insert(P);
        continue;
      }
      if (isa<Constant>(Operand)) {
        if (V && V != Operand)
          return nullptr; // operands not same
        V = Operand;
      } else
        return nullptr; // has non-constant input
    }
    Phis.erase(Phi);
  }

  return V;
}
#endif

static bool insertParsePoints(Function &F, DominatorTree &DT, LoopInfo &LI,
                              TargetTransformInfo &TTI,
                              SmallVectorImpl<CallBase *> &ToUpdate) {
#ifndef NDEBUG
  // sanity check the input
  std::set<CallBase *> Uniqued;
  Uniqued.insert(ToUpdate.begin(), ToUpdate.end());
  assert(Uniqued.size() == ToUpdate.size() && "no duplicates please!");

  for (CallBase *Call : ToUpdate)
    assert(Call->getFunction() == &F);
#endif

  // The following computation depends on metadata.
  // Lets harvest those metadata
  TangoLLVM::harvestGoMetadata(F, {GoMetadata::AlwaysLive});

  SmallVector<PartiallyConstructedSafepointRecord, 64> Records(ToUpdate.size());

  DenseMap<Value *, ToZeroInfo> ToZero;

  // Cache the 'defining value' relation used in the computation and
  // insertion of base phis and selects.  This ensures that we don't insert
  // large numbers of duplicate base_phis.
  DefiningValueMapTy DVCache;

  SmallSet<Value*, 32> CallOpSet;
  for (Instruction &I : instructions(F)) {
    if (isa<CallInst>(I)) {
      CallOpSet.insert(cast<CallInst>(&I)->getCalledOperand());
    }
  }

  // A) Identify all gc pointers which are statically live at the given call
  // site.
  findLiveReferences(F, DT, LI, ToUpdate, Records, ToZero, DVCache);

  // B) Find the base pointers for each live pointer
  for (size_t i = 0; i < Records.size(); i++) {
    PartiallyConstructedSafepointRecord &info = Records[i];
    findBasePointers(DT, DVCache, ToUpdate[i], info);
  }

  // It is possible that non-constant live variables have a constant base.  For
  // example, a GEP with a variable offset from a global.  In this case we can
  // remove it from the liveset.  We already don't add constants to the liveset
  // because we assume they won't move at runtime and the GC doesn't need to be
  // informed about them.  The same reasoning applies if the base is constant.
  // Note that the relocation placement code relies on this filtering for
  // correctness as it expects the base to be in the liveset, which isn't true
  // if the base is constant.
  // Also make sure bad loads are not included in the liveset.
  for (auto &Info : Records)
    for (auto &BasePair : Info.PointerToBase) {
      if (isa<Constant>(BasePair.second)
          || isGoConstantPointer(BasePair.second)
          || CallOpSet.contains(BasePair.first))
        Info.LiveSet.remove(BasePair.first);
    }

  // We need this to safely RAUW and delete call or invoke return values that
  // may themselves be live over a statepoint.  For details, please see usage in
  // makeStatepointExplicitImpl.
  std::vector<DeferredReplacement> Replacements;

  // Now run through and replace the existing statepoints with new ones with
  // the live variables listed.  We do not yet update uses of the values being
  // relocated. We have references to live variables that need to
  // survive to the last iteration of this loop.  (By construction, the
  // previous statepoint can not be a live variable, thus we can and remove
  // the old statepoint calls as we go.)
  for (size_t i = 0; i < Records.size(); i++)
    makeStatepointExplicit(DT, ToUpdate[i], Records[i], Replacements);

  ToUpdate.clear(); // prevent accident use of invalid calls

  for (auto &PR : Replacements)
    PR.doReplacement();

  Replacements.clear();

  for (auto &Info : Records) {
    // These live sets may contain state Value pointers, since we replaced calls
    // with operand bundles with calls wrapped in gc.statepoint, and some of
    // those calls may have been def'ing live gc pointers.  Clear these out to
    // avoid accidentally using them.
    //
    // TODO: We should create a separate data structure that does not contain
    // these live sets, and migrate to using that data structure from this point
    // onward.
    Info.LiveSet.clear();
    Info.PointerToBase.clear();
  }

  // Do all the fixups of the original live variables to their relocated selves
  // dbgs() << "Records.size(); " << Records.size() << "\n";
  SmallVector<Value *, 128> Live;
  for (size_t i = 0; i < Records.size(); i++) {
    PartiallyConstructedSafepointRecord &Info = Records[i];

    // We can't simply save the live set from the original insertion.  One of
    // the live values might be the result of a call which needs a safepoint.
    // That Value* no longer exists and we need to use the new gc_result.
    // Thankfully, the live set is embedded in the statepoint (and updated), so
    // we just grab that.
    Live.insert(Live.end(), Info.StatepointToken->gc_args_begin(),
                Info.StatepointToken->gc_args_end());
    // dbgs() << *Info.StatepointToken  << " Live.size() " << Live.size() << "\n";
#ifndef NDEBUG
    // Do some basic sanity checks on our liveness results before performing
    // relocation.  Relocation can and will turn mistakes in liveness results
    // into non-sensical code which is must harder to debug.
    // TODO: It would be nice to test consistency as well
    assert(DT.isReachableFromEntry(Info.StatepointToken->getParent()) &&
           "statepoint must be reachable or liveness is meaningless");
    for (Value *V : Info.StatepointToken->gc_args()) {
      if (!isa<Instruction>(V))
        // Non-instruction values trivial dominate all possible uses
        continue;
      auto *LiveInst = cast<Instruction>(V);
      assert(DT.isReachableFromEntry(LiveInst->getParent()) &&
             "unreachable values should never be live");
      assert(DT.dominates(LiveInst, Info.StatepointToken) &&
             "basic SSA liveness expectation violated by liveness analysis");
    }
#endif
  }

#ifndef NDEBUG
  // sanity check
  for (auto *Ptr : Live) {
    // if (!isHandledGCPointerType(Ptr->getType())) {
      // dbgs() << " PTR " << *Ptr << " TYPE " << *Ptr->getType() << "\n";
    // }
    assert(isHandledGCPointerType(Ptr->getType()) &&
           "must be a gc pointer type");
  }

#endif

  zeroAmbiguouslyLiveSlots(F, ToZero);

  // In clobber-non-live mode, delete all lifetime markers, as the
  // inserted clobbering may be beyond the original lifetime.
  if (ClobberNonLive) {
    SetVector<Instruction *> ToDel;

    for (Instruction &I : instructions(F))
      if (CallInst *CI = dyn_cast<CallInst>(&I))
        if (Function *Fn = CI->getCalledFunction())
          if (Fn->getIntrinsicID() == Intrinsic::lifetime_start ||
              Fn->getIntrinsicID() == Intrinsic::lifetime_end)
            ToDel.insert(&I);

    for (Instruction *I : ToDel)
      I->eraseFromParent();
  }

  return !Records.empty();
}

// Handles both return values and arguments for Functions and calls.
template <typename AttrHolder>
static void RemoveNonValidAttrAtIndex(LLVMContext &Ctx, AttrHolder &AH,
                                      unsigned Index) {
  AttrBuilder R(Ctx);
  AttributeSet AS = AH.getAttributes().getAttributes(Index);
  if (AS.getDereferenceableBytes())
    R.addAttribute(Attribute::get(Ctx, Attribute::Dereferenceable,
                                  AS.getDereferenceableBytes()));
  if (AS.getDereferenceableOrNullBytes())
    R.addAttribute(Attribute::get(Ctx, Attribute::DereferenceableOrNull,
                                  AS.getDereferenceableOrNullBytes()));
  for (auto Attr : ParamAttrsToStrip)
    if (AS.hasAttribute(Attr))
      R.addAttribute(Attr);

  if (R.hasAttributes())
    AH.setAttributes(AH.getAttributes().removeAttributesAtIndex(Ctx, Index));
}

static void stripNonValidAttributesFromPrototype(Function &F) {
  LLVMContext &Ctx = F.getContext();

  // Intrinsics are very delicate.  Lowering sometimes depends the presence
  // of certain attributes for correctness, but we may have also inferred
  // additional ones in the abstract machine model which need stripped.  This
  // assumes that the attributes defined in Intrinsic.td are conservatively
  // correct for both physical and abstract model.
  if (Intrinsic::ID id = F.getIntrinsicID()) {
    F.setAttributes(Intrinsic::getAttributes(Ctx, id));
    return;
  }

  for (Argument &A : F.args())
    if (isa<PointerType>(A.getType()))
      RemoveNonValidAttrAtIndex(Ctx, F,
                                A.getArgNo() + AttributeList::FirstArgIndex);

  if (isa<PointerType>(F.getReturnType()))
    RemoveNonValidAttrAtIndex(Ctx, F, AttributeList::ReturnIndex);

  for (auto Attr : FnAttrsToStrip) {
    F.removeFnAttr(Attr);
  }
}

/// Certain metadata on instructions are invalid after running this pass.
/// Optimizations that run after this can incorrectly use this metadata to
/// optimize functions. We drop such metadata on the instruction.
static void stripInvalidMetadataFromInstruction(Instruction &I) {
  if (!isa<LoadInst>(I) && !isa<StoreInst>(I))
    return;
  // These are the attributes that are still valid on loads and stores after
  // this pass.
  // The metadata implying dereferenceability and noalias are (conservatively)
  // dropped.  This is because semantically, after this pass runs,
  // all calls to gc.statepoint "free" the entire heap. Also, gc.statepoint can
  // touch the entire heap including noalias objects. Note: The reasoning is
  // same as stripping the dereferenceability and noalias attributes that are
  // analogous to the metadata counterparts.
  // We also drop the invariant.load metadata on the load because that metadata
  // implies the address operand to the load points to memory that is never
  // changed once it became dereferenceable. This is no longer true after this
  // pass. Similar reasoning applies to invariant.group metadata, which applies
  // to loads within a group.
  unsigned ValidMetadata[] = {LLVMContext::MD_tbaa,
                              LLVMContext::MD_range,
                              LLVMContext::MD_alias_scope,
                              LLVMContext::MD_nontemporal,
                              LLVMContext::MD_nonnull,
                              LLVMContext::MD_align,
                              LLVMContext::MD_type};

  // Drops all metadata on the instruction other than ValidMetadata.
  I.dropUnknownNonDebugMetadata(ValidMetadata);
}

static void stripNonValidDataFromBody(Function &F) {
  if (F.empty())
    return;

  LLVMContext &Ctx = F.getContext();
  MDBuilder Builder(Ctx);

  // Set of invariantstart instructions that we need to remove.
  // Use this to avoid invalidating the instruction iterator.
  SmallVector<IntrinsicInst*, 12> InvariantStartInstructions;

  for (Instruction &I : instructions(F)) {
    // invariant.start on memory location implies that the referenced memory
    // location is constant and unchanging. This is no longer true after
    // this pass runs because there can be calls to gc.statepoint
    // which frees the entire heap and the presence of invariant.start allows
    // the optimizer to sink the load of a memory location past a statepoint,
    // which is incorrect.
    if (auto *II = dyn_cast<IntrinsicInst>(&I))
      if (II->getIntrinsicID() == Intrinsic::invariant_start) {
        InvariantStartInstructions.push_back(II);
        continue;
      }

    if (MDNode *Tag = I.getMetadata(LLVMContext::MD_tbaa)) {
      MDNode *MutableTBAA = Builder.createMutableTBAAAccessTag(Tag);
      I.setMetadata(LLVMContext::MD_tbaa, MutableTBAA);
    }

    stripInvalidMetadataFromInstruction(I);

    if (auto *Call = dyn_cast<CallBase>(&I)) {
      for (int i = 0, e = Call->arg_size(); i != e; i++)
        if (isa<PointerType>(Call->getArgOperand(i)->getType()))
          RemoveNonValidAttrAtIndex(Ctx, *Call, i + AttributeList::FirstArgIndex);
      if (isa<PointerType>(Call->getType()))
        RemoveNonValidAttrAtIndex(Ctx, *Call, AttributeList::ReturnIndex);
    }
  }

  // Delete the invariant.start instructions and RAUW undef.
  for (auto *II : InvariantStartInstructions) {
    II->replaceAllUsesWith(UndefValue::get(II->getType()));
    II->eraseFromParent();
  }
}

/// Returns true if this function should be rewritten by this pass.
static bool shouldRewriteStatepointsIn(Function &F) {
  return F.hasGC() && F.getGC() == "go";
}

static void stripNonValidData(Module &M) {
#ifndef NDEBUG
  assert(llvm::any_of(M, shouldRewriteStatepointsIn) && "precondition!");
#endif

  for (Function &F : M)
    stripNonValidAttributesFromPrototype(F);

  for (Function &F : M)
    stripNonValidDataFromBody(F);
}

static bool callsInlineAsm(const CallBase* Call) {
  Value* Callee = Call->getCalledOperand();
  return isa<InlineAsm>(Callee);
}

// could be rdx, r14 and rsp
static bool isGoReadRegister(Value * V) {
  if (CallInst *Call = dyn_cast<CallInst>(V)) {
    if (Function *Fn = Call->getCalledFunction()) {
      if (Fn->getIntrinsicID() == Intrinsic::read_register) {
        return true;
      }
    }
  }
  return false;
}

static bool isGoStackAddr(Value * V) {
  V = V->stripPointerCasts();
  if (CallInst *Call = dyn_cast<CallInst>(V)) {
    if (Function *Fn = Call->getCalledFunction()) {
      if (Fn->getIntrinsicID() == Intrinsic::go_stackaddress) {
        return true;
      }
    }
    return false;
  } else if (IntToPtrInst *ITP = dyn_cast<IntToPtrInst>(V)) {
    return isGoStackAddr(ITP->getOperand(0));
  }
  return false;
}

static bool isGoPreAllocated(Value * V) {
  return TangoLLVM::isGoPreAllocated(V);
}

static bool hasStackMap(const CallBase *Call) {
  auto CC = Call->getCallingConv();
  if (!(CC == CallingConv::Go_Internal || CC == CallingConv::Go_0)) {
    return false;
  }
  if (Function *Fn = Call->getCalledFunction()) {
    StringRef Name = Fn->getName();

    if (Name.starts_with("runtime.gcWriteBarrier")) {
      return false;
    }

    if (Name == "runtime.typedmemclr"
        || Name == "runtime.typedmemmove"
        || Name == "runtime.wbZero"
        || Name == "runtime.wbMove"
        || Name == "runtime.KeepAlive") {
      return false;
    }
  }
  return true;
}

static bool optimizeNilCheck(Function &F) {
  SmallVector<LoadInst*, 8> NilChecks;
  for (Instruction &I : instructions(F)) {
    LoadInst *LI = dyn_cast<LoadInst>(&I);
    if (LI == nullptr)
      continue;
    if (LI->hasMetadata("go.nilcheckload"))
      NilChecks.push_back(LI);
  }

  if (NilChecks.empty())
    return false;

  for (LoadInst *LI : NilChecks) {
    Instruction *Pos = LI;
    DebugLoc DL = LI->getDebugLoc();
    Value *Ptr = LI->getPointerOperand();
    Type *VoidTy = Type::getVoidTy(F.getContext());
    FunctionType *FT = FunctionType::get(VoidTy, {Ptr->getType()}, false);
    // TODO: x86-64 only
    InlineAsm *IA = InlineAsm::get(FT, "testb ${0:b}, ($0)", "r,~{flags},~{dirflag}", true);
    CallInst *Call = CallInst::Create(IA, {Ptr});
    Call->setDebugLoc(DL);
    Call->insertBefore(Pos);
    if (LI->use_empty())
      LI->eraseFromParent();
  }


  return true;
}

static bool replaceUndefPointerWithNullPointer(Function &F) {
  bool Changed = false;

  for (BasicBlock &BB : F) {
    for (Instruction &I : BB) {
      PHINode *PN = dyn_cast<PHINode>(&I);
      if (!PN)
        break;

      if (!isHandledGCPointerType(PN->getType()))
        continue;

      unsigned NumPHIValues = PN->getNumIncomingValues();
      for (unsigned i = 0; i < NumPHIValues; i++) {
        Value *InVal = PN->getIncomingValue(i);
        if (isa<UndefValue>(InVal)) {
          // TODO: use other zero constant
          Type *Ty = PN->getType();
          if (!Ty->isPointerTy())
            report_fatal_error("TODO: support other zero constant values");

          PN->setIncomingValue(i, ConstantPointerNull::get(cast<PointerType>(Ty)));
          Changed = true;
        }
      }
    }
  }
  return Changed;
}

static Value* getNonNullValueFromBr(BasicBlock* BB, DenseMap<BasicBlock*,DenseSet<Value*>> &NNs) {
  BasicBlock *Pred = BB->getUniquePredecessor();
  if (!Pred)
    return nullptr;
  if (!isa<BranchInst>(Pred->getTerminator()))
    return nullptr;
  BranchInst *BI = dyn_cast<BranchInst>(Pred->getTerminator());
  if (!BI->isConditional())
    return nullptr;
  if (!isa<ICmpInst>(BI->getCondition()))
    return nullptr;
  ICmpInst *ICI = dyn_cast<ICmpInst>(BI->getCondition());
  if (ICI->getPredicate() != CmpInst::ICMP_EQ
      && ICI->getPredicate() == CmpInst::ICMP_NE)
    return nullptr;

  enum NullConstant {
    Null,
    NonNull,
    Unknown
  };
  std::function<NullConstant(Value*)> IsNullConstant;
  IsNullConstant = [&](Value* V) {
    if (NNs.contains(Pred) && NNs[Pred].contains(V))
      return NonNull;
    if (Constant *C = dyn_cast<Constant>(V))
      return C->isNullValue()? Null: NonNull;
    if (isa<ZExtInst>(V) || isa<SExtInst>(V))
      return IsNullConstant(dyn_cast<Instruction>(V)->getOperand(0));
    return Unknown;
  };

  Value *Op0 = ICI->getOperand(0);
  Value *Op1 = ICI->getOperand(1);
  Value* Op = nullptr;
  NullConstant Ret;
  if (Ret = IsNullConstant(Op0); Ret != Unknown) {
    Op = Op1;
  } else if (Ret = IsNullConstant(Op1); Ret != Unknown) {
    Op = Op0;
  }
  BasicBlock* SuccTrue = BI->getSuccessor(0);
  BasicBlock* SuccFalse = BI->getSuccessor(1);
  if (ICI->getPredicate() == CmpInst::ICMP_EQ) {
    if (Ret == NonNull && BB == SuccTrue)
      return Op;
    if (Ret == Null && BB == SuccFalse)
      return Op;
  } else if (ICI->getPredicate() == CmpInst::ICMP_NE) {
    if (Ret == Null && BB == SuccTrue)
      return Op;
  }
  return nullptr;
}

static bool isNonNullPointer(Instruction *I, BasicBlock* BB, DenseSet<Value*> NonNullVs) {
  if (isGoReadRegister(I))
    return true;

  // constant may be cast to ptr or used in phi
  if (Constant *C = dyn_cast<Constant>(I))
    return !C->isNullValue();

  if (isa<ZExtInst>(I) || isa<SExtInst>(I))
    return NonNullVs.contains(dyn_cast<Instruction>(I)->getOperand(0));

  // should be pointer-like
  if (!I->getType()->isPointerTy())
    return false;

  if (SelectInst* SI = dyn_cast<SelectInst>(I))
    return NonNullVs.contains(SI->getTrueValue())
           && NonNullVs.contains(SI->getFalseValue());

  if (IntToPtrInst* ITP = dyn_cast<IntToPtrInst>(I))
    return NonNullVs.contains(ITP->getOperand(0));
  if (PtrToIntInst* PTI = dyn_cast<PtrToIntInst>(I))
    return NonNullVs.contains(PTI->getOperand(0));

  if (TangoLLVM::isGoPreAllocated(I)
      || isa<AllocaInst>(I)
      || isa<ConstantExpr>(I)
      || isa<GlobalVariable>(I))
    return true;

  if (CallBase *CB = dyn_cast<CallBase>(I))
    if (Function *Callee = CB->getCalledFunction())
      if (Callee->getAttributes().hasRetAttr(Attribute::NonNull))
        return true;

  if (GetElementPtrInst* GEP = dyn_cast<GetElementPtrInst>(I)) {
    // GEP is non-null if the base is non-null
    if (NonNullVs.contains(GEP->getPointerOperand()))
      return true;
    // GEP is non-null if the base is a global variable
    if (isa<GlobalVariable>(GEP->getPointerOperand()))
      return true;
  }
  return false;
}

static uint64_t getDereferSize(Value* V, unsigned Idx,
                               DenseMap<Value*, DenseMap<uint64_t, uint64_t>> &DereferSizes) {
  if (DereferSizes.contains(V)) {
    if (DereferSizes[V].contains(Idx))
      return DereferSizes[V][Idx];
    return 0;
  }
  if (Instruction *I = dyn_cast<Instruction>(V)) {
    StringRef Metadata = TangoLLVM::getGoMetadataName(GoMetadata::DereferSize);
    if (!I->hasMetadata(Metadata))
      return 0;
    auto *MD = I->getMetadata(Metadata);
    assert(MD->getNumOperands() >= 3);
    unsigned Len = mdconst::extract<ConstantInt>(MD->getOperand(0))->getLimitedValue();
    for (unsigned N = 0; N < Len; N++) {
      uint64_t Index = mdconst::extract<ConstantInt>(MD->getOperand(1 + 2 * N))->getLimitedValue();
      uint64_t Size = mdconst::extract<ConstantInt>(MD->getOperand(2 + 2 * N))->getLimitedValue();;
      DereferSizes[V][Index] = Size;
    }
    if (DereferSizes[V].contains(Idx))
      return DereferSizes[V][Idx];
  }
  return 0;
}

static MDNode* mergeDereferMetadata(MDNode *L, MDNode *R) {
  if (!L || !R)
    return nullptr;
  if (L == R)
    return L;
  unsigned LLen = mdconst::extract<ConstantInt>(L->getOperand(0))->getLimitedValue();
  unsigned RLen = mdconst::extract<ConstantInt>(R->getOperand(0))->getLimitedValue();
  if (LLen != RLen)
    return nullptr;
  // for CallInst, only care about the first part. see TangoLLVM/DereferSize
  // MDNode::get(F.getContext(), {CommonLen, CommonIdx, SizeMD})
  bool AllSame = true;
  DenseMap<uint64_t, uint64_t> Sizes;
  for (unsigned I = 0; I < LLen; I++) {
    uint64_t LID = mdconst::extract<ConstantInt>(L->getOperand(1 + 2 * I))->getLimitedValue();
    uint64_t RID = mdconst::extract<ConstantInt>(R->getOperand(1 + 2 * I))->getLimitedValue();
    if (LID != RID)
      return nullptr;
    uint64_t LVal = mdconst::extract<ConstantInt>(L->getOperand(2 + 2 * I))->getLimitedValue();
    uint64_t RVal = mdconst::extract<ConstantInt>(R->getOperand(2 + 2 * I))->getLimitedValue();
    if (LVal!= RVal) {
      AllSame = false;
      LVal = std::min(LVal, RVal);
    }
    Sizes[LID] = LVal;
  }
  if (AllSame)
    return L;

  SmallVector<Metadata *, 4> MDs;
  MDs.reserve(2 * LLen + 1);
  MDs.push_back(ConstantAsMetadata::get(ConstantInt::get(Type::getInt64Ty(L->getContext()), LLen)));
  for (auto &Pair : Sizes) {
    MDs.push_back(ConstantAsMetadata::get(ConstantInt::get(Type::getInt64Ty(L->getContext()), Pair.first)));
    MDs.push_back(ConstantAsMetadata::get(ConstantInt::get(Type::getInt64Ty(L->getContext()), Pair.second)));
  }
  return MDNode::get(L->getContext(), MDs);
}


static void populateDereferMetadata(Function &F) {
  ConstantAsMetadata *CommonLen = ConstantAsMetadata::get(
      ConstantInt::get(Type::getInt64Ty(F.getContext()), 1));
  ConstantAsMetadata *CommonIdx = ConstantAsMetadata::get(
      ConstantInt::get(Type::getInt64Ty(F.getContext()), 0));

  bool Changed = true;
  DenseMap<Value*, DenseMap<uint64_t, uint64_t>> DereferSizes;
  StringRef GoDereferMetadata = TangoLLVM::getGoMetadataName(GoMetadata::DereferSize);
  while (Changed) {
    Changed = false;
    for (BasicBlock &BB : F) {
      for (Instruction &I : BB) {
        if (I.hasMetadata(GoDereferMetadata))
          continue;

        if (CallBase *CI = dyn_cast<CallBase>(&I)) {
          Function *Callee = CI->getCalledFunction();
          if (Callee && Callee->hasMetadata(GoDereferMetadata)) {
            I.setMetadata(GoDereferMetadata, Callee->getMetadata(GoDereferMetadata));
            for (auto i = 0; i < Callee->arg_size(); i++) {
              Instruction *Arg = dyn_cast<Instruction>(CI->getArgOperand(i));
              uint64_t Size = TangoLLVM::getDereferSize(Callee, i, true);
              if (Size > 0 && Arg && !Arg->hasMetadata(GoDereferMetadata)) {
                ConstantAsMetadata *SizeMD = ConstantAsMetadata::get(
                    ConstantInt::get(Type::getInt64Ty(F.getContext()), Size));
                Arg->setMetadata(GoDereferMetadata,
                                 MDNode::get(F.getContext(),
                                             {CommonLen, CommonIdx, SizeMD}));
              }
            }
            Changed = true;
          }
        }
        // %0 = call xxx
        // %1 = extractvalue { ptr, i64 }, %0, 0
        else if (ExtractValueInst *EVI = dyn_cast<ExtractValueInst>(&I)) {
          if (EVI->getNumIndices() != 1)
            continue;
          unsigned Size = getDereferSize(EVI->getOperand(0), EVI->getIndices()[0], DereferSizes);
          if (Size > 0) {
            ConstantAsMetadata *SizeMD = ConstantAsMetadata::get(
                ConstantInt::get(Type::getInt64Ty(F.getContext()), Size));
            EVI->setMetadata(GoDereferMetadata,
                             MDNode::get(F.getContext(), {CommonLen, CommonIdx, SizeMD}));
            Changed = true;
          }
        }
        // %0 = load i64, @runtime.error
        // %1 = inttoptr %0
        else if (IntToPtrInst *ITP = dyn_cast<IntToPtrInst>(&I)) {
          unsigned Size = getDereferSize(ITP->getOperand(0), 0, DereferSizes);
          if (Size > 0) {
            ConstantAsMetadata *SizeMD = ConstantAsMetadata::get(
                ConstantInt::get(Type::getInt64Ty(F.getContext()), Size));
            ITP->setMetadata(GoDereferMetadata,
                             MDNode::get(F.getContext(), {CommonLen, CommonIdx, SizeMD}));
            Changed = true;
          }
        }
        // phi?
        else if (PHINode* PHI = dyn_cast<PHINode>(&I)) {
          MDNode* Node = nullptr;
          for (unsigned i = 0, e = PHI->getNumIncomingValues(); i < e; i++) {
            Value *Incoming = PHI->getIncomingValue(i);
            Instruction *In = dyn_cast<Instruction>(Incoming);
            if (!In || !In->hasMetadata(GoDereferMetadata)) {
              Node = nullptr;
              break;
            }
            if (Node == nullptr) {
              Node = In->getMetadata(GoDereferMetadata);
            } else {
              Node = mergeDereferMetadata(Node, In->getMetadata(GoDereferMetadata));
            }
            if (!Node)
              break;
          }
          if (Node) {
            PHI->setMetadata(GoDereferMetadata, Node);
            Changed = true;
          }
        }
      }
    }
  }
}

static void analyzeNonNullGep(Function &F, DenseSet<Value*> &NNGeps) {
  using ValueSet = DenseSet<Value*>;
  // NonNull Pointers at BB
  DenseMap<BasicBlock*, ValueSet> InNN;
  DenseMap<BasicBlock*, ValueSet> OutNN;

  auto Intersect = [](ValueSet &A, const ValueSet &B) {
      for (auto I = A.begin(), E = A.end(); I != E;) {
        Value *V = *I;
        I++;
        if (!B.contains(V))
          A.erase(V);
      }
  };

  auto JoinBB = [&](BasicBlock* BB, ValueSet& NNs) {
    bool First = true;
    for (BasicBlock *Pred : predecessors(BB)) {
      if (!OutNN.contains(Pred))
        continue;
      // meet the non-null pointers
      if (First)
        NNs = OutNN[Pred];
      else
        Intersect(NNs, OutNN[Pred]);
      First = false;
    }

    // Phi is non-null if all predecessors are non-null
    for (Instruction &I : *BB) {
      if (!isa<PHINode>(&I))
        break;
      PHINode *PN = dyn_cast<PHINode>(&I);
      bool NNPhi = true;
      for (unsigned II = 0, E = PN->getNumIncomingValues(); II < E; II++) {
        Value *Incoming = PN->getIncomingValue(II);
        BasicBlock* Pred = PN->getIncomingBlock(II);
        if (OutNN.contains(Pred) && !OutNN[Pred].contains(Incoming)) {
          NNPhi = false;
          break;
        }
      }
      if (NNPhi)
        NNs.insert(PN);
    }
  };

  auto ProcessBB = [&](BasicBlock *BB, SmallSetVector<BasicBlock*, 16>& Worklist) {
    // join predecessors
    ValueSet NNs;
    JoinBB(BB, NNs);
    Value *NNV = getNonNullValueFromBr(BB, OutNN);
    if (NNV)
      NNs.insert(NNV);

    if (InNN.contains(BB) && InNN[BB] == NNs)
      return;

    InNN[BB] = NNs;
    for (Instruction &I : make_range(BB->begin(), BB->end())) {
      if (isa<PHINode>(&I))
        continue;

      if (isNonNullPointer(&I, BB, NNs))
        NNs.insert(&I);

      // Pointer is non-null after it's Loaded/Stored
      if (LoadInst *LI = dyn_cast<LoadInst>(&I))
        NNs.insert(LI->getPointerOperand());
      else if (StoreInst *SI = dyn_cast<StoreInst>(&I))
        NNs.insert(SI->getPointerOperand());

      // the ConstantExpr/GEP is a compiler time constant value,
      // so ignore it
      // if (ConstantExpr *CE = dyn_cast<ConstantExpr>(&I);
      //      CE && CE->getOpcode() == Instruction::GetElementPtr) {}

      // if the base pointer is non-null, collect the GEP
      if (GetElementPtrInst* GEP = dyn_cast<GetElementPtrInst>(&I)) {
        // GlobalVariable is not an Inst, so it is not stored in NNs
        if (isa<GlobalVariable>(GEP->getPointerOperand())) {
          NNs.insert(GEP->getPointerOperand());
        }
        if (NNGeps.contains(GEP) && !NNs.contains(GEP->getPointerOperand())) {
          NNGeps.erase(GEP);
        }
        if (NNs.contains(GEP->getPointerOperand()))
          NNGeps.insert(GEP);
      }
    }
    OutNN[BB] = NNs;
    Worklist.insert(succ_begin(BB), succ_end(BB));
  };

  SmallSetVector<BasicBlock*, 16> Worklist;
  // pre-process with RPO may be more efficient
  ReversePostOrderTraversal<Function*> RPOT(&F);
  for (BasicBlock *BB : RPOT) {
    ProcessBB(BB, Worklist);
  }
  while (!Worklist.empty()) {
    BasicBlock* BB = Worklist.front();
    Worklist.remove(BB);
    ProcessBB(BB, Worklist);
  }
}

static void analyzeDereferenceableGeps(DenseSet<GetElementPtrInst*> &NonDerefGEPs,
                                       DenseSet<GetElementPtrInst*> &GEPs,
                                       Function &F,
                                       DominatorTree &DT,
                                       const TargetLibraryInfo &TLI) {
  // 1. populate the dereferenceable metadata from go-frontend
  populateDereferMetadata(F);
  // 2. analyze the dereferenceable GEP
  const DataLayout &DL = F.getParent()->getDataLayout();
  for (GetElementPtrInst *GEP : GEPs) {
    Value *Base = GEP->getPointerOperand();
    if (IntToPtrInst *ITP = dyn_cast<IntToPtrInst>(Base))
      if (isGoReadRegister(ITP->getOperand(0)))
        continue;

    // ignore vector <N x Ptr>
    if (!Base->getType()->isPointerTy())
      continue;

    APInt Offset(DL.getIndexTypeSizeInBits(GEP->getType()), 0);
    if (!GEP->accumulateConstantOffset(DL, Offset) || Offset.isNegative())
      NonDerefGEPs.insert(GEP);
    // Is it correct to obtain the size from getResultElementType()?
    // Possibly not, since getResultElementType() might be optimized to 'i8'.
    // However, there is no better alternative currently.
    // False positives are acceptable here, as this may cause a GEP
    // that doesn't strictly require moving to be moved, which is safe.
    APInt Size(DL.getPointerTypeSizeInBits(GEP->getType()),
               DL.getTypeStoreSize(GEP->getResultElementType()));
    if (!TangoLLVM::isDereferenceable(
            Base, Offset + Size.sextOrTrunc(Offset.getBitWidth()), DL, TLI))
      NonDerefGEPs.insert(GEP);
  }
}


// the number of usage won't too large, so use SmallSet/SmallMap
template<typename T, unsigned DefaultBuckets = 8>
using GoSmallSet = llvm::SmallDenseSet<T, DefaultBuckets>;
template<typename K, typename V, unsigned DefaultBuckets = 8>
using GoSmallMap = llvm::SmallDenseMap<K, V, DefaultBuckets>;

struct GEPUsages {
  // first usage
  GoSmallSet<Instruction*> FirstUsages;
  // first usage -> id
  GoSmallMap<Instruction*, unsigned> FUIds;
  // id -> first usage, only for query
  GoSmallMap<unsigned, Instruction*> Id2Fu;
  // the ids of first usages that through stack map
  GoSmallSet<unsigned> UnsafeIds;
  // the following usages of a first usage, the key is the id of the first usage
  GoSmallMap<unsigned, GoSmallSet<Instruction*>> Children;
  // for phi, record the incoming basic block and the id of the incoming value
  GoSmallMap<Instruction*, GoSmallMap<BasicBlock*, unsigned>> PhiFUIds;
  // for phi, record the id to the index of the incoming value
  GoSmallMap<Instruction*, GoSmallMap<unsigned, GoSmallSet<unsigned>>> PhiId2Idx;
};

const unsigned UnDef = 0;
const unsigned Def = 1;
const unsigned StackMap = 2;

struct ReHoistDebugger {
private:
  const Function &F;
  bool Debug;
  std::string Buffer;
  raw_string_ostream OS;

  std::string idName(unsigned Id) {
    switch (Id) {
    case UnDef:
      return "Undef";
    case Def:
      return "Def";
    case StackMap:
      return "StackMap";
    default:
      return std::to_string(Id);
    }
  };

public:
  ReHoistDebugger(const Function &F)
      : F(F), Debug(F.getName() == ReHoistDebugFunc), Buffer(), OS(Buffer) {}

  void start() {
    if (Debug)
      OS << "ReHoist start: " << F.getName() << "\n";
  }

  void printCandidates(const DenseSet<GetElementPtrInst *> &GEPs,
                       const DenseSet<GetElementPtrInst *> &Candidates) {
    if (!Debug)
      return;
    F.print(OS);
    for (GetElementPtrInst *GEP : GEPs) {
      OS << "# GEP: " << *GEP << "\n";
      if (Candidates.contains(GEP))
        OS << "  - ToMove (Unsafe Pointer)\n";
      else
        OS << "  - Not moved (Safe Pointer)\n";
    }
  }

  void printAnalyzeResult(SetVector<GetElementPtrInst *> WorkSet,
                          SetVector<GetElementPtrInst *> NeedMove,
                          DenseSet<Value *> NNBase,
                          DenseSet<GetElementPtrInst *> NonDerefGEPs,
                          DenseMap<Value *, GEPUsages> Usages) {
    if (!Debug || WorkSet.empty())
      return;

    const DataLayout &DL = F.getParent()->getDataLayout();
    for (GetElementPtrInst *GEP : WorkSet) {
      OS << "** GEP: " << *GEP << "\n";
      APInt Offset(DL.getIndexTypeSizeInBits(GEP->getType()), 0);
      bool ConstantIdx = GEP->accumulateConstantOffset(DL, Offset);
      OS << "  - Constant Idx? : " << (ConstantIdx ? "True" : "False")
             << "\n";
      OS << "  - Base is "
             << (NNBase.contains(GEP) ? "Non-Null" : "NULL !!!") << "\n";
      OS << "  - GEP  is " << (NonDerefGEPs.contains(GEP) ? "Non-" : "")
             << "Dereferenceable\n";
      if (NeedMove.contains(GEP)) {
        assert(!Usages[GEP].FirstUsages.empty());
        OS << "  - First Usage: Size = " << Usages[GEP].FirstUsages.size()
               << "\n";
        for (Instruction *FU : Usages[GEP].FirstUsages) {
          if (isa<PHINode>(FU)) {
            OS << "  - First Usage: \n";
            OS << "    " << *FU << ", at BB: " << FU->getParent()->getName()
                   << "\n";
            for (auto &Pair : Usages[GEP].PhiFUIds[FU])
              OS << "    BB: " << Pair.first->getName()
                     << ", Id:" << Pair.second << "\n";
            continue;
          }
          unsigned Id = Usages[GEP].FUIds[FU];
          OS << "  - First Usage: \n";
          OS << "    " << *FU << ", at BB: " << FU->getParent()->getName()
                 << ", Id:" << Id << "\n";
          if (!Usages[GEP].Children[Id].empty()) {
            OS << "  Follow: Size = " << Usages[GEP].Children[Id].size()
                   << "\n";
            for (Value *V : Usages[GEP].Children[Id])
              OS << "   " << *V << ", at BB: "
                     << dyn_cast<Instruction>(V)->getParent()->getName()
                     << "\n";
          }
        }
      } else {
        OS << "    - Not moved (Not through StackMap)\n";
      }
    }
  }

  void analyzeBB(BasicBlock *BB, const DenseMap<Value *, unsigned> &Ids) {
    if (Debug) {
      OS << "  BB: " << BB->getName() << ", InState:\n";
      for (auto &Pair : Ids)
        OS << "  - " << *Pair.first << ", Id:" << idName(Pair.second) << "\n";
    }
  }

  void analyzeBBDone(BasicBlock *BB, const DenseMap<Value *, unsigned> &Ids) {
    if (Debug) {
      OS << "  BB: " << BB->getName() << ", OutState:\n";
      for (auto &Pair : Ids)
        OS << "  - " << *Pair.first << ", Id:" << idName(Pair.second) << "\n";
    }
  }

  void startMove() {
    if (Debug)
      F.print(OS);
  }

  void processGEP(GetElementPtrInst *GEP) {
    if (Debug)
      OS << "  process GEP: " << *GEP << "\n";
  }

  void processFirstUse(Instruction *I) {
    if (Debug)
      OS << "  process First Use: " << *I << "\n";
  }

  void processFollowingUse(Instruction *I) {
    if (Debug)
      OS << "  process Follow Use: " << *I << "\n";
  };

  void nonEmptyErr(Instruction *I) {
    if (!Debug || I->use_empty())
      return;
    dbgs() << "Use Not Empty: " << *I << "\n";
    dbgs() << OS.str() << "\n";
    dbgs() << "Error Function:\n";
    F.print(dbgs());
  }

  ~ReHoistDebugger() {
    if (Debug)
      dbgs() << OS.str() << "\n";
  }
};

static SetVector<GetElementPtrInst*> analyzeGepUsages(
    Function &F,
    DominatorTree &DT,
    SetVector<GetElementPtrInst*> &WorkSet,
    DenseMap<Value*, GEPUsages> &GEPUsages,
    ReHoistDebugger &Debugger) {
  SetVector<GetElementPtrInst*> NeedMove;
  auto AddToMoveList = [&](GetElementPtrInst* GEP) {
    if (!WorkSet.contains(GEP))
      return;
    NeedMove.insert(GEP);
  };

  DenseMap<Value*, unsigned> IdGetter;
  DenseMap<BasicBlock*, DenseMap<Value*, unsigned>> In;
  DenseMap<BasicBlock*, DenseMap<Value*, unsigned>> Out;

  // Undef: 0, Def: 1, StackMap: 2
  auto GetId = [&](Value* V) {
    unsigned Id;
    if (IdGetter.contains(V))
      Id = IdGetter[V] + 1;
    else
      Id = StackMap + 1;
    IdGetter[V] = Id;
    return Id;
  };
  auto MergeId = [](unsigned L, unsigned R) {
    if (L == R)
      return L;
    unsigned Max = std::max(L, R);
    unsigned Min = std::min(L, R);
    // GEP through stack map in at least one path, mark it as StackMap
    // then we can assign a new id for it.
    if (Min >= StackMap)
      return StackMap;
    if (Max >= StackMap)
      return StackMap;

    assert(Max == Def && Min == UnDef);
    // 1. the path with undef hasn't been analyzed
    // 2. if there is a usage in the following path, there must be a phi node
    return Def;
  };
  auto JoinBB = [&](BasicBlock* BB, DenseMap<Value*, unsigned>& Ids) {
    DenseMap<Value*, unsigned> Count;
    for (BasicBlock *Pred : predecessors(BB)) {
      for (auto &Pair : Out[Pred]) {
        Value *GEP = Pair.first;
        Count[GEP]++;
        if (!Ids.contains(GEP))
          Ids[GEP] = Pair.second;
        else
          Ids[GEP] = MergeId(Ids[GEP], Pair.second);
      }
    }
    for (auto &Pair: Count) {
      Value *GEP = Pair.first;
      unsigned Id = Ids[GEP];
      unsigned C = Pair.second;
      if (C < pred_size(BB))
        // if some of BB's predecessors have not been analyzed, treated it
        // as the first usage, unless a known first usage dom the BB. this
        // may happen when we meet a back edge. maybe it's a bit more conservative
        if (Id > StackMap) {
          Instruction *FU = GEPUsages[Pair.first].Id2Fu[Id];
          if (isa<PHINode>(FU) || !DT.dominates(FU->getParent(), BB))
            Ids[Pair.first] = StackMap;
        }
    }
  };
  auto ProcessBB = [&](BasicBlock *BB, SmallSetVector<BasicBlock*, 16>& Worklist) {
    DenseMap<Value*, unsigned> Ids;
    JoinBB(BB, Ids);
    if (In.contains(BB) && In[BB] == Ids)
      return;
    Debugger.analyzeBB(BB, Ids);
    In[BB] = Ids;

    for (Instruction &I : make_range(BB->begin(), BB->end())) {
      if (isa<GetElementPtrInst>(&I))
        Ids[&I] = Def;

      for (unsigned II = 0, E = I.getNumOperands(); II < E; II++) {
        Value *V = I.getOperand(II);
        GetElementPtrInst* GEP = dyn_cast<GetElementPtrInst>(V);
        if (!GEP)
          continue;

        if (PHINode *PHI = dyn_cast<PHINode>(&I)) {
          unsigned PhiIdx = II;
          BasicBlock* Pred = PHI->getIncomingBlock(PhiIdx);
          unsigned PredId = Out[Pred][GEP];
          if (PredId <= StackMap) {
            // Phi would be a special first-usage that has no children
            if (!GEPUsages[GEP].PhiFUIds[PHI].contains(Pred)) {
              PredId = GetId(GEP);
              GEPUsages[GEP].FirstUsages.insert(PHI);
              GEPUsages[GEP].PhiFUIds[PHI][Pred] = PredId;
              GEPUsages[GEP].PhiId2Idx[PHI][PredId].insert(PhiIdx);
              if (PredId == StackMap) {
                AddToMoveList(GEP);
                GEPUsages[GEP].UnsafeIds.insert(PredId);
              }
            } else {
              // there maybe duplicate phi incoming value
              PredId = GEPUsages[GEP].PhiFUIds[PHI][Pred];
              GEPUsages[GEP].PhiId2Idx[PHI][PredId].insert(PhiIdx);
            }
          } else if (PredId > StackMap) {
            if (!GEPUsages[GEP].PhiFUIds[PHI].contains(Pred)) {
              GEPUsages[GEP].Children[PredId].insert(PHI);
              GEPUsages[GEP].PhiId2Idx[PHI][PredId].insert(PhiIdx);
            } else {
              // there maybe duplicate phi incoming value
              GEPUsages[GEP].PhiId2Idx[PHI][PredId].insert(PhiIdx);
            }
          }
          continue;
        }

        unsigned Id = Ids[GEP];
        assert(Id != UnDef);
        if (Id <= StackMap) {
          // we meet the first usage
          if (!GEPUsages[GEP].FirstUsages.contains(&I)) {
            Id = GetId(GEP);
            GEPUsages[GEP].FirstUsages.insert(&I);
            GEPUsages[GEP].FUIds[&I] = Id;
            GEPUsages[GEP].Id2Fu[Id] = &I;
          }
          Ids[GEP] = GEPUsages[GEP].FUIds[&I];
          // there is state point between def and first usage
          if (Id == StackMap) {
            AddToMoveList(GEP);
            GEPUsages[GEP].UnsafeIds.insert(Ids[GEP]);
          }
        } else {
          if (!GEPUsages[GEP].FirstUsages.contains(&I))
            GEPUsages[GEP].Children[Id].insert(&I);
          else
            Ids[GEP] = GEPUsages[GEP].FUIds[&I];
        }
      }

      if (CallBase* CB = dyn_cast<CallBase>(&I);
          CB && hasStackMap(CB)) {
        for (auto &Pair : Ids)
          if (Pair.second == Def)
            Pair.second = StackMap;
      }
    }

    Debugger.analyzeBBDone(BB, Ids);
    Out[BB] = Ids;
    Worklist.insert(succ_begin(BB), succ_end(BB));
  };

  SmallSetVector<BasicBlock*, 16> Worklist;
  ReversePostOrderTraversal<Function*> RPOT(&F);
  for (BasicBlock *BB : RPOT) {
    ProcessBB(BB, Worklist);
  }
  while (!Worklist.empty()) {
    BasicBlock* BB = Worklist.front();
    Worklist.remove(BB);
    ProcessBB(BB, Worklist);
  }

  return NeedMove;
}

static void moveHoistedGEP(Function &F, DominatorTree &DT,
                           const SetVector<GetElementPtrInst*> &ToMove,
                           DenseMap<Value*, GEPUsages> &GEPUsages,
                           ReHoistDebugger &Debugger) {
  Debugger.startMove();
  auto CreateNewGEP = [&](GetElementPtrInst *GEP, unsigned Id) {
    GetElementPtrInst *NewGEP = dyn_cast<GetElementPtrInst>(GEP->clone());
    NewGEP->copyMetadata(*GEP);
    NewGEP->setName(GEP->getName() + ".gosp.hoist.id" + std::to_string(Id));
    return NewGEP;
  };

  for (GetElementPtrInst *GEP : ToMove) {
    assert(GEPUsages.contains(GEP));
    Debugger.processGEP(GEP);
    for (Instruction *FirstUse : GEPUsages[GEP].FirstUsages) {
      Debugger.processFirstUse(FirstUse);
      if (PHINode* PHI = dyn_cast<PHINode>(FirstUse)) {
        SmallDenseMap<BasicBlock*, GetElementPtrInst*> B2G;
        for (unsigned I = 0, E = PHI->getNumIncomingValues(); I < E; I++) {
          BasicBlock *Pred = PHI->getIncomingBlock(I);
          if (GEPUsages[GEP].PhiFUIds[PHI].contains(Pred)) {
            unsigned Id = GEPUsages[GEP].PhiFUIds[PHI][Pred];
            assert(!GEPUsages[GEP].Children.contains(Id));
            GetElementPtrInst *NewGEP = nullptr;
            if (B2G.contains(Pred)) {
              NewGEP = B2G[Pred];
            } else {
              NewGEP = CreateNewGEP(GEP, Id);
              NewGEP->insertBefore(Pred->getTerminator());
              B2G[Pred] = NewGEP;
            }
            if (!GEPUsages[GEP].PhiId2Idx[PHI][Id].contains(I)) {
              dbgs() << "PHI: " << *PHI << ", Id: " << Id << ", Idx: " << I << "\n";
              dbgs() << "GEP: " << *GEP << "\n";
              F.print(dbgs());
            }
            assert(GEPUsages[GEP].PhiId2Idx[PHI][Id].contains(I));
            PHI->setIncomingValue(I, NewGEP);
          }
        }
        // we have asserted that Phi has no children
        continue;
      }
      unsigned Id = GEPUsages[GEP].FUIds[FirstUse];
      GetElementPtrInst *NewGEP = CreateNewGEP(GEP, Id);
      NewGEP->insertBefore(FirstUse);
      FirstUse->replaceUsesOfWith(GEP, NewGEP);
      if (GEPUsages[GEP].Children.contains(Id)) {
        for (Value *U : GEPUsages[GEP].Children[Id]) {
          Instruction *User = dyn_cast<Instruction>(U);
          if (!User)
            continue;
          Debugger.processFollowingUse(User);
          if (PHINode *PHI = dyn_cast<PHINode>(User)) {
            for (unsigned Idx : GEPUsages[GEP].PhiId2Idx[PHI][Id]) {
              PHI->setIncomingValue(Idx, NewGEP);
            }
          } else {
            User->replaceUsesOfWith(GEP, NewGEP);
          }
        }
      }
    }
  }

  for (Instruction *I : ToMove) {
    Debugger.nonEmptyErr(I);
    assert(I->use_empty());
    I->eraseFromParent();
  }
}

static bool reHoistGEP(DominatorTree &DT, LoopInfo &LI, Function &F,
                       const TargetLibraryInfo &TLI) {
  if (!RehoistGEP)
    return false;

  DenseSet<GetElementPtrInst*> GEPs;
  for (BasicBlock &BB : F)
    for (Instruction &I : BB)
      if (GetElementPtrInst* GEP = dyn_cast<GetElementPtrInst>(&I))
        // filter GEP?
        GEPs.insert(GEP);

  if (GEPs.empty())
    return false;

  ReHoistDebugger Debugger(F);
  Debugger.start();

  // 1. forward analysis to confirm if a pointer is non-null
  DenseSet<Value*> NNGeps;
  analyzeNonNullGep(F, NNGeps);

  // 2. check if a GEP is dereferenceable
  DenseSet<GetElementPtrInst*> NonDerefGEPs;
  analyzeDereferenceableGeps(NonDerefGEPs, GEPs, F, DT, TLI);

  DenseSet<GetElementPtrInst*> ToMove;
  for (GetElementPtrInst *GEP : GEPs) {
    if (!NNGeps.contains(GEP) || NonDerefGEPs.contains(GEP))
      ToMove.insert(GEP);
  }

  Debugger.printCandidates(GEPs, ToMove);

  // there is nothing need to process
  if (ToMove.empty())
    return false;

  // 3. forward analysis to find the first usage and the following usage
  //    of a GEP, and check if it's through stack map
  bool Changed = false;
  // we must analyze and move the GEPs that are not the base of other GEPs
  DenseMap<GetElementPtrInst*, DenseSet<GetElementPtrInst*>> SplitByBase;
  for (GetElementPtrInst *GEP : ToMove) {
    if (GetElementPtrInst *Base = dyn_cast<GetElementPtrInst>(GEP->getPointerOperand());
        Base && ToMove.contains(Base))
      SplitByBase[Base].insert(GEP);
  }
  DenseSet<GetElementPtrInst*> Visited;
  while(true) {
    // filter the GEPs that are not the base of other GEPs
    SetVector<GetElementPtrInst*> WorkSet;
    for (GetElementPtrInst *GEP : ToMove) {
      if (!SplitByBase.contains(GEP) || SplitByBase[GEP].empty()) {
        if (!Visited.insert(GEP).second)
          continue;
        WorkSet.insert(GEP);
      }
    }
    for (GetElementPtrInst *GEP : WorkSet) {
      auto *Base = dyn_cast<GetElementPtrInst>(GEP->getPointerOperand());
      if (Base && SplitByBase.contains(Base))
        SplitByBase[Base].erase(GEP);
    }
    if (WorkSet.empty()) {
      assert(Visited.size() == ToMove.size());
      break;
    }

    DenseMap<Value *, GEPUsages> GEPUsages;
    auto NeedMove = analyzeGepUsages(F, DT, WorkSet, GEPUsages, Debugger);
    Debugger.printAnalyzeResult(WorkSet, NeedMove, NNGeps, NonDerefGEPs, GEPUsages);
    moveHoistedGEP(F, DT, NeedMove, GEPUsages, Debugger);
    Changed |= !NeedMove.empty();
  }

  return Changed;
}

struct UpdateUse {
  Value *Val;
  Instruction *User;
  unsigned OpNo;

  UpdateUse(Value *V, Instruction *U, unsigned O)
    : Val(V), User(U), OpNo(O) {}

  void apply() {
    User->setOperand(OpNo, Val);
  }
};

// cast non-zero constant int to ptr, these instructions are generated
// by optimizations, such as SROA
// -------
// %2 = phi ptr [null, b1], [%1 (alloca), b2]
// statepoint
// %3 = gep %2, 192
// -->
// %1.1 = gep %1, 192
// %null.1 = inttoptr 192 // gep null, 192
// %2 = phi ptr [%0, b1], [%1, b2]
static bool isNonZeroConstantInt(Value *V) {
  auto IsNonZeroConstantInt = [](Value *Tmp) {
    if (ConstantInt *CI = dyn_cast<ConstantInt>(Tmp); CI && !CI->isZero())
        return true;
    return false;
  };
  if (IsNonZeroConstantInt(V)) {
    return true;
  }
  if (auto *CE = dyn_cast<ConstantExpr>(V)) {
    if (CE->getOpcode() == Instruction::IntToPtr)
      if (IsNonZeroConstantInt(CE->getOperand(0)))
        return true;
  }
  return false;
}

static bool legalizeTangoLLVMIR(Function &F, DomTreeUpdater *DTU) {
  DenseMap<AllocaInst*, CallInst*> PreAllocated;
  SmallVector<CallInst*, 8> PanicReturns;
  // ComputedCalleeAreaSize is the max caller area size of all callees.
  int64_t ComputedCalleeAreaSize = 0;
  for (Instruction &I : instructions(F)) {
    if (PHINode *PHI = dyn_cast<PHINode>(&I)) {
      if (PHI->getType()->isPointerTy())
        for (unsigned i = 0; i < PHI->getNumIncomingValues(); i++)
          if (isNonZeroConstantInt(PHI->getIncomingValue(i))) {
            TangoLLVM::markObsoleted(&F);
            break;
          }
    } else if (IntToPtrInst *ITP = dyn_cast<IntToPtrInst>(&I)) {
      if (isNonZeroConstantInt(ITP->getOperand(0))) {
        TangoLLVM::markObsoleted(&F);
      }
    } else if (AllocaInst *AI = dyn_cast<AllocaInst>(&I)) {
      (void)AI;
      assert(AI->getAlign() <= F.getParent()->getDataLayout().getStackAlignment() && "invariant");
    } else if (CallInst *CI = dyn_cast<CallInst>(&I)) {
      int64_t CalleeCallerAreaSize = TangoLLVM::getCallSiteGoCallerAreaSize(CI);
      if (CalleeCallerAreaSize >= ComputedCalleeAreaSize)
        ComputedCalleeAreaSize = CalleeCallerAreaSize;

      if (CI->getIntrinsicID() == Intrinsic::go_preallocated) {
        Value *Arg = CI->getArgOperand(0);
        if (AllocaInst *AI = dyn_cast<AllocaInst>(Arg)) {
          auto It = PreAllocated.find(AI);
          if (It == PreAllocated.end()) {
            PreAllocated[AI] = CI;
          } else {
            CallInst *OtherCI = It->second;
            // they should have the same arguments
            for (unsigned I = 0; I < CI->arg_size(); ++I) {
              if (CI->getArgOperand(I) != OtherCI->getArgOperand(I))
                report_fatal_error("inconsistent preallcoated");
            }
          }
        }
      } else if (CI->getIntrinsicID() == Intrinsic::go_panicreturn) {
        PanicReturns.push_back(CI);
      }
    }
  }

  {
    // sanity check
    MDNode *MD = F.getMetadata("go.callee.args.area");
    if (MD == nullptr) {
      // TODO: this may happen if we fail to lower a llvm.memmove to instructions
      // We should ensure that all memmove from OpMove must be lowered to memcpy.inline.
      if (ComputedCalleeAreaSize != 0)
        report_fatal_error("bad callee args area: should be zero");
    } else {
      int64_t CalleeArgAreaSize = dyn_cast<ConstantAsMetadata>(MD->getOperand(0))
                                      ->getValue()->getUniqueInteger().getSExtValue();
      if (ComputedCalleeAreaSize > CalleeArgAreaSize) {
        bool HasOtherCall = false;
        bool HasGoPanicBounds = false;
        bool HasMemmove = false;
        bool HasMemclr = false;
        for (Instruction &I : instructions(F)) {
          if (CallInst *CI = dyn_cast<CallInst>(&I)) {
            if (CI->getIntrinsicID() != 0)
              continue;
            if (Function *Callee = CI->getCalledFunction()) {
              if (Callee->getName() == "runtime.memmove") {
                HasMemmove = true;
                continue;
              } else if (Callee->getName() == "runtime.memclrNoHeapPointers") {
                HasMemclr = true;
                continue;
              } else if (TangoLLVM::isGoPanicBounds(Callee->getName())) {
                HasGoPanicBounds = true;
                continue;
              } else {
                HasOtherCall = true;
              }
            }
          }
        }
        bool ReportFailure = HasOtherCall;
        if (ReportFailure) {
          // 24 is the space for memmove
          if (HasMemmove && ComputedCalleeAreaSize == 24) {
            ReportFailure = false;
          } else if (HasMemclr && ComputedCalleeAreaSize == 16) {
            ReportFailure = false;
          }
        }
        if (ReportFailure) {
#if !defined(NDEBUG) || defined(LLVM_ENABLE_DUMP)
          F.dump();
#endif
          dbgs() << F.getName() << ": bad callee args size: " << ComputedCalleeAreaSize << " vs. " << CalleeArgAreaSize << "\n";
          report_fatal_error("bad callee args area");
        } else {
          if (HasGoPanicBounds && ComputedCalleeAreaSize < 16) {
            report_fatal_error("bad callee args area: must be at least 16 for panic bounds");
          }
          // We need to update the callee size
          TangoLLVM::updateGoCalleeAreaSize(F, ComputedCalleeAreaSize);
        }
      }
    }
  }

  bool Changed = false;

  if (!PanicReturns.empty())
    Changed = true;

  for (CallInst *CI : PanicReturns) {
    auto RemoveBr = [&]() {
      // Check if panicreturn is the last statement of the basic block
      BasicBlock *BB = CI->getParent();

      TangoLLVM::prepareFixGoPanicReturn(BB);

      SmallVector<DominatorTree::UpdateType, 2> DTUpdates;
      SmallVector<BasicBlock*, 2> Succs;
      for (BasicBlock *Succ : successors(BB)) {
        Succs.push_back(Succ);
        DTUpdates.push_back({DominatorTree::Delete, BB, Succ});
      }

      Instruction* T = BB->getTerminator();
      T->eraseFromParent();

      DTU->applyUpdates(DTUpdates);

      for (BasicBlock *Succ : Succs) {
        if (pred_size(Succ) == 0)
          DTU->deleteBB(Succ);
      }

      IRBuilder<> Builder(BB);
      Builder.CreateUnreachable();
    };

    RemoveBr();
    if (!CI->use_empty()) {
      CI->replaceAllUsesWith(Constant::getNullValue(CI->getType()));
    }
    CI->eraseFromParent();

#if 0
    auto &U = *CI->use_begin();
    if (PHINode *PN = dyn_cast<PHINode>(U.getUser())) {
      unsigned Num = PHINode::getIncomingValueNumForOperand(U.getOperandNo());
      PN->removeIncomingValue(Num);
      RemoveBr();
      CI->eraseFromParent();
    } else if (ReturnInst *RI = dyn_cast<ReturnInst>(U.getUser())) {
      BasicBlock *BB = RI->getParent();
      RI->eraseFromParent();
      CI->eraseFromParent();
      IRBuilder<> Builder(BB);
      Builder.CreateUnreachable();
    } else {
      llvm_unreachable("invalid use of panicreturn");
    }
#endif
  }

  for (auto I : PreAllocated) {
    AllocaInst *AI = I.first;
    CallInst *CI = I.second;
    if (CI->getParent() == nullptr) {
      report_fatal_error("bad state");
    }
    Value *Null = ConstantPointerNull::get(cast<PointerType>(AI->getType()));
    // Cut usage
    CI->setArgOperand(0, Null);
    CI->moveBefore(AI);
    if (CI->paramHasAttr(0, Attribute::NonNull)) {
      CI->removeParamAttr(0, Attribute::NonNull);
    }
    SmallVector<Instruction*> OtherIntrinsics;
    for (auto &U : AI->uses()) {
      Instruction *I = dyn_cast<Instruction>(U.getUser());
      if (I == CI) {
        report_fatal_error("bad state");
      } else if (isGoPreAllocated(I)) {
        OtherIntrinsics.push_back(I);
      }
    }
    for (Instruction* I : OtherIntrinsics) {
      I->replaceAllUsesWith(CI);
      I->eraseFromParent();
    }
    AI->replaceAllUsesWith(CI);
    AI->eraseFromParent();
    Changed = true;
  }
  return Changed;
}

/**
 * we change the code
 * ////////////////////
 * %1 = load i64 ....
 * call ...
 * call ...
 * %2 = inttoptr i64 %1 to i8*
 * call %2
 * ////////////////////
 * to
 * ////////////////////
 * %1 = load i64 ....
 * %2 = inttoptr i64 %1 to i8*
 * call ...
 * call ...
 * call %2
 * ////////////////////
 * so that %2 can be record in StatePoints and copystack can process %2
 * correctly
 */
static bool moveITPToDefAndPTIToUse(Function &F) {
  if (!MoveITPToDef && !MovePTIToUse)
    return false;
  bool Changed = false;
  SmallVector<Instruction*, 4> ITPs;
  SmallVector<Instruction*, 4> PTIs;
  for (Instruction &I : instructions(F))
    if (IntToPtrInst* ITP = dyn_cast<IntToPtrInst>(&I))
      ITPs.push_back(ITP);
    else if (PtrToIntInst* PTI = dyn_cast<PtrToIntInst>(&I))
      PTIs.push_back(PTI);

  if (MoveITPToDef) {
    DenseMap<Instruction*, Instruction*> SrcToITP;
    for (Instruction *ITP : ITPs) {
      Instruction *Src = dyn_cast<Instruction>(ITP->getOperand(0));
      if (Src == nullptr)
        continue;

      Changed = true;
      auto I = SrcToITP.find(Src);
      if (I != SrcToITP.end()) {
        ITP->replaceAllUsesWith(I->second);
        ITP->eraseFromParent();
        continue;
      }
      SrcToITP[Src] = ITP;
      if (isa<PHINode>(Src)) {
        ITP->moveBefore(Src->getParent()->getFirstNonPHIOrDbg());
      } else {
        ITP->moveAfter(Src);
      }
    }

    SmallVector<IntToPtrInst*, 8> ToCheck;
    for (Instruction &I : instructions(F)) {
      for (User *U : I.users()) {
        if (auto *ITP = dyn_cast<IntToPtrInst>(U)) {
          if (ITP->getOperand(0) == &I)
            ToCheck.push_back(ITP);
        }
      }
    }

    DenseMap<Instruction*, IntToPtrInst*> Canonical;
    for (auto *ITP : ToCheck) {
      auto *Key = dyn_cast<Instruction>(ITP->getOperand(0));
      if (auto *C = Canonical.lookup(Key)) {
        ITP->replaceAllUsesWith(C);
        ITP->eraseFromParent();
      } else {
        Canonical[Key] = ITP;
      }
    }
  }

  if (MovePTIToUse) {
    IRBuilder<> Builder(F.getContext());
    for (Instruction *PTI : PTIs) {
      if (PTI->use_empty())
        continue;

      SmallVector<UpdateUse, 4> Actions;
      bool Used = false;

      DenseMap<Instruction*,Instruction*> IP2NewPTI;
      auto NextPTI = [&](Instruction *IP) {
        auto It = IP2NewPTI.find(IP);
        if (It != IP2NewPTI.end()) {
          return It->second;
        }

        if (!Used) {
          Used = true;
          PTI->moveBefore(IP);
          IP2NewPTI[IP] = PTI;
          return PTI;
        }

        Builder.SetInsertPoint(IP);
        Instruction *NewPTI = cast<Instruction>(Builder.CreatePtrToInt(PTI->getOperand(0), PTI->getType()));
        // TODO: can we copy all metadata?
        NewPTI->copyMetadata(*PTI);
        IP2NewPTI[IP] = NewPTI;
        return NewPTI;
      };

      for (auto &U : PTI->uses()) {
        Instruction *I = dyn_cast<Instruction>(U.getUser());
        if (I == nullptr)
          continue;

        Instruction *IP = I;
        if (PHINode *PN = dyn_cast<PHINode>(I)) {
          unsigned Num = PHINode::getIncomingValueNumForOperand(U.getOperandNo());
          BasicBlock *UseBlock = PN->getIncomingBlock(Num);
          IP = UseBlock->getTerminator();
        }

        Changed = true;
        Instruction *NewPTI = NextPTI(IP);
        if (NewPTI != PTI) {
          Actions.push_back({NewPTI, I, U.getOperandNo()});
        }
      }

      for (auto &A : Actions)
        A.apply();
    }
  }

  return Changed;
}

static bool needsRewriteForGo(CallBase *Call) {
  return !isa<GCStatepointInst>(Call) && !callsInlineAsm(Call) && hasStackMap(Call);
}

bool GoStatepoints::runOnFunction(Function &F, DominatorTree &DT,
                                  LoopInfo &LI,
                                  TargetTransformInfo &TTI,
                                  const TargetLibraryInfo &TLI) {
  assert(!F.isDeclaration() && !F.empty() &&
         "need function body to rewrite statepoints in");
  assert(shouldRewriteStatepointsIn(F) && "mismatch in rewrite decision");
  LLVM_DEBUG(dbgs() << "compute statepoints for function " << F.getName() << "\n";);
  if (!PrintFunc.empty())
    PrintLiveSet = F.getName() == PrintFunc;
  if (PrintLiveSet)
    dbgs() << "\n********** Liveness of function " << F.getName() << " **********\n";

  auto NeedsRewrite = [&TLI](Instruction &I) {
    if (auto *Call = dyn_cast<CallBase>(&I)) {
      if (callsGCLeafFunction(Call, TLI) && needsRewriteForGo(Call)) {
        report_fatal_error("needsRewriteForGo should reject all GCLeafFunctions");
      }
      return !callsGCLeafFunction(Call, TLI) && needsRewriteForGo(Call);
    }
    return false;
  };

  DomTreeUpdater DTU(DT, DomTreeUpdater::UpdateStrategy::Lazy);

  // Preprocess intrinsics.
  if (convertIntrinsicsToCalls(F))
    DTU.getDomTree();

  // Delete any unreachable statepoints so that we don't have unrewritten
  // statepoints surviving this pass.  This makes testing easier and the
  // resulting IR less confusing to human readers.
  bool MadeChange = removeUnreachableBlocks(F, &DTU);
  // Legalize all alloca and go preallocated.
  // TODO: move to a standalone pass
  MadeChange |= legalizeTangoLLVMIR(F, &DTU);

  // Rewrite all the calls that need statepoints to invokes, so we can
  // attach a stack map through its landing pad.
  // SmallVector<CallInst *, 64> Calls;
  // for (Instruction &I : instructions(F))
  //   if (NeedsRewrite(I) && isa<CallInst>(I))
  //     Calls.push_back(cast<CallInst>(&I));

  // We should not run removeUnreachableBlocks at this point, as it
  // will remove the dummy landing pads.

  // Flush the Dominator Tree.
  DTU.getDomTree();

  // Gather all the statepoints which need rewritten.  Be careful to only
  // consider those in reachable code since we need to ask dominance queries
  // when rewriting.  We'll delete the unreachable ones in a moment.
  SmallVector<CallBase *, 64> ParsePointNeeded;
  for (Instruction &I : instructions(F)) {
    // TODO: only the ones with the flag set!
    if (NeedsRewrite(I)) {
      // NOTE removeUnreachableBlocks() is stronger than
      // DominatorTree::isReachableFromEntry(). In other words
      // removeUnreachableBlocks can remove some blocks for which
      // isReachableFromEntry() returns true.
      assert(DT.isReachableFromEntry(I.getParent()) &&
            "no unreachable blocks expected");
      ParsePointNeeded.push_back(cast<CallBase>(&I));
    }
  }

  // Return early if no work to do.
  if (ParsePointNeeded.empty())
    return MadeChange;

  // As a prepass, go ahead and aggressively destroy single entry phi nodes.
  // These are created by LCSSA.  They have the effect of increasing the size
  // of liveness sets for no good reason.  It may be harder to do this post
  // insertion since relocations and base phis can confuse things.
  for (BasicBlock &BB : F)
    if (BB.getUniquePredecessor()) {
      MadeChange = true;
      FoldSingleEntryPHINodes(&BB);
    }


  std::vector<BasicBlock*> SplittedBlocks;
  if (!ParsePointNeeded.empty()) {
    SplittedBlocks.reserve(ParsePointNeeded.size());
    bool Modified = false;
    for (auto Call : ParsePointNeeded) {
      BasicBlock* Cur = Call->getParent();
      Instruction* Next = Call->getNextNode();
      if (Next != Cur->getTerminator()) {
        Modified = true;
        MadeChange = true;
        BasicBlock * NewBB = SplitBlock(Cur, Next, &DTU, &LI, nullptr, "gc.split");
        SplittedBlocks.push_back(NewBB);
      }
    }

    if (Modified)
      DTU.getDomTree();
  }

  // Before we start introducing relocations, we want to tweak the IR a bit to
  // avoid unfortunate code generation effects.  The main example is that we
  // want to try to make sure the comparison feeding a branch is after any
  // safepoints.  Otherwise, we end up with a comparison of pre-relocation
  // values feeding a branch after relocation.  This is semantically correct,
  // but results in extra register pressure since both the pre-relocation and
  // post-relocation copies must be available in registers.  For code without
  // relocations this is handled elsewhere, but teaching the scheduler to
  // reverse the transform we're about to do would be slightly complex.
  // Note: This may extend the live range of the inputs to the icmp and thus
  // increase the liveset of any statepoint we move over.  This is profitable
  // as long as all statepoints are in rare blocks.  If we had in-register
  // lowering for live values this would be a much safer transform.
  auto getConditionInst = [](Instruction *TI) -> Instruction* {
    if (auto *BI = dyn_cast<BranchInst>(TI))
      if (BI->isConditional())
        return dyn_cast<Instruction>(BI->getCondition());
    // TODO: Extend this to handle switches
    return nullptr;
  };
  for (BasicBlock &BB : F) {
    Instruction *TI = BB.getTerminator();
    if (auto *Cond = getConditionInst(TI))
      // TODO: Handle more than just ICmps here.  We should be able to move
      // most instructions without side effects or memory access.
      if (isa<ICmpInst>(Cond) && Cond->hasOneUse()) {
        MadeChange = true;
        Cond->moveBefore(TI);
      }
  }

  MadeChange |= moveITPToDefAndPTIToUse(F);
  MadeChange |= reHoistGEP(DT, LI, F, TLI);
  MadeChange |= insertParsePoints(F, DT, LI, TTI, ParsePointNeeded);
  MadeChange |= replaceUndefPointerWithNullPointer(F);
  MadeChange |= optimizeNilCheck(F);

  if (!SplittedBlocks.empty()) {
    for (BasicBlock *BB : SplittedBlocks) {
      // We do not update CFG in this pass
#ifndef NDEBUG
      BasicBlock *Pred = BB->getUniquePredecessor();
      assert(Pred != nullptr && "invariant");
      // BB is splitted from Pred;
      assert(Pred->getUniqueSuccessor() == BB && "invariant");
#endif
      if (!MergeBlockIntoPredecessor(BB, &DTU, &LI)) {
        report_fatal_error("fail to merge splitted bb back into predecessor");
      }
    }
  }

  return MadeChange;
}

// liveness computation via standard dataflow
// -------------------------------------------------------------------

// TODO: Consider using bitvectors for liveness, the set of potentially
// interesting values should be small and easy to pre-compute.

static Value*
isAlloca(Value *V, DefiningValueMapTy &DVCache) {
  if (!isHandledGCPointerType(V->getType()))
    return nullptr;
  Value *Base = findBaseOrBDV(V, DVCache);
  return isa<AllocaInst>(Base) ? Base : nullptr;
}

static Value*
isGoPreAllocated(Value *V, DefiningValueMapTy &DVCache) {
  if (!isHandledGCPointerType(V->getType()))
    return nullptr;
  Value *Base = findBaseOrBDV(V, DVCache);
  return isGoPreAllocated(Base) ? Base : nullptr;
}

static Value*
isTrackedAlloca(Value *V, DefiningValueMapTy &DVCache) {
  Value *Base = isAlloca(V, DVCache);
  if (Base &&
      hasPointerTy(getValueElementType(Base)))
    return Base;
  Base = isGoPreAllocated(V, DVCache);
  if (Base && isGoPreAllocatedParamOut(Base) && hasPointerTy(getValueElementType(Base)))
    return Base;
  return nullptr;
}

/// Compute the live-in set for the location rbegin starting from
/// the live-out set of the basic block
static void computeLiveInValues(BasicBlock::reverse_iterator Begin,
                                BasicBlock::reverse_iterator End,
                                SetVector<Value *> &LiveTmp,
                                DefiningValueMapTy &DVCache,
                                MapVector<Instruction*, SetVector<Value *>> &GenAt) {
  for (auto &I : make_range(Begin, End)) {
    // KILL/Def - Remove this definition from LiveIn
    LiveTmp.remove(&I);

    // Don't consider *uses* in PHI nodes, we handle their contribution to
    // predecessor blocks when we seed the LiveOut sets
    if (isa<PHINode>(I))
      continue;

    // Here is the logic of computing GenSet at each instruction.
    // Record it for later usage
    SetVector<Value*> &Gen = GenAt[&I];

    // USE - Add to the LiveIn set for this instruction
    for (Value *V : I.operands()) {
      assert(!isUnhandledGCPointerType(V->getType()) &&
             "unexpected value type");
      if (isGoGCPointerOrPointerLikeValue(V)) {
        // The choice to exclude all things constant here is slightly subtle.
        // There are two independent reasons:
        // - We assume that things which are constant (from LLVM's definition)
        // do not move at runtime.  For example, the address of a global
        // variable is fixed, even though it's contents may not be.
        // - Second, we can't disallow arbitrary inttoptr constants even
        // if the language frontend does.  Optimization passes are free to
        // locally exploit facts without respect to global reachability.  This
        // can create sections of code which are dynamically unreachable and
        // contain just about anything.  (see constants.ll in tests)
        if (isAlloca(V, DVCache)) {
          Value *Base = isTrackedAlloca(V, DVCache);
          if (!Base) {
            continue;
          }

          // For non-address-taken alloca, record its use.
          if (isa<DbgInfoIntrinsic>(I)) {
            // Not real use.
            continue;
          }

          if (isa<BitCastInst>(I) || isa<InsertValueInst>(I) ||
              isa<GetElementPtrInst>(I) || isa<ICmpInst>(I) ||
              isa<InsertElementInst>(I) || isa<ExtractElementInst>(I) ||
              isa<AddrSpaceCastInst>(I) || isa<PtrToIntInst>(I) ||
              isa<IntToPtrInst>(I)) {
            Gen.insert(Base);
            LiveTmp.insert(Base);
            // Not real use but help to keep liveness
            continue;
          }

          if (isa<SelectInst>(I)) {
            // SelectInst is tricky
            // TODO: SelectInst is not a real use because it does not access memory.
            // However, the use of SelectInst assumes that the two operands must be fully
            // initialized.
            // TODO: We need to treat SelectInst as Load/Store. Once we detect a SelectInst,
            // we need to force the initialization of the base Alloca.
            Gen.insert(Base);
            LiveTmp.insert(Base);
            // Not real use but help to keep liveness
            continue;
          }

          // The only real use are Load/Store.
          // CallInst is a potential real use.
          if (isa<LoadInst>(I) || isa<StoreInst>(I)
              || isa<AtomicCmpXchgInst>(I)
              || isa<AtomicRMWInst>(I) ) {
            Gen.insert(Base);
            LiveTmp.insert(Base);
            continue;
          }

          if (CallInst *CI = dyn_cast<CallInst>(&I)) {
            if (Function *Fn = CI->getCalledFunction())
              switch (Fn->getIntrinsicID()) {
              case Intrinsic::lifetime_start:
              case Intrinsic::lifetime_end:
              case Intrinsic::assume:
              case Intrinsic::go_metadata:
                // Not real use.
                continue;
              default:
                break;
              }

            Gen.insert(Base);
            LiveTmp.insert(Base);
            continue;
          }

          if (isa<ReturnInst>(&I)) {
            // return alloca, this does indeed happen.
            Gen.insert(Base);
            LiveTmp.insert(Base);
            continue;
          }

          dbgs() << "illegal operation on non-address-taken alloca " << I << ", Base: " << *Base << "\n";
          I.getFunction()->print(dbgs());
          // We know it is not address-taken, other operation should not happen.
          assert(false && "illegal operation on non-address-taken alloca");
        }

        if (auto * PTI = dyn_cast<PtrToIntInst>(V)) {
          Value * Src = PTI->getOperand(0);
          if (Value *Base = isTrackedAlloca(Src, DVCache)) {
            Gen.insert(Base);
            LiveTmp.insert(Base);
          } else {
            Gen.insert(Src);
            LiveTmp.insert(Src);
          }
        }

        Gen.insert(V);
        LiveTmp.insert(V);
      }
    }
  }
}

static void computeLiveOutSeed(BasicBlock *BB, SetVector<Value *> &LiveTmp,
                               DefiningValueMapTy &DVCache) {
  for (BasicBlock *Succ : successors(BB)) {
    for (auto &I : *Succ) {
      PHINode *PN = dyn_cast<PHINode>(&I);
      if (!PN)
        break;

      Value *V = PN->getIncomingValueForBlock(BB);
      assert(!isUnhandledGCPointerType(V->getType()) &&
             "unexpected value type");
      if (isGoGCPointerOrPointerLikeValue(V)) {
        LiveTmp.insert(V);
      }
    }
  }
}

static SetVector<Value *> computeKillSet(BasicBlock *BB, DefiningValueMapTy &DVCache) {
  SetVector<Value *> KillSet;
  for (Instruction &I : *BB) {
    if (isGoGCPointerOrPointerLikeValue(&I))
      KillSet.insert(&I);
  }
  return KillSet;
}

#ifndef NDEBUG
/// Check that the items in 'Live' dominate 'TI'.  This is used as a basic
/// sanity check for the liveness computation.
static void checkBasicSSA(DominatorTree &DT, SetVector<Value *> &Live,
                          Instruction *TI, bool TermOkay = false) {
  for (Value *V : Live) {
    if (auto *I = dyn_cast<Instruction>(V)) {
      // The terminator can be a member of the LiveOut set.  LLVM's definition
      // of instruction dominance states that V does not dominate itself.  As
      // such, we need to special case this to allow it.
      if (TermOkay && TI == I)
        continue;
      if (!DT.dominates(I, TI))
        dbgs() << "basic SSA liveness expectation violated by liveness analysis, I" << *I << "TI" << *TI << "\n";
      assert(DT.dominates(I, TI) &&
             "basic SSA liveness expectation violated by liveness analysis");
    }
  }
}

/// Check that all the liveness sets used during the computation of liveness
/// obey basic SSA properties.  This is useful for finding cases where we miss
/// a def.
static void checkBasicSSA(DominatorTree &DT, GCPtrLivenessData &Data,
                          BasicBlock &BB) {
  checkBasicSSA(DT, Data.LiveSet[&BB], BB.getTerminator());
  checkBasicSSA(DT, Data.LiveOut[&BB], BB.getTerminator(), true);
  checkBasicSSA(DT, Data.LiveIn[&BB], BB.getTerminator());
}
#endif


static void markInstrAsIllFormed(Instruction *I, const char *Reason) {
  if (I->hasMetadata("go.pointerlike.illformed"))
    return;
  I->setMetadata("go.pointerlike.illformed", MDNode::get(I->getContext(), {}));
  if (PrintObsoleted) {
    dbgs() << "Mark Instr IllFormed: " << *I << ", reason is " << Reason << "\n";
  }
}

// return false if we must mark parent as ill-formed pointer-like
// return false if Child is not a pointer like value.
// We need additional backward checking to confirm results.
bool CheckIllFormedPointerForward(Instruction *Parent, Instruction *Child) {
  auto Mark = [](Instruction *I) {
    I->setMetadata("go.pointerlike", MDNode::get(I->getContext(), {}));
    return true;
  };
  auto MarkIllFormed = [](Instruction *I, const char * Reason) {
    if (!I->hasMetadata("go.pointerlike")) {
      I->setMetadata("go.pointerlike", MDNode::get(I->getContext(), {}));
    }
    markInstrAsIllFormed(I, Reason);
    return false;
  };

  if (!isGoGCPointerLikeValue(Child)) {
    return false;
  }

  if (Child->hasMetadata("go.pointerlike.illformed"))
    return MarkIllFormed(Parent, "child is an ill-formed pointer like value");

  if (Parent->hasMetadata("go.pointerlike.illformed"))
    return false;

  if (Parent->hasMetadata("go.pointerlike"))
    return true;

  if (BinaryOperator *BO = dyn_cast<BinaryOperator>(Parent)) {
    Value *Op0 = Parent->getOperand(0);
    Value *Op1 = Parent->getOperand(1);

    if (BO->getOpcode() == Instruction::Add
        // || BO->getOpcode() == Instruction::And
        // || BO->getOpcode() == Instruction::Or
       ) {
      if (Op0 == Child) {
        // only one can be pointer like
        if (Op1 == Child || isGoGCPointerLikeValue(Op1)) {
          return MarkIllFormed(Parent, "op1 is pointer like value");
        }

      } else {
        assert(Op1 == Child && "invariant");
        // only one can be pointer like
        if (isGoGCPointerLikeValue(Op0)) {
          return MarkIllFormed(Parent, "op0 is pointer like value");
        }
      }
    } else if (BO->getOpcode() == Instruction::Sub) {
      if (Op1 == Child || isGoGCPointerLikeValue(Op1))
        return MarkIllFormed(Parent, "Sub has a pointer-like op1");

      assert(Op0 == Child && "invariant");
    } else {
      // all other are ill-format pointers
      return MarkIllFormed(Parent, "unsupported binary operator");
    }
    return Mark(Parent);
  } else if (isa<SelectInst>(Parent)
            || isa<PHINode>(Parent)
            || isa<ShuffleVectorInst>(Parent)
            || isa<BinaryOperator>(Parent)
            || isa<FreezeInst>(Parent)
            || isa<AtomicCmpXchgInst>(Parent)
            ) {
    // fallthrough
    // We can assume Parent is a pointer-like value
    return Mark(Parent);
  } else if (isa<UnaryOperator>(Parent)) {
    // I cannot figure out when a unary operator can make a valid pointer
    // from anther pointer.
    return MarkIllFormed(Parent, "unsupported unary operator");
  } else if (isa<InsertElementInst>(Parent)
      || isa<InsertValueInst>(Parent)
      || isa<ExtractElementInst>(Parent)
      || isa<ExtractValueInst>(Parent)
      ) {
    // Similar to ShuffleVector
    // A value will taint the whole FCA or Vector as ill-formed.
    // Similarly, all values extracted from an ill-formed FCA or Vector
    // is also conservatively marked as ill-formed.
    return Mark(Parent);
  }

#if !defined(NDEBUG) || defined(LLVM_ENABLE_DUMP)
  dbgs() << "find an unsupported pointer like value: " << *Parent << "\n";
  dbgs() << "child: " << *Child << "\n";
  Parent->getParent()->getParent()->dump();
#endif
  report_fatal_error("unsupported pointer like value");
  return MarkIllFormed(Parent, "unsupported parent");
}

struct StackVisitGuard {

  std::vector<Instruction*> *Stack;

  StackVisitGuard(std::vector<Instruction*> *Stack, Instruction *I) {
    this->Stack = Stack;
    Stack->push_back(I);
  }

  ~StackVisitGuard() {
    Instruction *Child = Stack->back();
    Stack->pop_back();
    if (!Stack->empty()) {
      Instruction *Parent = Stack->back();
      // If Parent has a non-instruction Child, the Child will
      // not be put into the Stack and will not trigger a Check.
      // A non-instruction value cannot be a pointer-like value.
      // Thus it has no impact to the well-form or ill-form value.
      CheckIllFormedPointerForward(Parent, Child);
    }
  }
};


// Return true if Start cannot be a well-formed pointer like value.
// We use backward because we must check all operands of Start to
// make decision for it.
// Invariant: Start must have data dependency on PtrToInt
static void checkBackward(Function &F, Instruction* Start, const Type *PtrIntTy) {
  // Not a pointer-like value
  if (Start->getType()->getScalarType() != PtrIntTy) {
    return;
  }

  if (Start->hasMetadata("go.pointerlike.illformed")) {
    return;
  }

  // We only mark a value as pointer-like if all operands have
  // been checked.
  // if (Start->hasMetadata("go.pointerlike")) {
  //   return;
  // }

  SmallSetVector<Value*, 32> Visited;
  std::vector<Instruction*> Stack;

  // auto DumpStack = [&]() {
  //   dbgs() << "Dump visiting stack:\n";
  //   for (Instruction *I : Stack) {
  //     dbgs() << "    " << *I << "\n";
  //   }
  // };

  std::function<void(Value*)> DepthFirstCheck;

  // Return true if V depends on an Ill-Formed pointer.
  DepthFirstCheck = [&](Value *V) {
    Instruction *I = dyn_cast<Instruction>(V);
    if (!I)
      return;

    // For better logging, check type here.
    // Must be i64 or <i64 x n> in 64bit system
    Type *T = V->getType()->getScalarType();
    if (T->isVoidTy() || T->isPointerTy()) {
      return;
    }

    StackVisitGuard Guard(&Stack, I);

    // Sink: stop propagation
    if (isa<LoadInst>(V)
        || isa<CallInst>(V)) {
      // TODO: we must mark the source of IntToPtr as Pointer as well.
      return;
    }

    // We check pointers in the destructor of Guard
    if (Visited.count(V))
      return;

    Visited.insert(V);

    // Freeze, PHI, Select, ShuffleVector and Binary
    // are valid instructions for producing well-formed pointer-like values.
    if (FreezeInst *FI = dyn_cast<FreezeInst>(V)) {
      DepthFirstCheck(FI->getOperand(0));
    } else if (PHINode *PHI = dyn_cast<PHINode>(V)) {
      for (Value *InVal : PHI->incoming_values()) {
        DepthFirstCheck(InVal);
      }
    } else if (SelectInst *SI = dyn_cast<SelectInst>(V)) {
      DepthFirstCheck(SI->getOperand(1));
      DepthFirstCheck(SI->getOperand(2));
    } else if (ShuffleVectorInst *SVI = dyn_cast<ShuffleVectorInst>(V)) {
      DepthFirstCheck(SVI->getOperand(0));
      DepthFirstCheck(SVI->getOperand(1));
    } else if (BinaryOperator *BO = dyn_cast<BinaryOperator>(V)) {
      DepthFirstCheck(BO->getOperand(0));
      DepthFirstCheck(BO->getOperand(1));
    } else {
      // For better logging
      for (Value *Op : I->operands())
        DepthFirstCheck(Op);
    }
  };

  DepthFirstCheck(Start);
}

static void checkNegativeOffset(Function &F) {
  const DataLayout &DL = F.getParent()->getDataLayout();

  auto isNegativeOffset = [](Value *V) {
    if (ConstantInt *Off = dyn_cast<ConstantInt>(V)) {
      return Off->isNegative();
    }
    return false;
  };

  auto isPositiveOffset = [](Value *V) {
    if (ConstantInt *Off = dyn_cast<ConstantInt>(V)) {
      return !(Off->isNegative() || Off->isZero());
    }
    return false;
  };


  std::function<bool(Value*,SmallSet<Value*, 4>&)> isBadBasePointerForNegativeOffsetRecur = nullptr;
  isBadBasePointerForNegativeOffsetRecur = [&](Value *Ptr, SmallSet<Value*, 4> &Visited) {
    if (isa<GetElementPtrInst>(Ptr)
        || isa<IntToPtrInst>(Ptr)) {
      return false;
    }
    if (PHINode *PHI = dyn_cast<PHINode>(Ptr)) {
      if (Visited.count(PHI))
        return false;
      Visited.insert(PHI); // only need to avoid visiting PHI repeatedly
      for (Value *InVal : PHI->incoming_values())
        if (isBadBasePointerForNegativeOffsetRecur(InVal, Visited))
          return true;
      // assume PHI are safe and always related to loop
      return false;
    }
    if (isa<Constant>(Ptr)
        || isa<Argument>(Ptr)
        || isa<LoadInst>(Ptr)
        || isa<CallInst>(Ptr)
        || isa<ExtractValueInst>(Ptr)
        || isa<ExtractElementInst>(Ptr)
       ) {
      return true;
    }
    return false;
  };

  auto isBadBasePointerForNegativeOffset = [&](Value *Ptr) {
    SmallSet<Value*, 4> Visited;
    return isBadBasePointerForNegativeOffsetRecur(Ptr, Visited);
  };

  // Another check: separated in another loop for simplicity
  for (Instruction &I : instructions(F)) {
    if (!isGoGCPointerOrPointerLikeValue(&I)) {
      continue;
    }

    if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(&I)) {
      if (!GEP->hasAllConstantIndices()) {
        continue;
      }
      APInt Off(DL.getPointerSizeInBits(0), 0);
      if (!GEP->accumulateConstantOffset(DL, Off)) {
        report_fatal_error("cannot accumulate offset for constant GEP");
      }

      // TODO: we need to check out of bounds pointers.
      // However, here we can only check negative offset on
      // object base address. Note that a base address may not be
      // a GC-base pointer because objects can be embedded in another
      // object.
      if (!Off.isNegative()) {
        continue;
      }

      Value *P = GEP->getPointerOperand();
      if (isBadBasePointerForNegativeOffset(P)) {
        // All other cases are potential invalid;
        if (PrintObsoleted) {
          dbgs() << F.getName() << ": found a bad pointer " << I << "\n";
        }
        markInstrAsIllFormed(&I, "bad base pointer for negative offset (GEP)");
      }
    } else if (BinaryOperator *BO = dyn_cast<BinaryOperator>(&I)) {
      Value *Op0 = BO->getOperand(0);
      Value *Op1 = BO->getOperand(1);
      Value *Base = nullptr;
      Value *Off = nullptr;
      if (BO->getOpcode() == Instruction::Add
         // || BO->getOpcode() == Instruction::And
         // || BO->getOpcode() == Instruction::Or
          ) {
        if (isGoGCPointerLikeValue(Op0)) {
          if (isNegativeOffset(Op1)) {
            Base = Op0;
            Off = Op1;
          }
        } else if (isGoGCPointerLikeValue(Op1)) {
          assert(isGoGCPointerLikeValue(Op1) && "invariant");
          if (isNegativeOffset(Op0)) {
            Base = Op1;
            Off = Op0;
          }
        } else {
          // dbgs() << "Not a pointer like value: " << *BO << " in " << F.getName() << "\n";
          // may be marked by the front-end
        }
      } else if (BO->getOpcode() == Instruction::Sub) {
        if (isPositiveOffset(Op1)) {
          Base = Op0;
          Off = Op1;
        }
      } else {
        // may be marked by the front-end
      }
      if (Base && Off && isNegativeOffset(Off) && isBadBasePointerForNegativeOffset(Base)) {
        if (PrintObsoleted) {
          dbgs() << F.getName() << ": found a bad pointer " << I << "\n";
        }
        markInstrAsIllFormed(&I, "bad base pointer for negative offset (BinaryOperator)");
      }
    }
  }
}

// Mark all values that depends on a PtrToInt value.
static void checkForward(Function &F, Instruction *Start, const Type *PtrIntTy,
                         SetVector<IntToPtrInst*> &CheckedITPs) {
  SmallVector<Instruction*, 32> Worklist;
  SmallSet<Instruction*, 32> Visited;

  Worklist.push_back(Start);

  while (!Worklist.empty()) {
    Instruction *V = Worklist.pop_back_val();
    Visited.insert(V);
    // We simply use V->uses().
    for (auto &U : V->uses()) {
      Instruction *I = dyn_cast<Instruction>(U.getUser());
      if (!I)
        continue;

      // TODO: AtomicRMWInst
      if (isa<CallInst>(I)
          || isa<InsertElementInst>(I)
          || isa<InsertValueInst>(I)
          || isa<AtomicRMWInst>(I))
        continue;

      if (IntToPtrInst* ITP = dyn_cast<IntToPtrInst>(I)) {
        // We try to find a chain from PtrToInt to IntToPtr.
        // All well-formed pointer values in the Chain must
        // be GC-live to keep IntToPtr live.
        CheckedITPs.insert(ITP);
        checkBackward(F, V, PtrIntTy);
        continue;
      }

      if (StoreInst *SI = dyn_cast<StoreInst>(I)) {
        if (CheckIllFormedPointerStore && SI->getValueOperand() == V) {
          // StoreInst is like IntToPtr.
          // Since Opaque Pointer, we may store a pointer-like value without casting
          // it to a pointer.
          if (PrintObsoleted) {
            dbgs() << F.getName() << ": found a dangerous store of well-formed pointer-like integer: " << *I << "\n";
          }
          // Start from V not I because I is StoreInst and it is not a
          // pointer-like value.
          checkBackward(F, V, PtrIntTy);
        }
        continue;
      }

      if (AtomicCmpXchgInst *ACXI = dyn_cast<AtomicCmpXchgInst>(I)) {
        // Similar to Store
        if (CheckIllFormedPointerStore && (ACXI->getCompareOperand() == V || ACXI->getNewValOperand() == V)) {
          if (PrintObsoleted) {
            dbgs() << F.getName() << ": found a dangerous atomic cmpxchg of well-formed pointer-like integer: " << *I << "\n";
          }
          checkBackward(F, V, PtrIntTy);
        }
        continue;
      }

      // We only care about Use that may be a pointer-like-value
      Type *T = I->getType()->getScalarType();
      if (T->isVoidTy())
        continue; // void cannot propagate

      if (T->isPointerTy())
        continue; // already a pointer, no need to propagate

      if (!Visited.count(I))
        Worklist.push_back(I);
    }
  }

}

static void checkGoPointerLikeValues(Function &F) {
  assert(F.getParent()->getDataLayout().getPointerSize() == 8 && "only 64bit");
  const Type *PtrIntTy = Type::getInt64Ty(F.getContext());

  SetVector<IntToPtrInst*> CheckedITPs;
  SetVector<IntToPtrInst*> AllITPs;
  for (Instruction &I : instructions(F)) {
    if (PtrToIntInst *PTI = dyn_cast<PtrToIntInst>(&I)) {
      checkForward(F, PTI, PtrIntTy, CheckedITPs);
      continue;
    }
    if (IntToPtrInst *ITP = dyn_cast<IntToPtrInst>(&I)) {
      AllITPs.insert(ITP);
      continue;
    }
  }
  AllITPs.set_subtract(CheckedITPs);
  for (IntToPtrInst *ITP : AllITPs) {
    // The Src of a IntToPtrInst must be a pointer-like value.
    // If the Src does not derive from a PtrToInt, we probably have no need to track it.
    // However, it produces many false warnings in the MIR fixup pass.
    // Here, we simply mark the value as pointer-like value to avoid unnecessary false warnings.
    if (Instruction *Src = dyn_cast<Instruction>(ITP->getOperand(0)))
      checkBackward(F, Src, PtrIntTy);
  }

  std::function<void(Value*)> MarkAllIllFormed;
  MarkAllIllFormed = [&](Value *V) {
    for (auto &U : V->uses()) {
      Instruction *I = dyn_cast<Instruction>(U.getUser());
      if (!I)
        continue;

      if (I->hasMetadata("go.pointerlike.illformed"))
        continue;

      if (I->hasMetadata("go.pointerlike")) {
        markInstrAsIllFormed(I, "propagate ill-formed pointer-like value");
        MarkAllIllFormed(I);
      }
    }
  };

  // Assumption: A PtrToIntInst is valid if it is not converted
  // from a non-zero constant or ill-formed integer to IntToPtr
  for (Instruction &I : instructions(F)) {
    if (PtrToIntInst *PTI = dyn_cast<PtrToIntInst>(&I)) {
      Value * Src = PTI->getOperand(0);
      if (ConstantInt *CI = dyn_cast<ConstantInt>(Src)) {
        if (CI->isZero()) {
          continue;
        }
        // report error:
        if (PrintObsoleted) {
          dbgs() << F.getName() << ": a PtrToInt depends on a non-zero constant: " << *Src << "\n";
#if !defined(NDEBUG) || defined(LLVM_ENABLE_DUMP)
          F.dump();
#endif
        }
        MarkAllIllFormed(PTI);
      } else if (IntToPtrInst *ITP = dyn_cast<IntToPtrInst>(Src)) {
        if (Instruction *I = dyn_cast<Instruction>(ITP->getOperand(0))) {
          if (I->hasMetadata("go.pointerlike.illformed")) {
            if (PrintObsoleted) {
              dbgs() << F.getName() << ": a PtrToInt depends on an ill-formed IntToPtr: " << *ITP << "\n";
#if !defined(NDEBUG) || defined(LLVM_ENABLE_DUMP)
              F.dump();
#endif
            }
            MarkAllIllFormed(PTI);
          }
        }
      }
    }
  }

  checkNegativeOffset(F);
}

static bool SameBlockUsage(Instruction *Def) {
  BasicBlock *BB = Def->getParent();
  BasicBlock *Succ = BB->getUniqueSuccessor();
  for (auto &U : Def->uses()) {
    Instruction * I = dyn_cast<Instruction>(U.getUser());
    if (!I)
      continue;

    if (I->getParent() == BB) {
      // same block usage, we cannot move after this BB.
      return true;
    }

    if (I->getParent() == Succ && isa<PHINode>(I))
      // TODO: move after the GCStatepointInst in BB if there exists
      return true;
  }
  return false;
}

static bool moveUnusedGEPAfterStatepoints(BasicBlock &BB, GCPtrLivenessData &Data) {
  // We only handle BB that has a single unique successor.
  // Otherwise, we may move a GEP into a loop.
  BasicBlock *Succ = BB.getUniqueSuccessor();
  if (Succ == nullptr)
    return false;

  SetVector<Value*> &LiveOut = Data.LiveOut[&BB];

  // We could iterate LiveOut instead of BB
  // However, LiveOut is not ordered. We must guarantee
  // reproducible compilation.
  for (Instruction &I : llvm::reverse(BB)) {
    Instruction *Def = &I;
    // only handle live-out pointers
    if (LiveOut.count(Def) == 0)
      continue;

    Value* Base = nullptr;
    if (BitCastInst *BCI = dyn_cast<BitCastInst>(Def))
      Base = BCI->getOperand(0);
    else if (GetElementPtrInst *GEP=dyn_cast<GetElementPtrInst>(Def))
      if (GEP->hasAllConstantIndices())
        Base = GEP->getPointerOperand();
      else
        continue;
    else
      continue;

    SmallVector<Instruction*, 4> Pointers;

    if (SameBlockUsage(Def))
      continue;

    // All usages have been checked
    Def->moveBefore(Succ->getFirstNonPHI());
    LiveOut.remove(Def);
    Data.LiveSet[&BB].remove(Def);
    Data.LiveIn[Succ].remove(Def);

    // We must add Base to LiveOut and LiveIn
    LiveOut.insert(Base);
    Data.LiveIn[Succ].insert(Base);
    return true;
  }

  return false;
}

// Unpack a pointer to base + offset
// Note that we already use isTrackedAlloca to find the Base.
// We may fail to successfully unpack a GEP. Use -1 to indicate this case.
static int getPtrOffsetFromBase(Value *Base, Value *Ptr, const DataLayout &DL) {
  Ptr = Ptr->stripPointerCasts();

  // If Ptr is a stack object, simpley return it.
  if (Base == Ptr) {
    return 0;
  }

  // If V is a GEP, unpack it.
  // TODO: we may need to handle more cases.
  if (auto *GEP = dyn_cast<GetElementPtrInst>(Ptr)) {
    // Use the same Base
    auto PtrOff = getPtrOffsetFromBase(Base, GEP->getPointerOperand(), DL);
    if (PtrOff == -1) {
      return -1;
    }

    APInt Off(DL.getPointerSizeInBits(0), 0);
    if (!GEP->accumulateConstantOffset(DL, Off)) {
      return -1;
    }

    return PtrOff + (unsigned)Off.getSExtValue();
  }

  // Not a derived pointer from Base
  return -1;
}

// compute StoredSize inited by typedmemmove and typedmemclr
static unsigned getTypedmemmoveSize(Value* V, const DataLayout& DL, DefiningValueMapTy &DVCache) {
  if (BitCastInst* BCI = dyn_cast<BitCastInst>(V)) {
    return getTypedmemmoveSize(BCI->getOperand(0), DL, DVCache);
  } else if (GetElementPtrInst* GEP = dyn_cast<GetElementPtrInst>(V)) {
    return DL.getTypeStoreSize(GEP->getResultElementType());
  } else if (Value* Base = isTrackedAlloca(V, DVCache)) {
    return DL.getTypeStoreSize(getValueElementType(Base));
  }
  return 0;
}

// Compute the initialization state of Alloca
// This could be done with the computeLiveInValues. But for simplicity,
// let's implement it stand-alone here.
//
// The Go frontend ensures that there is no bad load. Here, we only need to track
// the init-state of pointers. However, for simplicity we may track all bits but only
// bits of pointers matter.
//
// We leverage a Backward data flow analysis to find must-init bytes for each Alloca (with pointers).
// We use a BitVector to track the state of each Alloca. A bit is set if the bit must be initialized.
// Any USE of the Alloca requires that all necessary bits must be initialized.
// Any DEF will clear the bits that has been defined, which means these bits are not necessarily to be
// initialized before the DEF. When flow joins, we need to unify all bits.
//
// For an Alloca, the DEF can be StoreInst, MemSet/MemCpy/MemMove
// For an Alloca, the USE can be LoadInst or AddrTaken instructions such as
// CallInst/PHINode. For a live-out Alloca, it can also be used by the garbage collector.
// So any GCStatepointInst is a use of a live-out Alloca.
//
// This analysis depends on the result of findBasePointer.
static void computeAllocaDefState(DominatorTree &DT, LoopInfo &LI, Function &F,
                                GCPtrLivenessData &Data,
                                DenseMap<Value *, ToZeroInfo> &ToZero,
                                DefiningValueMapTy &DVCache) {

  const DataLayout &DL = F.getParent()->getDataLayout();
  const unsigned PtrSize = DL.getPointerSize();

  BasicBlock &Entry = F.getEntryBlock();
  bool HasAlloca = false;
  SmallVector<Instruction*, 4> ToBeMoved;
  for (Instruction &I: instructions(F)) {
    if (isa<AllocaInst>(&I) || isGoPreAllocated(&I)) {
      HasAlloca = true;
      if (I.getParent() != &Entry) {
        ToBeMoved.push_back(&I);
      }
    }
  }

  if (!HasAlloca)
    return;

  for (Instruction* I : ToBeMoved) {
    I->moveBefore(Entry.getFirstNonPHIOrDbg());
  }

  auto IsTrackedAllocaChecked = [&](Value *V) {
    if (isGoGCPointerOrPointerLikeValue(V))
      if (Value *Base = isTrackedAlloca(V, DVCache))
        return Base;
    return (Value*)nullptr;
  };

  // Return a deep copy that we can manipulate directly
  auto GetTypePtrMap = [&](Type* Ty) {
    // make a deep copy
    BitVector Ret;
    auto Found = Data.TypeToPtrMap.find(Ty);
    if (Found != Data.TypeToPtrMap.end()) {
      Ret = Found->second;
    } else {
      BitVector &BV = Data.TypeToPtrMap[Ty];
      uint64_t Size = DL.getTypeStoreSize(Ty);
      BV.resize(Size/PtrSize);
      getPtrBitmapForTypeHelper(Ty, DL, 0, BV);
      Ret = BV;
    }
    return Ret;
  };

  bool Debug = false;
  auto DumpAllocaStates = [&](DenseMap<Value*, BitVector> &States) {
    for (auto &It : States) {
      dbgs() << "Value: " << *It.first << "\n";
      auto TypePtrMap = GetTypePtrMap(getValueElementType(It.first));
      dbgs() << "TypePtrMap: ";
      for (auto Word : TypePtrMap.getData()) {
        dbgs() << llvm::format("%llX", Word);
      }
      dbgs() << "\n    PtrMap: ";
      for (auto Word : It.second.getData()) {
        dbgs() << llvm::format("%llX", Word);
      }
      dbgs() << "\n";
    }
  };

  // Return true if all bit in LHS are not set in RHS
  auto AllBitsUnset = [](BitVector &LHS, BitVector &RHS) {
    assert(RHS.size() == LHS.size() && "invariant");

    auto RHSData = RHS.getData();
    auto LHSData = LHS.getData();

    for (size_t i = 0; i < RHSData.size(); i++) {
      auto LHSBits = LHSData[i];
      auto RHSBits = RHSData[i];
      if ((RHSBits & LHSBits) != 0) {
        // Return false if any bits in common
        return false;
      }
    }

    return true;
  };

  auto AllBitsSet = [](BitVector &LHS, BitVector &RHS) {
    assert(RHS.size() == LHS.size() && "invariant");
    auto RHSData = RHS.getData();
    auto LHSData = LHS.getData();

    for (size_t i = 0; i < RHSData.size(); i++) {
      auto RHSBits = RHSData[i];
      auto LHSBits = LHSData[i];
      if ((RHSBits & ~LHSBits) != RHSBits) {
        return false;
      }
    }

    return true;
  };


  // At a Safepoint, all Alloca must be fully initialized
  auto ResetAtSafepoint = [&](DenseMap<Value*, BitVector> &States) {
    for (auto &It : States) {
      auto &S = It.second;
      auto TypePtrMap = GetTypePtrMap(getValueElementType(It.first));
      assert(TypePtrMap.size() == S.size() && "invariant");
      if (AllBitsUnset(TypePtrMap, S)) {
        // No bits in TypePtrMap are set, which means
        // even the object is live, all bits of the object will have
        // no use-before-def-after-this-bb.
        States.erase(It.first);
      } else {
        // true means at least one bit in TypePtrMap has been set in
        // The object must be live-out or live-through.
        // We must ensure all bits are set before this statepoint.
        S = std::move(TypePtrMap);
      }
    }
  };

  // Mark bits that are not necessarily to be initialized before this instruciton
  // because this instruction will initialize it and override old values.
  // Return true if States have changed.
  // We apply CheckDef only if we know the size and the offset.
  auto CheckDef = [&](Value *Base, Value *Ptr, unsigned Size, DenseMap<Value*, BitVector> &States) {
    // Ptr is the destination of a store
    int PtrOff = getPtrOffsetFromBase(Base, Ptr, DL);
    if (PtrOff == -1)
      return false; // We cannot determine the offset from the base.

    // For use we do not need to know the exact offset and size
    auto Found = States.find(Base);
    if (Found == States.end()) {
      // no use of this def
      // ignore
      return false;
    } else {
      auto &S = Found->second;
      bool Changed = false;
      // TODO: Size may be smaller than PtrSize.
      // In this case, the slot must not be poitner.
      // We still set the entire slot since ideally a pointer should not be partially updated.
      for (unsigned I = 0, Idx = (unsigned)PtrOff/PtrSize; I < Size; I += PtrSize, Idx++) {
        assert(Idx < S.size() && "invariant");
        if (S.test(Idx)) {
          S.reset(Idx);
          Changed = true;
        }
      }

      return Changed;
    }
  };

  // Mark all bits must be initialized
  auto CheckUse = [&](Value *Base, Value *Ptr, DenseMap<Value*, BitVector> &States) {
    // For use we do not need to know the exact offset and size.
    // TODO: we currently require that a Alloca must be fully initialized
    // before any REAL or AddrTaken use (e.g., load or Phi or Call).
    // For Load, we actually can precisely track which bytes are used.
    // But it seems that it is not worth doing this.
    auto Found = States.find(Base);
    auto TypePtrMap = GetTypePtrMap(getValueElementType(Base));
    if (Found == States.end()) {
      auto &S = States[Base];
      S = std::move(TypePtrMap);
      return true;
    } else {
      auto &S = Found->second;
      bool Changed = !AllBitsSet(TypePtrMap, S);
      S = std::move(TypePtrMap);
      return Changed;
    }
  };

  // We can only zero an Alloca once. If the Alloca is used in a Loop,
  // we should zero it before entering loop.
  auto FindNonLoopDom = [&](BasicBlock *BB) {
    if (BB == &F.getEntryBlock()) {
      return BB;
    }

    auto *Loop = LI.getLoopFor(BB);
    if (Loop == nullptr) {
      return BB;
    }

    // Iterate to find the outer-most loop
    for (; Loop->getParentLoop() != nullptr; Loop = Loop->getParentLoop()) {

    }

    // Preheader dominates everything in loop, including Header
    BasicBlock * Preheader = Loop->getLoopPreheader();
    if (Preheader != nullptr) {
      // dbgs() << "Use " << Preheader->getName() << " for " << BB->getName() << " depth=" << Loop->getLoopDepth() << "\n";
      assert(DT.dominates(Preheader, BB) && "loop header dominates everything in a loop");
      return Preheader;
    }
    return &F.getEntryBlock();
  };

  // We need to use IsTrackedAllocaChecked to compute
  // the dominator of all real usage of Alloca
  DenseMap<Value *, BasicBlock *> AllocaToDomUse;
  auto UpdateCommonDom = [&](Value* Base, BasicBlock *DomOfBB) {
    auto It = AllocaToDomUse.find(Base);
    if (It == AllocaToDomUse.end()) {
      // dbgs() << "Set DOM use for "  << *Base << " to " << BB.getName() << " reason=" << I << "\n";
      AllocaToDomUse[Base] = DomOfBB;
    } else if (It->second == DomOfBB) {
      // do nothing if they are the same.
      return;
    } else {
      // dbgs() << "Set DOM use for "  << *Base << " to " << BB.getName() << " reason=" << I << " old=" << It->second->getName() << "\n";
      It->second = DT.findNearestCommonDominator(It->second, DomOfBB);
      assert(It->second != nullptr && "invariant: all BB must be reachable from Entry");
    }
  };

  for (BasicBlock &BB : F) {
    BasicBlock *DomOfBB = FindNonLoopDom(&BB);
    for (Instruction &I : BB) {
      if (isa<BitCastInst>(I) || isa<GetElementPtrInst>(I)) {
        continue;
      }

      // PHI is also not a real use.
      // PHI: we could initialize the Alloca used in PHI after PHI
      for (Value *V : I.operands()) {
        Value *Base = IsTrackedAllocaChecked(V);
        if (Base != nullptr) {
          UpdateCommonDom(Base, DomOfBB);
        }
      }
    }
  }

  // ParamOut are used by all return st
  for (Value *ParamOut : Data.ParamOut) {
    if (!IsTrackedAllocaChecked(ParamOut))
      continue;

    for (BasicBlock &BB : F) {
      if (isa<ReturnInst>(BB.getTerminator())) {
        BasicBlock *DomOfBB = FindNonLoopDom(&BB);
        UpdateCommonDom(ParamOut, DomOfBB);
      }
    }
  }

  auto GetAllocaDomUse = [&AllocaToDomUse](Value *Base) {
    auto It = AllocaToDomUse.find(Base);
    if (It == AllocaToDomUse.end()) {
      return (BasicBlock*)nullptr;
    }
    return It->second;
  };

  SmallSetVector<BasicBlock *, 32> Worklist;

  for (BasicBlock &BB : F) {
    // Lets try all BB first.
    Worklist.insert(&BB);
    if (isa<ReturnInst>(BB.getTerminator()) && !Data.ParamOut.empty()) {
      DenseMap<Value*, BitVector> &States = Data.OutAllocaDefStates[&BB];
      // On-stack ParamOut
      for (Value *ParamOut : Data.ParamOut) {
        if (Value *Base = IsTrackedAllocaChecked(ParamOut))
          CheckUse(Base, ParamOut, States);
      }
    }
  }


  // TODO: Use worklist instead of a simple big loop
  while (!Worklist.empty()) {
    BasicBlock *BB = Worklist.pop_back_val();

    // We maintain an OutStates for PHI.
    // TODO: we actually can pull effects of each PHI from successor.
    // Then, we can start with an empty states and simply pull changes from successor's InStates
    DenseMap<Value*, BitVector> &OutStates = Data.OutAllocaDefStates[BB];

    // Join of all successors and update OutStates in-place
    for (BasicBlock *Succ : successors(BB)) {
      auto &SuccStates = Data.InAllocaDefStates[Succ];
      for (auto &It : SuccStates) {
        auto Found = OutStates.find(It.first);
        if (Found == OutStates.end()) {
          auto &S = OutStates[It.first];
          // deep copy
          S = It.second;
        } else {
          // Bits union
          Found->second |= It.second;
        }
      }
    }

    // Start with Out states
    DenseMap<Value*, BitVector> States = OutStates;

    if (Debug) {
      dbgs() << "Dump OutStates for " << BB->getName() << "\n";
      DumpAllocaStates(States);
    }

    // Backward propagation
    for (Instruction &I : llvm::reverse(*BB)) {

      // PHINode must be handled differently because
      // an incoming value only needs to be live in the corresponding incoming block.
      if (auto *PN = dyn_cast<PHINode>(&I)) {
        if (isGoGCPointerOrPointerLikeValue(PN)) {
          unsigned NumPHIValues = PN->getNumIncomingValues();
          for (unsigned N = 0; N < NumPHIValues; N++) {
            Value *InVal = PN->getIncomingValue(N);
            if (Value *Base = IsTrackedAllocaChecked(InVal)) {
              BasicBlock *InBB = PN->getIncomingBlock(N);
              DenseMap<Value*, BitVector> &InOutStates = Data.OutAllocaDefStates[InBB];
              // Phi is also a use f InVal.
              // Note that we do not update the InStates of BB.
              if (BB == GetAllocaDomUse(Base)) {
                // Phi is the only use of Base
                if (!ToZero.count(Base)) {
                  // We should insert a zeroing here.
                  ToZeroInfo Info;
                  Info.State = GetTypePtrMap(getValueElementType(Base));
                  if (InBB == &Entry) {
                    // Zeroing after Alloca
                    Info.Pos = nullptr;
                  } else {
                    // Zeroing after the first NonPHI in this block
                    Info.Pos = InBB;
                  }
                  ToZero[Base] = Info;
                  if (PrintToZero)
                    dbgs() << "Add zeroing in " << BB->getName() << " for " << *Base << " in " << F.getName() << "\n";
                }
                // do not add Base into InOutStates
              } else if (CheckUse(Base, InVal, InOutStates)) {
                Worklist.insert(InBB);
              }
            }
          }
        }
        continue;
      }

      if (isa<BitCastInst>(I) || isa<GetElementPtrInst>(I)) {
        // isTrackedAlloca can remove BitCast
        continue;
      }

      // Handle Def first, it could be Store and MemIntrinsics
      if (StoreInst *SI = dyn_cast<StoreInst>(&I)) {
        // Process DEF
        Value *Ptr = SI->getPointerOperand();
        Value *PtrBase = IsTrackedAllocaChecked(Ptr);
        if (PtrBase) {
          CheckDef(PtrBase, Ptr, DL.getTypeStoreSize(SI->getValueOperand()->getType()), States);
        }

        // Process USE
        Value *Val = SI->getValueOperand();
        if (Value *ValBase = IsTrackedAllocaChecked(Val)) {
          // NOTE: This case is tricky. An addr is not a real usage of the Alloca.
          // We can safely use the address of an Alloca during initializing the same Alloca.
          // See strings.Join and strings.Builder
          if (ValBase != PtrBase) {
            CheckUse(ValBase, Val, States);
          } else {
            // Ignore update the use of the same Alloca
            // We can ignore because Val must a BitCast or a GE
            // that is not a real use of the Alloca.
          }
        }

        continue;
      } else if (CallInst *CI = dyn_cast<CallInst>(&I)) {

        // We need to mark all live-out Alloca fully initialized.
        // This reset also includes those initialized by CI.
        if (needsRewriteForGo(CI)) {
          ResetAtSafepoint(States);
        }

        // Make control flow clean. Use a bool variable to track whether
        // we need fall-through to handle all operands as Use.
        bool OperandAdded = false;
        if (Function *Fn = CI->getCalledFunction()) {
          switch (Fn->getIntrinsicID()) {
          case Intrinsic::go_metadata:
            break;
          case Intrinsic::memmove:
          case Intrinsic::memcpy:
          case Intrinsic::memcpy_inline:
          case Intrinsic::memset: {
            Value *Ptr = CI->getArgOperand(0);
            if (Value *Base = IsTrackedAllocaChecked(Ptr)) {
              ConstantInt *Len = dyn_cast<ConstantInt>(cast<AnyMemIntrinsic>(CI)->getLength());
              if (Len) {
                CheckDef(Base, Ptr, (unsigned)Len->getZExtValue(), States);
              }
            }
            if (auto *MT = dyn_cast<AnyMemTransferInst>(CI)) {
              Value *Src = MT->getRawSource();
              if (Value *Base = IsTrackedAllocaChecked(Src)) {
                CheckUse(Base, Src, States);
              }
            } else if (auto *MS = dyn_cast<AnyMemSetInst>(CI)) {
              Value *Val = MS->getValue();
              if (Value *Base = IsTrackedAllocaChecked(Val)) {
                CheckUse(Base, Val, States);
              }
            }
            // Avoid fall-through
            OperandAdded = true;
            break;
          }
          default:
            if (Fn->getName() == "runtime.typedmemmove" || Fn->getName() == "runtime.typedmemclr") {
              // Dst is the second argument. The first argument is type descriptor.
              Value *Ptr = CI->getArgOperand(1);
              if (Value *Base = IsTrackedAllocaChecked(Ptr)) {
                // The typedmemmove should take a known type descriptor in most cases.
                // We cannot trust the size of getTypedmemmoveSize.
                // If we cannot get the size, we may ignore this def and trigger an
                // potential zeroing.
                unsigned Size = getTypedmemmoveSize(Ptr, DL, DVCache);
                if (Size) {
                  CheckDef(Base, Ptr, Size, States);
                }
              }

              if (Fn->getName() == "runtime.typedmemmove") {
                Value *Src = CI->getArgOperand(2);
                if (Value *Base = IsTrackedAllocaChecked(Src)) {
                  CheckUse(Base, Src, States);
                }
              }

              OperandAdded = true;
              break;
            } else if (Fn->getName() == "runtime.memmove") {
              // Dst is the first argument
              Value *Ptr = CI->getArgOperand(0);
              if (Value *Base = IsTrackedAllocaChecked(Ptr)) {
                ConstantInt *Len = dyn_cast<ConstantInt>(CI->getArgOperand(2));
                if (Len) {
                  CheckDef(Base, Ptr, (unsigned)Len->getZExtValue(), States);
                }
              }

              Value *Src = CI->getArgOperand(1);
              if (Value *Base = IsTrackedAllocaChecked(Src)) {
                CheckUse(Base, Src, States);
              }

              // continue to handle next instruction;
              OperandAdded = true;
              break;
            } // fall-through
          } // fall-through
        } // fall-through

        if (OperandAdded)
          continue;

        // fall-through to handle all operands as Use.

      } // fall-through

      // For all other instructions, including unhandled CallInst
      for (Value *V : I.operands()) {
        if (isGoGCPointerOrPointerLikeValue(V))
          if (Value *Base = IsTrackedAllocaChecked(V)) {
            CheckUse(Base, V, States);
          }
      }
    }

    // Remove all fully initialized objects since
    // we could insert a VarDef here.
    for (auto &It : States) {
      auto &S = It.second;
      auto TypePtrMap = GetTypePtrMap(getValueElementType(It.first));
      if (AllBitsUnset(TypePtrMap, S)) {
        // No bits in TypePtrMap should be initialized, which means
        // even the object is live, all bits of the object will have
        // no use-before-def-after-this-bb.
        States.erase(It.first);
      } else if (BB == GetAllocaDomUse(It.first)) {
        if (!ToZero.count(It.first)) {
          // We should insert a zeroing here.
          ToZeroInfo Info;
          Info.State = S;
          if (BB == &Entry) {
            // Zeroing after Alloca
            Info.Pos = nullptr;
          } else {
            // Zeroing after the first NonPHI in this block
            Info.Pos = BB;
          }
          ToZero[It.first] = Info;
          if (PrintToZero)
            dbgs() << "Add zeroing in " << BB->getName() << " for " << *It.first << " in " << F.getName() << "\n";
        }
        // After zeroing we can safely remove It.first
        States.erase(It.first);
      }
    }

    auto &InStates = Data.InAllocaDefStates[BB];

    if (Debug) {
      dbgs() << "Dump Old IntStates for " << BB->getName() << "\n";
      DumpAllocaStates(InStates);
    }

    // Make sure operator!= works
    if (InStates != States) {

      if (Debug) {
        dbgs() << "Dump New IntStates for " << BB->getName() << "\n";
        DumpAllocaStates(States);
      }
      // make a deep copy
      InStates = States;
      Worklist.insert(pred_begin(BB), pred_end(BB));
    }
  }


  // Sanity Check
  // Make sure all Alloca is at the Entry block
  // TODO: we could add Alloca at any basic block.
  // Also, we should not handle lifetime.start and lifetime.end
  // 3. Ensure no Alloca is used in GEP indices.
  DenseMap<Value*, BitVector> &EntryStates = Data.InAllocaDefStates[&Entry];
  for (BasicBlock &BB : F) {
    for (Instruction &I : BB) {
      if (isa<AllocaInst>(&I) || isGoPreAllocated(&I)) {
        if (&BB != &Entry) {
          // Remove the sanity check if we allocate stack objects in other basic blocks.
#if !defined(NDEBUG) || defined(LLVM_ENABLE_DUMP)
          F.dump();
#endif
          dbgs() << "stack objects must be declared in entry block: " << I << "\n";
          report_fatal_error("stack objects must be declared in entry block");
        }
        auto It = EntryStates.find(&I);
        if (It != EntryStates.end()) {
          auto &S = It->second;
          auto TypePtrMap = GetTypePtrMap(getValueElementType(It->first));
          if (!AllBitsUnset(TypePtrMap, S)) {
            if (!ToZero.count(It->first) && GetAllocaDomUse(It->first) != nullptr) {
#if !defined(NDEBUG) || defined(LLVM_ENABLE_DUMP)
              F.dump();
#endif
              dbgs() << "must be added to ToZero: " << *It->first << "\n";
              report_fatal_error("must be added to ToZero");
            }
          }
        }
        // Force all alwayslive alloca to be zeroed right after the alloca
        if (I.hasMetadata("go.alwayslive")) {
          if (IsTrackedAllocaChecked(&I)) {
            if (isGoPreAllocated(&I) && !isGoPreAllocatedParamOut(&I))
              report_fatal_error("cannot zero ParamIn");
            auto TypePtrMap = GetTypePtrMap(getValueElementType(&I));
            ToZeroInfo Info;
            Info.State = TypePtrMap;
            Info.Pos = nullptr;
            ToZero[&I] = Info;
          }
        }
      } else if (IntrinsicInst *II = dyn_cast<IntrinsicInst>(&I)) {
        // Remove the following sanity check if we need lifetime_start
        switch (II->getIntrinsicID()) {
          case Intrinsic::lifetime_start:
          case Intrinsic::lifetime_end:
            report_fatal_error("lifetime intrinsics are not handled");
        }
      } else if (GetElementPtrInst *GEP = dyn_cast<GetElementPtrInst>(&I)) {
        for (Use &Idx : GEP->indices()) {
          if (IsTrackedAllocaChecked(Idx)) {
            report_fatal_error("address of alloca is used as GEP indice");
          }
        }
      }
    }
  }

#if 0
  if (ToZero.empty())
    return;

  // Compute AllocaDef in each BB
  F.dump();
  for (BasicBlock &BB : F) {
    auto &InStates = Data.InAllocaDefStates[&BB];
    dbgs() << "Dump at " << BB.getName() << "\n";
    DumpAllocaStates(InStates);
  }
#endif
}

static void computeLiveInValues(DominatorTree &DT, LoopInfo &LI, Function &F,
                                GCPtrLivenessData &Data,
                                DenseMap<Value *, ToZeroInfo> &ToZero,
                                DefiningValueMapTy &DVCache) {
  // collect all parameter out
  for (BasicBlock &BB : F) {
    for (Instruction &I : BB) {
      if (I.hasMetadata("go.alwayslive"))
        Data.AlwaysLive.insert(&I);
      if (isGoPreAllocatedParamOut(&I)) {
        Data.ParamOut.insert(&I);
      }
    }
  }

  if (PrintLiveSet) {
    dbgs() << "ParamOutAllocas:\n";
    for (auto V : Data.ParamOut) {
      dbgs() << "\t" << *V << "\n";
    }
  }

  // collect all pointer like values,
  // including those derived from PtrToInt
  checkGoPointerLikeValues(F);

  computeAllocaDefState(DT, LI, F, Data, ToZero, DVCache);

  // Seed the liveness for each individual block
  for (BasicBlock &BB : F) {
    Data.KillSet[&BB] = computeKillSet(&BB, DVCache);
    Data.LiveSet[&BB].clear();
    computeLiveInValues(BB.rbegin(), BB.rend(), Data.LiveSet[&BB], DVCache, Data.GenAtInst);
    Data.LiveOut[&BB] = SetVector<Value *>();
    computeLiveOutSeed(&BB, Data.LiveOut[&BB], DVCache);

#ifndef NDEBUG
    for (Value *Kill : Data.KillSet[&BB])
      assert(!Data.LiveSet[&BB].count(Kill) && "live set contains kill");
#endif
  }

  SmallSetVector<BasicBlock *, 32> Worklist;
  for (BasicBlock &BB : F) {
    if (isa<ReturnInst>(BB.getTerminator()) && !Data.ParamOut.empty()) {
      Data.LiveOut[&BB].set_union(Data.ParamOut);
    }
    Data.LiveIn[&BB] = Data.LiveSet[&BB];
    Data.LiveIn[&BB].set_union(Data.LiveOut[&BB]);
    Data.LiveIn[&BB].set_subtract(Data.KillSet[&BB]);
    if (!Data.LiveIn[&BB].empty())
      Worklist.insert(pred_begin(&BB), pred_end(&BB));
  }

  // Propagate liveness until stable
  while (!Worklist.empty()) {
    BasicBlock *BB = Worklist.pop_back_val();

    // Compute our new liveout set, then exit early if it hasn't changed despite
    // the contribution of our successor.
    SetVector<Value *> LiveOut = Data.LiveOut[BB];
    const auto OldLiveOutSize = LiveOut.size();
    for (BasicBlock *Succ : successors(BB)) {
      assert(Data.LiveIn.count(Succ));
      LiveOut.set_union(Data.LiveIn[Succ]);
    }
    // assert OutLiveOut is a subset of LiveOut
    if (OldLiveOutSize == LiveOut.size()) {
      // If the sets are the same size, then we didn't actually add anything
      // when unioning our successors LiveIn.  Thus, the LiveIn of this block
      // hasn't changed.
      continue;
    }
    Data.LiveOut[BB] = LiveOut;

    // Apply the effects of this basic block
    SetVector<Value *> LiveTmp = LiveOut;
    LiveTmp.set_union(Data.LiveSet[BB]);
    LiveTmp.set_subtract(Data.KillSet[BB]);

    assert(Data.LiveIn.count(BB));
    const SetVector<Value *> &OldLiveIn = Data.LiveIn[BB];
    // assert: OldLiveIn is a subset of LiveTmp
    if (OldLiveIn.size() != LiveTmp.size()) {
      Data.LiveIn[BB] = LiveTmp;
      Worklist.insert(pred_begin(BB), pred_end(BB));
    }
  } // while (!Worklist.empty())

  {
    // TODO: collect all Alloca that has pointers
    LLVMContext& Context = F.getContext();
    Metadata *Zero = ConstantAsMetadata::get(ConstantInt::get(Type::getInt64Ty(Context), 0));
    Metadata *Empty = MDString::get(Context, "go.empty");
    MDNode *MD = MDNode::get(Context, {Zero, Zero, Empty});
    for (Instruction &I : instructions(F)) {
      AllocaInst *AI = dyn_cast<AllocaInst>(&I);
      if (!AI)
        continue;

      if (!hasPointerTy(AI->getAllocatedType()))
        continue;

      if (AI->hasMetadata("go.stackobject.gcdata"))
        continue;

      AI->setMetadata("go.stackobject.gcdata", MD);
    }
  }

  auto StackObjectNotDefAll = [&](Value *V, BasicBlock *BB){
    // if (!isGoGCPointerOrPointerLikeValue(V))
    //   return false;
    if (Value *Base = isTrackedAlloca(V, DVCache)) {
      if (!Data.OutAllocaDefStates[BB].count(Base)) {
        // dbgs() << "!!! alloca live but not initialized:\n\t" <<
        //          F.getName() << "  " << BB.getName() << "\n" << *V << "\n";
        return true;
      }
    }
    return false;
  };

  if (MoveGEPBackward) {
    ReversePostOrderTraversal<Function *> RPOT(&F);
    // for performance: we can avoid save GEP too early and we only
    // move GEP following unconditional branches.
    for (BasicBlock *BB : RPOT) {
      // until fixed point
      while (true) {
        if (!moveUnusedGEPAfterStatepoints(*BB, Data))
          break;
      }
    }
  }

  // After Liveout is finalized, we can compute live-in values at each CallInst.
  for (BasicBlock &BB : F) {
    // The type of an Alloca is not changed even its lifetime can be changed by
    // intrinsic lifetime_start and lifetime_end. That means once an Alloca has
    // been defined, it is parsable. We only need to track all Defs in forward analysis.
    //
    // One case may happen when an Alloca is a struct and is initialized element-wise.
    // That means we cannot easily identify all Defs of Alloca using the following checking,
    // i.e., only check Store and MemMove/MemSet/MemCpy/MemCpyInline. Nevertheless, previous ToZero will
    // identify such Alloca as uninitialized and zero them.
    //
    // Here we only need to handle Alloca as all Preallocated should already be initialized.
    // TODO: we split the basic block at each CallInst. The terminator should have no side effect.
    // That means, we actually can use the LiveOut of the split basic block for the CallInst.
    SetVector<Value*> LiveTmp = Data.LiveOut[&BB];
    for (auto &I : make_range(BB.rbegin(), BB.rend())) {
      // KILL/Def - Remove this definition from LiveIn
      LiveTmp.remove(&I);

      //  LiveAtInst when CallInst is invoking
      // See NeedsRewrite to filter out irrelevant CallInst
      if (CallInst *Call = dyn_cast<CallInst>(&I)) {
        if (!isa<GCStatepointInst>(Call) && hasStackMap(Call)) {
          LiveTmp.remove_if([&](Value *V) {
              return StackObjectNotDefAll(V, &BB);
              });
          Data.LiveAtInst[&I] = LiveTmp;
          Data.LiveAtInst[&I].set_union(Data.AlwaysLive);
          // TODO: Very important!!!
          // A basic block should have only a single state point.
          break;
        }
      }

      SetVector<Value*> &Gen = Data.GenAtInst[&I];
      LiveTmp.insert(Gen.begin(), Gen.end());

      // no CallInst before PHINode
      if (isa<PHINode>(I))
        break;
    }
  }

#ifndef NDEBUG
  // Sanity check our output against SSA properties.  This helps catch any
  // missing kills during the above iteration.
  for (BasicBlock &BB : F)
    checkBasicSSA(DT, Data, BB);
#endif
}

// Compute the set of values live at Inst, store the result in Out.
//
// Side effect: in clobber-non-live mode, the clobbering instructions
// are inserted here.
static void findLiveSetAtInst(Instruction *Inst, GCPtrLivenessData &Data,
                              StatepointLiveSetTy &Out,
                              SetVector<Value *> &AllAllocas,
                              DefiningValueMapTy &DVCache) {
  // We have put everything into Data.LiveAtInst
  assert(Data.LiveAtInst.count(Inst) && "must have data");
  SetVector<Value*> &LiveAt = Data.LiveAtInst[Inst];
  Out.insert(LiveAt.begin(), LiveAt.end());
}
