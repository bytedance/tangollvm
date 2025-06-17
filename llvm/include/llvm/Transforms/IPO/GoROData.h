// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 ByteDance Ltd. and/or its affiliates.

#ifndef LLVM_TRANSFORMS_IPO_GORODATA_H
#define LLVM_TRANSFORMS_IPO_GORODATA_H

#include "llvm/IR/Function.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/Instructions.h"

namespace llvm {

struct GoRODataPass : public PassInfoMixin<GoRODataPass> {
public:
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &);
};
}

#endif // LLVM_TRANSFORMS_IPO_GORODATA_H
