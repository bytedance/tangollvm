//===--------------------------- GoROData.cpp -----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 ByteDance Ltd. and/or its affiliates.
//===----------------------------------------------------------------------===//
// Eliminate load from RO data in Go
//
//===----------------------------------------------------------------------===//

#include "llvm/Analysis/InstructionSimplify.h"
#include "llvm/Transforms/IPO/GoROData.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/Local.h"
#include "llvm/ADT/Hashing.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/TangoLLVM.h"
#include "llvm/Support/Alignment.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
using namespace llvm;


#define DEBUG_TYPE "gorodata"

STATISTIC(NumSpecializedFunc, "Number of specialized functions");
STATISTIC(NumSpecializedCall, "Number of call instructions");

// Enable Go RO data optimization
static cl::opt<bool> EnableGoROData("go-rodata", cl::Hidden,
                                  cl::init(true));

static cl::opt<bool> EnableGoSpecialization("go-specialization", cl::Hidden,
                                  cl::init(true));

static cl::opt<uint32_t> GoMaxSpecInlineSize("go-max-spec-inline-size", cl::Hidden,
                                  cl::init(2500));




static bool replaceLoadROData(Function &F);
static bool runOnFunction(Function &F, bool specialize, bool useinline);

static bool intrinsifyGoBuiltins(Function &F) {
  SmallVector<CallInst*, 4> MemClrs;

  for (Instruction &I : instructions(F)) {
    CallInst *CI = dyn_cast<CallInst>(&I);
    if (!CI)
      continue;

    Function *Callee = CI->getCalledFunction();
    if (!Callee)
      continue;

    StringRef Name = Callee->getName();
    if (Name == "runtime.memclrNoHeapPointers") {
      assert(CI->arg_size() == 2 && "invariant");
      if (auto * C = dyn_cast<ConstantInt>(CI->getArgOperand(1))) {
        if (C->getZExtValue() < 128) { // TODO: hard coded threshold here
          MemClrs.push_back(CI);
        }
      }
    }
  }

  if (MemClrs.empty())
    return false;


  for (auto *CI : MemClrs) {
    IRBuilder<> Builder(CI);
    const Align ArgAlign(1);
    Builder.CreateMemSet(
        CI->getArgOperand(0),
        ConstantInt::get(Type::getInt8Ty(CI->getContext()), 0, false),
        cast<ConstantInt>(CI->getArgOperand(1))->getZExtValue(),
        ArgAlign,
        false);

    CI->eraseFromParent();
  }

  return true;
}

static bool isSupportedArg(const Value *V) {
  if (isa<ConstantInt>(V) || isa<ConstantFP>(V))
    return true;
  if (auto *F = dyn_cast<Function>(V))
    return !F->isDeclaration();
  if (auto *G = dyn_cast<GlobalVariable>(V))
    return G->hasMetadata("go.rodata");

  return false;
}

static CallInst *specializeCall(Function &F, CallInst *CI) {
  Function* Callee = CI->getCalledFunction();
  if (Callee->hasMetadata("go.specialized"))
    return nullptr;

  assert(Callee != nullptr && "invariant");

  // "CalleeName,,,{ArgNo,ArgVal},,,specialized"
  std::string SpecName;
  SpecName += Callee->getName();
  SpecName += ",,,";

  // Copy necessary attributes
  for (unsigned I = 0, E = Callee->arg_size(); I < E; I++) {
    Value *Arg = CI->getArgOperand(I);
    if (!isSupportedArg(Arg)) {
      continue;
    }
    SpecName += std::to_string(I);
    SpecName += ",";

    if (ConstantInt *C = dyn_cast<ConstantInt>(Arg)) {
      SmallString<40> S;
      C->getValue().toString(S, 10, false);
      SpecName += S;
    } else if (ConstantFP *C = dyn_cast<ConstantFP>(Arg)) {
      SmallString<40> S;
      C->getValue().toString(S);
      SpecName += S;
    } else {
      SpecName += Arg->getName();
    }
    SpecName += ",";
  }
  // Mark this function is a specialized version in the object file
  SpecName += ",,specialized";

  Module *M = F.getParent();
  Function * NewCallee = M->getFunction(SpecName);
  if (NewCallee == nullptr) {
    // We must create a function with the same signature
    // even we know some arguments must be constants.
    // This is because some Go ABI related metadata are created
    // based on the signature. We do not want to compute the ABI
    // in LLVM before we have implemented full Go ABI in LLVM.
    NewCallee = Function::Create(Callee->getFunctionType(),
        Callee->getLinkage(), SpecName, *M);

    ValueToValueMapTy VMap;
    for (unsigned I = 0, E = Callee->arg_size(); I < E; I++) {
      Value *Arg = CI->getArgOperand(I);
      if (!isSupportedArg(Arg)) {
        VMap[Callee->getArg(I)] = NewCallee->getArg(I);
        continue;
      }
      assert(isa<Constant>(Arg) && "invariant: only constant can be shared");
      VMap[Callee->getArg(I)] = Arg;
    }
    SmallVector<ReturnInst *, 8> Returns;
    CloneFunctionInto(NewCallee, Callee, VMap, CloneFunctionChangeType::LocalChangesOnly,
        Returns);

    NewCallee->setMetadata("go.specialized", MDNode::get(Callee->getContext(), {}));

    runOnFunction(*NewCallee, true, true);

    ++NumSpecializedFunc;
  }

  if (Callee == NewCallee)
    return nullptr;

  SmallVector<Value*, 8> Args(CI->arg_begin(), CI->arg_end());
  CallInst *NewCI = CallInst::Create(NewCallee, Args);
  NewCI->setCallingConv(CI->getCallingConv());
  NewCI->copyMetadata(*CI);
  NewCI->setAttributes(CI->getAttributes());
  return NewCI;
}

