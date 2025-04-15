//==- TangoLLVM.h - constants and functions for TangoLLVM  ----------*-C++-*-==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 ByteDance Ltd. and/or its affiliates.

#ifndef LLVM_IR_TANGOLLVM_H_
#define LLVM_IR_TANGOLLVM_H_

#include <llvm/IR/Function.h>
#include <llvm/CodeGen/MachineInstr.h>


namespace llvm {

enum class GoAdHocABI {
  None,
  GoWriteBarrier, // gcWriteBarrier
  GoWriteBarrierN, // gcWriteBarrier1 ~ gcWriteBarrier2
  DuffCopy,
  DuffZero,
  PanicBoundsA,
  PanicBoundsB,
  PanicBoundsC
};

class TangoLLVM {

public:

  static void updateGoCalleeArgsArea(Function *Callee, Function *Caller);

  static bool isObsoleted(Function *Fn);
  static void markObsoleted(Function *Fn);

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

};


} // end namespace llvm

#endif // LLVM_IR_TANGOLLVM_H_
