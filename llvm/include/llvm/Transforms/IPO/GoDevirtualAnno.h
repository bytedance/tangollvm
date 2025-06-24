// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 ByteDance Ltd. and/or its affiliates.

#ifndef LLVM_TRANSFORMS_IPO_GODEVIRTUALANNO_H
#define LLVM_TRANSFORMS_IPO_GODEVIRTUALANNO_H

#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"

namespace llvm {
struct GoDevirtualAnnotatorPass
    : public PassInfoMixin<GoDevirtualAnnotatorPass> {
public:
  GoDevirtualAnnotatorPass() {}

  PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
};

} // namespace llvm

#endif // LLVM_TRANSFORMS_IPO_GODEVIRTUALANNO_H