static bool specializeGoBuiltins(Function &F, bool UseInline) {
  if (!EnableGoSpecialization)
    return false;

  SmallVector<CallInst*, 16> Targets;

  // If specialization is too aggressive, we
  // will result in excessively large binaries.
  for (Instruction &I : instructions(F)) {
    CallInst *CI = dyn_cast<CallInst>(&I);
    if (!CI)
      continue;

    if (CI->arg_empty())
      continue;

    Function *Callee = CI->getCalledFunction();
    if (!Callee)
      continue;

    if (Callee->isDeclaration() || Callee->isIntrinsic())
      continue;

    if (Callee->hasMetadata("go.specialized"))
      continue;

    // hard coded heuristic
    if (Callee->getName() == "runtime.heapBitsSetType") {
      Value *ArgSize = CI->getArgOperand(1);
      Value *ArgType = CI->getArgOperand(3);
      if (isSupportedArg(ArgSize) && isSupportedArg(ArgType)) {
        Targets.push_back(CI);
      }
      continue;
    }

    // if (Callee->hasFnAttribute(Attribute::NoInline))
    //   continue;
    auto MD = Callee->getMetadata("go.spec.cond");
    if (MD == nullptr) {
      continue;
    }

    uint64_t ArgPos = dyn_cast<ConstantAsMetadata>(MD->getOperand(0))
      ->getValue()->getUniqueInteger().getZExtValue();

    if (ArgPos != 0) {
      // See llvm.go
      // All configured targets have the first args.
      // ArgPos is a bitmap if we have more args in the future
      continue;
    }

    if (!isSupportedArg(CI->getArgOperand(ArgPos)))
      continue;

    Targets.push_back(CI);
  }

  if (Targets.empty())
    return false;

  bool Changed = false;
  for (CallInst *CI : Targets) {
    LLVM_DEBUG(dbgs() << "GoROData: " << F.getName() << ": try to specialize a call " << *CI << "\n");

    Function *Callee = CI->getCalledFunction();
    bool DoInline = UseInline && !Callee->hasFnAttribute(Attribute::NoInline)
      && F.getInstructionCount() < GoMaxSpecInlineSize;
    if (DoInline) {
      InlineFunctionInfo FnInfo;
      auto InlineRes = InlineFunction(*CI, FnInfo);
      if (InlineRes.isSuccess()) {
        Changed = true;
        LLVM_DEBUG(dbgs() << "GoROData: " << F.getName() << ": inline a  specialize function " << Callee->getName() << "\n");
        TangoLLVM::updateMetadataForInlining(Callee, &F);
      }
    } else {
      CallInst *NewCI = specializeCall(F, CI);
      if (NewCI != nullptr) {
        Changed = true;
        NewCI->insertBefore(CI);
        CI->replaceAllUsesWith(NewCI);
        CI->eraseFromParent();
        ++NumSpecializedCall;
        LLVM_DEBUG(dbgs() << "GoROData: " << F.getName() << ": create a specialize a call " << *NewCI << "\n");
      }
    }
  }

  return Changed;
}

static bool replaceLoadROData(Function &F) {
  bool Changed = false;

  const DataLayout &DL = F.getParent()->getDataLayout();

  SmallSetVector<LoadInst*, 32> Worklist;
  for (Instruction &I : instructions(F)) {
    LoadInst *LI = dyn_cast<LoadInst>(&I);
    if (LI)
      Worklist.insert(LI);
  }

  while (!Worklist.empty()) {
    LoadInst *LI = Worklist.pop_back_val();

    Constant *NewValue = TangoLLVM::resolveLoadROData(LI, DL);
    if (NewValue != nullptr) {
      LLVM_DEBUG(dbgs() << "GoROData: " << F.getName() << ": replace" << *LI
          << " by constant value ";
          if (NewValue->hasName()) { dbgs() << NewValue->getName(); } else { dbgs() <<  *NewValue; }
          dbgs() << "\n");

      for (auto &U : LI->uses()) {
        LoadInst *NewLI = dyn_cast<LoadInst>(U.getUser());
        if (NewLI == nullptr)
          continue;
        Worklist.insert(NewLI);
      }

      LI->replaceAllUsesWith(NewValue);
      LI->eraseFromParent();
      Changed = true;
    }
  }

  if (Changed) {
    for (BasicBlock &BB : F)
      SimplifyInstructionsInBlock(&BB);
  }

  return Changed;
}

