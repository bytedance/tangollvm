//===-- TangoLLVM.cpp ----  -------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 ByteDance Ltd. and/or its affiliates.
//===----------------------------------------------------------------------===//
//
/// @file
/// This file contains the implementations of TangoLLVM
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/TangoLLVM.h"

namespace llvm {

// TANGOLLVM
static int64_t getArgAreaSize(Function *F) {
  if (auto n = F->getMetadata("go.callee.args.area")) {
    assert(n->getNumOperands() == 2 && "must have at least two operands");
    int64_t argAreaSize = dyn_cast<ConstantAsMetadata>(n->getOperand(0))
                              ->getValue()
                              ->getUniqueInteger()
                              .getSExtValue();
    return argAreaSize;
  }
  return 0;
}

// update go.callee.args.area metadata when inlining
void TangoLLVM::updateGoCalleeArgsArea(Function *Callee, Function *Caller) {
  int64_t CalleeArgAreaSize = getArgAreaSize(Callee);
  int64_t CallerArgAreaSize = getArgAreaSize(Caller);
  // no need to update if the caller has enough arg area
  if (CallerArgAreaSize >= CalleeArgAreaSize)
    return;
  // Caller's go.callee.args.area is too small, update
  auto newSize = ConstantAsMetadata::get(ConstantInt::get(
      Type::getInt64Ty(Caller->getContext()), CalleeArgAreaSize));
  auto newType =
      ConstantAsMetadata::get(ConstantAggregateZero::get(ArrayType::get(
          Type::getInt8Ty(Caller->getContext()), CalleeArgAreaSize)));
  Caller->setMetadata("go.callee.args.area",
                      MDNode::get(Caller->getContext(), {newSize, newType}));
}

bool TangoLLVM::isObsoleted(Function *Fn) {
  return Fn->hasFnAttribute("go.obsolete");
}

void TangoLLVM::markObsoleted(Function *Fn) {
  if (Fn->hasFnAttribute("go.obsolete")) {
    return;
  }

  // TODO: a better way is to write attributes in object
  // files and ignore the function during patching.

  StringRef Name = Fn->getName();
  std::string OldName = Name.str();

  // 1). Rename the function
  std::string NewName = Name.str();
  NewName += "..obsolete";
  Fn->setName(StringRef(NewName));

  // 2). Add a new function declaration with the same name
  Module *M = Fn->getParent();
  Function *NewFn = Function::Create(Fn->getFunctionType(), Fn->getLinkage(), StringRef(OldName), M);

  // 3). Replace the old function by the new function
  Fn->replaceAllUsesWith(NewFn);
  Fn->addFnAttr("go.obsolete");
}

} // namespace llvm
