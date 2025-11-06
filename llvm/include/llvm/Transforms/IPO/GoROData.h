// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 ByteDance Ltd. and/or its affiliates.

#ifndef LLVM_TRANSFORMS_IPO_GORODATA_H
#define LLVM_TRANSFORMS_IPO_GORODATA_H

#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "llvm/Analysis/ProfileSummaryInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Support/VirtualFileSystem.h"

namespace llvm {

struct GoRODataOptions {
  bool Specialize;
  bool LTO;
  std::string FileName;
  GoRODataOptions(bool Specialize = false, bool LTO = false,
                  const std::string &FileName = "") : Specialize(Specialize), LTO(LTO), FileName(FileName) {}
};

struct GoReloc {
  uint64_t off;
  uint64_t size;
  int64_t addend;
  int64_t enabled;
  std::string sym;
};

struct GoROData {
  bool invalid = false;
  SmallVector<GoReloc, 4> relocs;
};

struct GoRODataPass : public PassInfoMixin<GoRODataPass> {
  GoRODataOptions Options;

private:
  DenseMap<GlobalVariable *, GoROData> GVMap;
  SampleProfileMap ProfileMap;
  IntrusiveRefCntPtr<vfs::FileSystem> FS;
public:
  GoRODataPass() = default;
  GoRODataPass(GoRODataOptions Options) : Options(Options) {}

  PreservedAnalyses run(Module &M, ModuleAnalysisManager &);
  bool specializeGoBuiltins(ProfileSummaryInfo& PSI, Function &F, bool UseInline);
  CallInst *specializeCall(ProfileSummaryInfo& PSI, Function &F, CallInst *CI, bool Aggressive);
  bool replaceLoadROData(Function &F);
  bool runOnFunction(ProfileSummaryInfo& PSI, Function &F, bool Specialize, bool UseInline);
  Constant *resolveLoadROData(LoadInst *LI, const DataLayout &DL);
};

struct GORoDataInitializerPass : public PassInfoMixin<GORoDataInitializerPass> {
public:
  GORoDataInitializerPass() = default;

  GlobalVariable* materializeROInit(GlobalVariable &G, Module &M);
  PreservedAnalyses run(Module &M, ModuleAnalysisManager &);
private:
  DenseMap<GlobalVariable *, GoROData> GVMap;
};

struct GORoDataRestorePass : public PassInfoMixin<GORoDataRestorePass> {
public:
  GORoDataRestorePass() = default;

  PreservedAnalyses run(Module &M, ModuleAnalysisManager &);
};


} // namespace llvm

#endif // LLVM_TRANSFORMS_IPO_GORODATA_H