static bool runOnFunction(Function &F, bool Specialize, bool UseInline) {
  bool Changed = false;
  while (true) {
    bool IterChanged = replaceLoadROData(F);
    IterChanged |= intrinsifyGoBuiltins(F);
    if (Specialize && specializeGoBuiltins(F, UseInline)) {
      IterChanged = true;
    }
    if (!IterChanged) {
      break;
    }
    Changed = true;
  }
  return Changed;
}

PreservedAnalyses GoRODataPass::run(Module &M, ModuleAnalysisManager &) {
  if (!EnableGoROData)
    return PreservedAnalyses::all();

  bool Changed = false;

  for (Function &F : M) {
    if (F.isDeclaration())
      continue;

    if (F.isIntrinsic())
      continue;

    Changed |= runOnFunction(F, Options.Specialize, false);
  }

  if (!Changed)
    return PreservedAnalyses::all();

  return PreservedAnalyses::none() ;
}

struct PtrAndOff {
  GlobalVariable *GV;
  uint64_t Offset;
};

static PtrAndOff findBasePtrAndOff(Value *V, const DataLayout &DL);

static PtrAndOff findBasePtrAndOffGEP(GEPOperator *GEP, const DataLayout &DL) {
    unsigned BitWidth = DL.getIndexTypeSizeInBits(GEP->getType());
    APInt GEPOffset(BitWidth, 0);
    if (!GEP->accumulateConstantOffset(DL, GEPOffset)) {
      return {};
    }
    auto PO = findBasePtrAndOff(GEP->getPointerOperand(), DL);
    return {PO.GV, PO.Offset + GEPOffset.getZExtValue()};
}

static PtrAndOff findBasePtrAndOff(Value *V, const DataLayout &DL) {
  if (auto *GEP = dyn_cast<GetElementPtrInst>(V)) {
    return findBasePtrAndOffGEP(cast<GEPOperator>(GEP), DL);
  }

  if (auto *CE = dyn_cast<ConstantExpr>(V)) {
    if (CE->getOpcode() == Instruction::GetElementPtr) {
      auto *GEP = cast<GEPOperator>(CE);
      return findBasePtrAndOffGEP(GEP, DL);
    }
    return {};
  }

  if (auto *BC = dyn_cast<BitCastInst>(V)) {
    return findBasePtrAndOff(BC->getOperand(0), DL);
  }

  if (auto *GV = dyn_cast<GlobalVariable>(V)) {
    return {GV, 0};
  }

  return {};
}

