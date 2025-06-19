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

#include "llvm/Transforms/IPO/GoROData.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/TangoLLVM.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
using namespace llvm;

// Enable Go RO data optimization
static cl::opt<bool> EnableGoROData("go-rodata", cl::Hidden,
                                  cl::init(true));

#define DEBUG_TYPE "gorodata"

static bool replaceLoadROData(Function &F) {
  bool Changed = false;

  const DataLayout &DL = F.getParent()->getDataLayout();

  const unsigned PtrSizeInBits = DL.getPointerSizeInBits();

  SmallSetVector<LoadInst*, 32> Worklist;
  for (Instruction &I : instructions(F)) {
    LoadInst *LI = dyn_cast<LoadInst>(&I);
    if (LI)
      Worklist.insert(LI);
  }

  while (!Worklist.empty()) {
    LoadInst *LI = Worklist.pop_back_val();
    Value* Ptr = LI->getPointerOperand();
    if (auto *CE = dyn_cast<ConstantExpr>(Ptr)) {
      Instruction *NewInst = CE->getAsInstruction();
      NewInst->insertBefore(LI);
      LI->setOperand(LoadInst::getPointerOperandIndex(), NewInst);
      Ptr = NewInst;
      Changed = true;
    }

    Value *NewValue = TangoLLVM::resolveLoadROData(LI, DL);
    if (NewValue != nullptr) {
      Constant *ConstValue = dyn_cast<Constant>(NewValue);
      assert(ConstValue != nullptr && "currently only constants can be loaded from ROData");
      Type *Expected = LI->getType();
      if (Expected->isPointerTy()) {
        // LI may be int and there is a LoadInst with IntToPtr over it.
        for (auto &U : LI->uses()) {
          LoadInst *NewLI = dyn_cast<LoadInst>(U.getUser());
          if (NewLI == nullptr)
            continue;
          Worklist.insert(NewLI);
        }
      }

      LLVM_DEBUG(dbgs() << "GoROData: " << F.getName() << ": replace" << *LI
          << " by constant value ";
          if (ConstValue->hasName()) { dbgs() << ConstValue->getName(); } else { dbgs() <<  *ConstValue; }
          dbgs() << "\n");

      if (Expected != ConstValue->getType()) {
        // TODO: type cast would be messy.
        Type *Got = ConstValue->getType();
        if (Expected->isIntegerTy(PtrSizeInBits) && Got->isPointerTy()) {
          // The only case that would happen
          // See the implementation of resolveLoadROData

          // Now check if there are redundant IntToPtr
          SmallVector<Instruction*, 8> Replacement;
          for (auto &U : LI->uses()) {
            if (IntToPtrInst *ITP = dyn_cast<IntToPtrInst>(U.getUser())) {
              if (ITP->getType() == ConstValue->getType())
                Replacement.push_back(ITP);
            }
          }

          for (Instruction *R : Replacement) {
            // We are safe to erase because a ITP has only one operand
            // and it will appear in the Replacement vector once.
            R->replaceAllUsesWith(ConstValue);
            R->eraseFromParent();
          }

          if (!LI->use_empty()) {
            // We need proper type to do the replacement.
            LI->replaceAllUsesWith(ConstantExpr::getPtrToInt(ConstValue, Expected));
          } // other case: LI may only have redundant IntToPtr use.
        } else {
          // TODO:
          continue;
        }
      } else {
        LI->replaceAllUsesWith(ConstValue);
      }

      assert(LI->use_empty() && "must be replaced");
      LI->eraseFromParent();
      Changed = true;
    }
  }

  return Changed;
}


PreservedAnalyses GoRODataPass::run(Module &M, ModuleAnalysisManager &) {
  if (!EnableGoROData)
    return PreservedAnalyses::all();

  bool Changed = false;

  for (Function &F : M) {
    Changed |= replaceLoadROData(F);
  }

  if (!Changed)
    return PreservedAnalyses::all();

  return PreservedAnalyses::none() ;
}
