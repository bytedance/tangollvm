// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 ByteDance Ltd. and/or its affiliates.

#ifndef LLVM_TRANSFORMS_SCALAR_GONILCHECKELIM_H
#define LLVM_TRANSFORMS_SCALAR_GONILCHECKELIM_H

#include "llvm/IR/Function.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Instructions.h"

namespace llvm {

class GoNilCheckElimPass : public PassInfoMixin<GoNilCheckElimPass> {
public:
  PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
};
}

#endif // LLVM_TRANSFORMS_SCALAR_GONILCHECKELIM_H
