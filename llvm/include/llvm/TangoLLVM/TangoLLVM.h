//==- TangoLLVM.h - constants and functions for TangoLLVM  ----------*-C++-*-==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 ByteDance Ltd. and/or its affiliates.

#ifndef LLVM_IR_TANGOLLVM_H_
#define LLVM_IR_TANGOLLVM_H_

#include <llvm/Analysis/AssumptionCache.h>
#include <llvm/CodeGen/MachineInstr.h>
#include <llvm/IR/Constants.h>
#include "llvm/IR/Dominators.h"
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>


namespace llvm {

enum class GoMetadata {
  // The stack object is used for open defer return values.
  OpenDeferReturn,
  OpenDefer,
  // The Stack object must be always live during liveness analysis.
  AlwaysLive,
  // Used to generate FUNCDATA_StackObjects
  StackObjectGCData,
  // Pointer created by the front-end, which is a valid GC pointer
  PointerValue,
  // uintptr created by the front-end, which can be converted into a pointer.
  PointerLikeValue,
  // The number of bytes known to be dereferenceable
  // for function, it would be
  // |------- ret -------|------ param ------|
  // |len{idx:value, ...}, len{idx:value,...}
  DereferSize,

  // Count of metadata
  MaxMetadata
};

enum class GoAdHocABI {
  None,
  GoWriteBarrier, // gcWriteBarrier
  GoWriteBarrierN, // gcWriteBarrier1 ~ gcWriteBarrier2
  DuffCopy,
  DuffZero,
  PanicBoundsA,
  PanicBoundsB,
  PanicBoundsC,
  KeepAlive
};

class TangoLLVM {

public:

  static bool isGoPanicBounds(StringRef Name) {
    if (!Name.consume_front("runtime."))
      return false;
    if (!Name.consume_front("panic"))
      return false;

    return StringSwitch<bool>(Name)
        .Cases("Slice3Alen", "Slice3AlenU", true)
        .Cases("Slice3Acap", "Slice3AcapU", true)
        .Case("SliceConvert", true)
        .Cases("SliceAlen", "SliceAlenU", true)
        .Cases("SliceAcap", "SliceAcapU", true)
        .Cases("Slice3B", "Slice3BU", true)
        .Cases("Index", "IndexU", true)
        .Cases("SliceB", "SliceBU", true)
        .Cases("Slice3C", "Slice3CU", true)
        .Default(false);
  }

  static GoAdHocABI getGoAdHocABI(StringRef Name) {
    GoAdHocABI goAdHocABI = GoAdHocABI::None;
    if (Name.starts_with("runtime.")) {
      Name = Name.substr(8); // 8 = strlen("runtime.")
      if (Name == "gcWriteBarrier") {
        // AdHoc Calling Convention and Preserve All General Purposes
        goAdHocABI = GoAdHocABI::GoWriteBarrier;
      } else if (Name.starts_with("gcWriteBarrier")) {
        // starts_with but not gcWriteBarrier
        goAdHocABI = GoAdHocABI::GoWriteBarrierN;
      } else if (Name == "duffcopy") {
        // C Calling Convention and Preserve All
        goAdHocABI = GoAdHocABI::DuffCopy;
      } else if (Name == "duffzero") {
        // C Calling Convention and Preserve All
        goAdHocABI = GoAdHocABI::DuffZero;
      } else if (Name.starts_with("panic")) {
        Name = Name.substr(5); // 5 = strlen("panic")
        if (Name == "Slice3Alen" || Name == "Slice3AlenU"
            || Name == "Slice3Acap" || Name == "Slice3AcapU"
            || Name == "SliceConvert") {
          // DX, BX
          goAdHocABI = GoAdHocABI::PanicBoundsA;
        } else if (Name == "SliceAlen" || Name == "SliceAlenU"
            || Name == "SliceAcap" || Name == "SliceAcapU"
           || Name == "Slice3B" || Name == "Slice3BU") {
          // CX, DX
          goAdHocABI = GoAdHocABI::PanicBoundsB;
        } else if (Name == "Index" || Name == "IndexU"
            || Name == "SliceB" || Name == "SliceBU"
           || Name == "Slice3C" || Name == "Slice3CU") {
          // AX, CX
          goAdHocABI = GoAdHocABI::PanicBoundsC;
        }
      } else if (Name == "KeepAlive") {
        goAdHocABI = GoAdHocABI::KeepAlive;
      }
    }
    return goAdHocABI;
  }

