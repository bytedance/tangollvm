// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 ByteDance Ltd. and/or its affiliates.

#include "X86.h"
#include "X86Subtarget.h"
#include "llvm/Analysis/ProfileSummaryInfo.h"
#include "llvm/CodeGen/LazyMachineBlockFrequencyInfo.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/TangoLLVM.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

#define GOFIXUPSF_DESC "X86 Go Stack Split Fixup"
#define GOFIXUPSF_NAME "x86-go-fixup-stack-split"

using namespace  llvm;
namespace {
class GoFixupStackSplitPass : public MachineFunctionPass {
public:
  static char ID;

  StringRef getPassName() const override { return GOFIXUPSF_NAME; }

  GoFixupStackSplitPass() : MachineFunctionPass(ID) { }

  bool runOnMachineFunction(MachineFunction &MF) override;
private:
  void fixupGoSplitStack(MachineFunction &MF);
};

}

INITIALIZE_PASS(GoFixupStackSplitPass, GOFIXUPSF_NAME, GOFIXUPSF_DESC, false, false)

char GoFixupStackSplitPass::ID = 0;

FunctionPass *llvm::createX86GoFixupStackSplitPass() { return new GoFixupStackSplitPass(); }

void GoFixupStackSplitPass::fixupGoSplitStack(MachineFunction &MF) {
  auto *F = &MF;
  auto TII = MF.getSubtarget<X86Subtarget>().getInstrInfo();
  if (F->size() == 0) return;
  // The first block would be a empty block ending with a JMP
  MachineBasicBlock &Entry = F->front();
  if (Entry.hasAddressTaken())
    return; // we may not be able to remove entry
  if (Entry.succ_size() != 1)
    return;

  if (Entry.getFirstTerminator() == Entry.end())
    return;

  // An empty MBB to avoid place a loop head at the entry.
  MachineInstr *Jmp = &*Entry.getFirstTerminator();
  if (!Jmp->isUnconditionalBranch())
    return;

  for (auto &MI : Entry) {
    if (MI.isMetaInstruction())
      continue;
    if (Jmp == &MI)
      continue;

    // return for any other cases
    return;
  }

  // Move Alloc
  auto I = Entry.succ_begin();
  MachineBasicBlock *CheckMBB = *I;
  if (CheckMBB->succ_size() != 2)
    return;

  auto isAllocMBB = [](MachineBasicBlock &MBB) {
    // find the call to morestack
    for (auto &MI : MBB)
      if (TangoLLVMCodeGen::isMoreStackCall(MI))
        return true;
    return false;
  };

  MachineBasicBlock *ExitMBB = nullptr;
  MachineBasicBlock *AllocMBB = nullptr;
  for (auto I = CheckMBB->succ_begin(), E = CheckMBB->succ_end();
      I != E; I++) {
    MachineBasicBlock *SuccMBB = *I;
    if (isAllocMBB(*SuccMBB)) {
      assert(AllocMBB == nullptr && "A CheckMBB can only have one AllocMBB");
      if (AllocMBB != nullptr)
        return; // something that we cannot handle; add this check when assertion is disabled.
      AllocMBB = SuccMBB;
    } else {
      if (ExitMBB != nullptr)
        return; // happens when there is no AllocMBB
      ExitMBB = SuccMBB;
    }
      }

  assert(AllocMBB != nullptr && ExitMBB != nullptr && "invariant");
  assert(AllocMBB->succ_size() == 1 && "CheckMBB is the only successor of AllocMBB");

  MachineBasicBlock *CheckMBB2 = nullptr;
  if (AllocMBB->pred_size() == 1) {
    // CheckMBB is the only predecessor
  } else if (AllocMBB->pred_size() == 2) {
    // there may be a CheckMBB2
    for (auto *MBB : AllocMBB->predecessors()) {
      if (MBB == CheckMBB)
        continue;
      assert(CheckMBB2 == nullptr && "invariant");
      if (CheckMBB2 != nullptr)
        return; // return if something goes wrong
      CheckMBB2 = MBB;
    }
  } else {
    assert(false && "should not happen");
    return;
  }

  DebugLoc dl; // FIXME: this is nowhere

  // always have one because CheckMBB is not the entry
  assert(CheckMBB->getIterator() != F->begin() && "must not be entry");
  MachineBasicBlock *PrevMBB = &*std::prev(CheckMBB->getIterator());
  bool NeedMoveCheckMBB = true;
  if (PrevMBB->canFallThrough()) {
    assert((PrevMBB == &Entry || PrevMBB == AllocMBB) && "invariant");
    if (PrevMBB == AllocMBB) {
      // we know we will move AllocMBB to the end
      TII->insertUnconditionalBranch(*AllocMBB, CheckMBB, dl);
    } else {
      // do nothing since we will remove Entry
      NeedMoveCheckMBB = false;
    }
  }

  if (NeedMoveCheckMBB && CheckMBB->canFallThrough()) {
    // there must some BB after it
    MachineBasicBlock *NextMBB = &*std::next(CheckMBB->getIterator());
    assert((NextMBB == ExitMBB || NextMBB == AllocMBB) && "invariant");
    if (NextMBB == AllocMBB) {
      // TODO: rare case
      // Note that if CheckMBB can fall through to AllocMBB
      // then AllocMBB cannot fall through to CheckMBB.
      return;
    } else {
      // need do nothing if NextMBB is ExitMBB
      // move exit MBB is messy
      bool NeedJump = false;
      for (auto I = std::next(ExitMBB->getReverseIterator()),
          E = F->rend(); I != E; I++) {
        MachineBasicBlock *MBB = &*I;
        if (!(MBB == &Entry || MBB == CheckMBB || MBB == AllocMBB)) {
          NeedJump = true;
          break;
        }
          }

      // NeedJump rarely happens
      if (NeedJump)
        TII->insertUnconditionalBranch(*CheckMBB, ExitMBB, dl);
    }
  }

  // invariant: No BB except Entry can fall through to CheckMBB
  // invariant: CheckMBB is the only predecessor of ExitMBB
  assert(ExitMBB->pred_size() == 1 && "invariant");
  CheckMBB->moveAfter(&Entry);

  // TODO: move internal MBB
  MachineFunction::iterator FI = AllocMBB->getIterator();
  ++FI;
  if (FI != F->end()) {
    MachineBasicBlock *PrevMBB = &*std::prev(AllocMBB->getIterator());
    if (PrevMBB == CheckMBB // we already insert conditional branch for AllocMBB
        || !PrevMBB->canFallThrough()) {
      AllocMBB->moveAfter(&F->back());
        } else {
          assert(CheckMBB2 != nullptr && "the only fall through MBB is CheckMBB2");
        }
  }

  // Finally, remove the trivial entry block
  Entry.removeSuccessor(CheckMBB);
  F->remove(&Entry);
  F->deleteMachineBasicBlock(&Entry);
}

bool GoFixupStackSplitPass::runOnMachineFunction(MachineFunction &MF) {
  this->fixupGoSplitStack(MF);
  return true;
}