Constant *TangoLLVM::resolveLoadROData(LoadInst *LI, const DataLayout &DL) {
  auto PO = findBasePtrAndOff(LI->getPointerOperand(), DL);
  GlobalVariable *GV = PO.GV;
  if (GV == nullptr) {
    return nullptr;
  }

  uint64_t Offset = PO.Offset;
  auto *ROData = GV->getMetadata("go.rodata");
  if (!ROData) {
    return nullptr;
  }

  const unsigned PtrSizeInBits = DL.getPointerSizeInBits();
  assert(PtrSizeInBits == 64 && "64bit only");

  Type *ValType = LI->getType();
  // TODO: ValSize could be zero in LLVM but non-zero in Go, e.g., empty struct
  auto ValSize = DL.getTypeStoreSize(ValType);

  StringRef Data = cast<MDString>(ROData->getOperand(0))->getString();
  // TODO: load from all zero data is zero, even the offset is out of bounds
  if (Offset >= Data.size() || (Offset + ValSize) > Data.size()) {
    if (GV->hasMetadata("go.zerorodata")) {
      return Constant::getNullValue(ValType);
    }
    return nullptr;
  }

  if (ROData->getNumOperands() == 2) {
    MDNode *Reloc = cast<MDNode>(ROData->getOperand(1));

    // Function *F = LI->getParent()->getParent();
    // dbgs() << F->getName() << " GlobalVariable: "
    //   << *LI << ", GV="
    //   << *GV << ", Reloc=" << *Reloc << "\n";
    for (unsigned i = 0; i < Reloc->getNumOperands(); i += 5) {
      uint64_t Off = dyn_cast<ConstantAsMetadata>(Reloc->getOperand(i + 1))
        ->getValue()->getUniqueInteger().getZExtValue();
      uint64_t Siz = dyn_cast<ConstantAsMetadata>(Reloc->getOperand(i + 2))
        ->getValue()->getUniqueInteger().getZExtValue();

      // We must stop constant replacement if the Offset+ValSize has overlap with the Reloc
      if (Off < Offset) {
        if (Off + Siz > Offset)
          return nullptr; // overlap
        continue;
      }

      if (Off > Offset) {
        if (Offset + ValSize > Off)
          return nullptr; // overlap
        break;
      }

      uint64_t enabled = dyn_cast<ConstantAsMetadata>(Reloc->getOperand(i + 0))
        ->getValue()->getUniqueInteger().getZExtValue();
      if (enabled == 0)
        return nullptr; // unknown relocation type or invalid offset

      // Now, we can only return a pointer or pointer-like integer
      if (!(ValType->isPointerTy() || ValType->isIntegerTy(PtrSizeInBits)))
        return nullptr;

      // Reloc.Siz must be PtrSize
      if (Siz != PtrSizeInBits/8)
        return nullptr;

      if (Reloc->getOperand(i + 4) == nullptr)
        return nullptr; // constant symbol is dead and removed by GlobalOpt

      Value *Ptr = dyn_cast<ConstantAsMetadata>(Reloc->getOperand(i + 4))->getValue();
      if (!(isa<GlobalVariable>(Ptr) || isa<Function>(Ptr)))
        return nullptr; // other constants are not supported yet

      assert(Ptr->getType()->isPointerTy() && "must be a global");

      uint64_t Add = dyn_cast<ConstantAsMetadata>(Reloc->getOperand(i + 3))
        ->getValue()->getUniqueInteger().getZExtValue();

      Constant *NewValue = nullptr;
      if (Add != 0) {
        GlobalVariable *PtrGV = dyn_cast<GlobalVariable>(Ptr);
        if (!PtrGV)
          return nullptr;

        if (!PtrGV->getValueType()->isIntegerTy(8)) {
          return nullptr;
        }
        // Add is the offset in bytes. We can only make a GEP here
        // if the GV is i8*.
        NewValue = ConstantExpr::getGetElementPtr(PtrGV->getValueType(), PtrGV,
            ConstantInt::get(DL.getIndexType(PtrGV->getType()), Add));
      } else {
        NewValue = dyn_cast<Constant>(Ptr);
      }

      Type *Expected = LI->getType();

      if (Expected != NewValue->getType()) {
        // TODO: type cast would be messy.
        Type *Got = NewValue->getType();
        if (Expected->isIntegerTy(PtrSizeInBits) && Got->isPointerTy()) {
          NewValue = ConstantExpr::getPtrToInt(NewValue, Expected);
        } else {
          // TODO:
          return nullptr;
        }
      }

      return NewValue;
    }

  } else if (ROData->getNumOperands() > 2) {
    // TODO: llvm-link may merge ROData
    return nullptr;
  }

  // Load raw bytes from Data
  // We can only load integer or float
  // TODO: endian?
  if (!(ValType->isIntegerTy() || ValType->isFloatTy() || ValType->isDoubleTy()))
    return nullptr;

  const char * DataPtr = Data.data() + Offset;

  if (ValType->isIntegerTy()) {
    switch (ValType->getIntegerBitWidth()) {
    case 1: {
      int8_t val = *(const int8_t*)DataPtr;
      return ConstantInt::get(ValType, val == 0 ? 0 : 1);
    }
    case 8: {
      int8_t val = *(const int8_t*)DataPtr;
      return ConstantInt::get(ValType, uint64_t(val));
    }
    case 16: {
      int16_t val = *(const int16_t*)DataPtr;
      return ConstantInt::get(ValType, uint64_t(val));
    }
    case 32: {
      int32_t val = *(const int32_t*)DataPtr;
      return ConstantInt::get(ValType, uint64_t(val));
    }
    case 64: {
      int64_t val = *(const int64_t*)DataPtr;
      return ConstantInt::get(ValType, uint64_t(val));
    }
    default:
      return nullptr;
    }
  }

  if (ValType->isFloatTy()) {
    assert(sizeof(float) == 4 && "invariant");
    double val = (double)(*(const float*)DataPtr);
    return ConstantFP::get(ValType, val);
  }

  if (ValType->isDoubleTy()) {
    assert(sizeof(double) == 8 && "invariant");
    double val = *(const double*)DataPtr;
    return ConstantFP::get(ValType, val);
  }

  return nullptr;
}

Value* TangoLLVM::simplifyLoadGoROData(LoadInst *LI) {
  if (!EnableGoROData)
    return nullptr;
  const DataLayout &DL = LI->getDataLayout();
  return TangoLLVM::resolveLoadROData(LI, DL);
}