  static bool intrinsifyMemClr(CallInst *CI);
  static bool intrinsifyStrHash(CallInst *CI);
  static bool intrinsifyMemHash32Or64(CallInst *CI);
  static bool intrinsifyMemHash(CallInst *CI);

  static bool hasGoPanicReturn(const BasicBlock *BB) {
    return BB->getGoPanicReturnCall() != nullptr;
  }

  static bool isGoPseudoIntrinsic(const Instruction &I) {
    if (const IntrinsicInst * II = dyn_cast<const IntrinsicInst>(&I)) {
      auto ID = II->getIntrinsicID();
      return ID == Intrinsic::go_metadata || ID == Intrinsic::go_panicreturn;
    }
    return false;
  }

  static void prepareFixGoPanicReturn(BasicBlock *BB);
  static bool fixGoPanicReturn(Function &F, Function::iterator Begin, SmallVectorImpl<ReturnInst*> &Returns);

  static const char* getGoMetadataName(GoMetadata M) {
    switch (M) {
    default:
      report_fatal_error("unsupported metadata");
    case GoMetadata::OpenDeferReturn:
      return "go.opendefer.return";
    case GoMetadata::OpenDefer:
      return "go.opendefer";
    case GoMetadata::AlwaysLive:
      return "go.alwayslive";
    case GoMetadata::StackObjectGCData:
      return "go.stackobject.gcdata";
    case GoMetadata::PointerValue:
      return "go.pointervalue";
    case GoMetadata::PointerLikeValue:
      return "go.pointerlikevalue";
      case GoMetadata::DereferSize:
      return "go.derefersize";
    }

    return nullptr;
  }

  // Remove all intrinsic call llvm.go.metadata
  // Fill the metadata back to tracked instructions
  static void harvestGoMetadata(Function &F, ArrayRef<GoMetadata> WL);

  static void updateGoCalleeAreaSize(Function &F, int64_t Size);
  static void updateMetadataForInlining(Function *Callee, Function *Caller);

  static bool isObsoleted(Function *Fn);
  static void markObsoleted(Function *Fn);

  static bool isGoClosureCall(CallBase *CB) {
    return CB->hasFnAttr("go.closurecall");
  }

  static CallInst * lowerIntrinsic(IntrinsicInst *II);

  static int64_t getCallSiteGoCallerAreaSize(CallBase *CB) {
    if (CB->hasFnAttr("go.caller.area")) {
      auto Attr = CB->getFnAttr("go.caller.area").getValueAsString();
      int64_t Val;
      if (!Attr.getAsInteger(10, Val)) {
        return Val;
      }
    }
    // no attr or no valid attr
    return -1;
  }

  // Note that all these symbols must be builtin symbols, which means they always
  // have prefix "runtime.", even we are building the runtime package.
  static bool isZeroArgRuntimeCall(StringRef FuncName) {
    StringRef PrefixRuntime = StringRef("runtime.");
    if (!FuncName.starts_with(PrefixRuntime)) {
      return false;
    }
    FuncName = FuncName.substr(PrefixRuntime.size());
    if (FuncName.starts_with(StringRef("panicdivide")) || FuncName.starts_with(StringRef("panicwrap"))
        || FuncName.starts_with(StringRef("panicshift")) || FuncName.starts_with(StringRef("panicIndex"))
        || FuncName.starts_with(StringRef("panicSlice"))) {
      return true;
    }
    if (FuncName.starts_with(StringRef("gcWriteBarrier"))) {
      return true;
    }
    if (FuncName == (StringRef("duffzero")) || FuncName == (StringRef("duffcopy"))) {
      return true;
    }
    return false;
  }

