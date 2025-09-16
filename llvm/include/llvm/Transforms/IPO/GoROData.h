// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 ByteDance Ltd. and/or its affiliates.

#ifndef LLVM_TRANSFORMS_IPO_GORODATA_H
#define LLVM_TRANSFORMS_IPO_GORODATA_H

#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PassManager.h"

namespace llvm {

struct GoRODataOptions {
  bool Specialize;

  GoRODataOptions(bool Specialize = false) : Specialize(Specialize) {}
};

struct GoRODataPass : public PassInfoMixin<GoRODataPass> {
  GoRODataOptions Options;

public:
  GoRODataPass() = default;

  GoRODataPass(GoRODataOptions Options) : Options(Options) {}

  PreservedAnalyses run(Module &M, ModuleAnalysisManager &);
};

struct GoReloc {
  uint64_t off;
  int64_t addend;
  std::string sym;
};

struct GORoDataInitializerPass : public PassInfoMixin<GORoDataInitializerPass> {
public:
  GORoDataInitializerPass() = default;

  PreservedAnalyses run(Module &M, ModuleAnalysisManager &);
};

struct GORoDataRestorePass : public PassInfoMixin<GORoDataRestorePass> {
public:
  GORoDataRestorePass() = default;

  PreservedAnalyses run(Module &M, ModuleAnalysisManager &);
};


} // namespace llvm

#endif // LLVM_TRANSFORMS_IPO_GORODATA_H