  static bool isGCWriteBarrier(StringRef FuncName) {
    if (FuncName.starts_with("runtime.gcWriteBarrier")) {
      return true;
    }
    return false;
  }

  static bool hasPointerTy(Type *T) {
    switch (T->getTypeID()) {
    case Type::PointerTyID:
      return true;
    case Type::ArrayTyID:
      return T->getArrayNumElements() > 0 && hasPointerTy(T->getArrayElementType());
    case Type::FixedVectorTyID:
    case Type::ScalableVectorTyID:
      return hasPointerTy(T->getScalarType());
    case Type::StructTyID: {
      for (unsigned i = 0, e = T->getStructNumElements(); i < e; ++i)
        if (hasPointerTy(T->getStructElementType(i)))
          return true;
      return false;
    }
    default:
      return false;
    }
  }


  static StringRef getMoreStackSymbol(const Function &F) {
    // TODO: we allow frontend to use internal name mangling
    if (F.hasFnAttribute("go-sym-morestack")) {
      Attribute attr = F.getFnAttribute("go-sym-morestack");
      assert(attr.isStringAttribute() && "must be string attribute");
      return attr.getValueAsString();
    }
    StringRef morestackSymbol = "runtime.morestack_noctxt";
    if (F.hasFnAttribute("go-need-ctxt")) {
      morestackSymbol = "runtime.morestack";
    }
    return morestackSymbol;
  }

  static bool hasGoGC(const Function &Fn) {
    return Fn.hasGC() && Fn.getGC() == "go";
  }

  static bool isGoABIInternal(const Function &Fn) {
    return Fn.getCallingConv() == CallingConv::Go_Internal;
  }

  static bool isGoPreAllocated(Value * V) {
    if (CallInst *Call = dyn_cast<CallInst>(V)) {
      if (Function *Fn = Call->getCalledFunction()) {
        if (Fn->getIntrinsicID() == Intrinsic::go_preallocated) {
          return true;
        }
      }
    }
    return false;
  }

  static Type* getGoPreAllocatedElementType(Value * V) {
    if (CallInst *Call = dyn_cast<CallInst>(V)) {
      if (Function *Fn = Call->getCalledFunction()) {
        if (Fn->getIntrinsicID() == Intrinsic::go_preallocated) {
          return Call->getArgOperand(3)->getType();
        }
      }
    }
    return nullptr;
  }

  static bool isGoPreAllocatedParamOut(Value * V) {
    if (CallInst *Call = dyn_cast<CallInst>(V)) {
      if (Function *Fn = Call->getCalledFunction()) {
        if (Fn->getIntrinsicID() == Intrinsic::go_preallocated) {
          ConstantInt *ParamOut = dyn_cast<ConstantInt>(Call->getArgOperand(4));
          assert(ParamOut != nullptr && "sanity check");
          return !ParamOut->isZero();
        }
      }
    }
    return false;
  }

  static Constant *resolveLoadROData(LoadInst *LI, const DataLayout &DL);

  static Value *simplifyLoadGoROData(LoadInst *LI);

  // The front-end of Go may create some globals for function as the placeholder
  // in go.rodata
  static Function *createGoFunction(FunctionType *Ty, GlobalValue::LinkageTypes Linkage,
      StringRef N, Module *M);
  static GlobalVariable *createGoGlobalVariable(Type *ValueType, GlobalValue::LinkageTypes Linkage,
      StringRef N, Module *M);

  static uint64_t getDereferSize(const Value* V, unsigned Idx = 0, bool FnParam = false);

  static bool isDereferenceable(const Value *V,
                                const APInt &Size,
                                const DataLayout &DL,
                                const TargetLibraryInfo &TLI);
};


} // end namespace llvm

#endif // LLVM_IR_TANGOLLVM_H_
