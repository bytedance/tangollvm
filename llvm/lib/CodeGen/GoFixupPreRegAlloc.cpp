//===- llvm/CodeGen/GoFixupPreRegAlloc.cpp - Virtual Register Map -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 ByteDance Ltd. and/or its affiliates.
//===----------------------------------------------------------------------===//
// TODO: for future fixup
//===----------------------------------------------------------------------===//

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/CodeGen/LiveInterval.h"
#include "llvm/CodeGen/LiveIntervals.h"
#include "llvm/CodeGen/LiveStacks.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/SlotIndexes.h"
#include "llvm/CodeGen/StackMaps.h"
#include "llvm/CodeGen/TargetFrameLowering.h"
#include "llvm/CodeGen/TargetInstrInfo.h"
#include "llvm/CodeGen/TargetOpcodes.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/CodeGen/TargetSubtargetInfo.h"
#include "llvm/CodeGen/VirtRegMap.h"
#include "llvm/Config/llvm-config.h"
#include "llvm/IR/Statepoint.h"
#include "llvm/Support/raw_ostream.h"
#include <cassert>
#include <vector>

using namespace llvm;

namespace {

class GoFixupPreRegAlloc : public MachineFunctionPass {
  MachineFunction *MF;
  const TargetRegisterInfo *TRI;

  bool removeUnusedPointers();

public:
  static char ID;
  GoFixupPreRegAlloc() : MachineFunctionPass(ID) {
    initializeGoFixupPreRegAllocPass(*PassRegistry::getPassRegistry());
  }

  MachineFunctionProperties getRequiredProperties() const override {
    return MachineFunctionProperties()
      .set(MachineFunctionProperties::Property::IsSSA).set(
          MachineFunctionProperties::Property::TracksLiveness);
  }

  bool runOnMachineFunction(MachineFunction&) override;
};

} // end anonymous namespace

char GoFixupPreRegAlloc::ID = 0;

char &llvm::GoFixupPreRegAllocID = GoFixupPreRegAlloc::ID;

INITIALIZE_PASS_BEGIN(GoFixupPreRegAlloc, "gofixupssa",
                      "GCStatepoint Spill Deduplication", false, false)
INITIALIZE_PASS_END(GoFixupPreRegAlloc, "gofixupssa",
                    "GCStatepoint Spill Deduplication", false, false)

bool GoFixupPreRegAlloc::removeUnusedPointers() {
  bool Changed = false;
  // Used by meta operands of STATEPOINT
  DenseMap<Register, SmallVector<MachineInstr*, 8>> FakeUsage;
  // Any other usage
  DenseSet<Register> RealUsage;

  auto RemoveFakeUsage = [](MachineInstr &MI, SmallVector<unsigned, 4> &RegUseIdxs) {
    StatepointOpers SO(&MI);
    for (auto I = RegUseIdxs.rbegin(), E = RegUseIdxs.rend();
        I != E; I++) {
      MI.removeOperand(*I);
    }
    int64_t NumDeoptArgs = SO.getNumDeoptArgs();
    int64_t NumKilledArgs = int64_t(RegUseIdxs.size());
    assert(NumDeoptArgs >= NumKilledArgs && "invariant");
    MI.getOperand(SO.getNumDeoptArgsIdx()).setImm(NumDeoptArgs - NumKilledArgs);
  };

  for (auto &MBB : *MF) {
    for (auto &MI : MBB) {
      if (MI.getOpcode() == TargetOpcode::STATEPOINT) {
        StatepointOpers SO(&MI);
        SmallVector<unsigned, 4> KilledFakeUsage;
        assert(!(SO.getFlags() & (uint64_t)StatepointFlags::DeoptLiveIn) && "go GC uses deopt to encode live-through pointers");
        for (unsigned Idx = 0, E = SO.getVarIdx(); Idx < E;
            ++Idx) {
          const MachineOperand &MO = MI.getOperand(Idx);
          if (!MO.isReg())
            continue;
          if (MO.isDef())
            continue;
          if (!Register::isVirtualRegister(MO.getReg())) {
            continue;
          }
          RealUsage.insert(MO.getReg());
        }
        for (unsigned Idx = SO.getNumDeoptArgsIdx(), E = SO.getNumGCPtrIdx(); Idx < E;
            ++Idx) {
          const MachineOperand &MO = MI.getOperand(Idx);
          if (!MO.isReg())
            continue;
          if (MO.isDef())
            continue;
          if (!Register::isVirtualRegister(MO.getReg())) {
            continue;
          }

          // Here is tricky.
          // At IR level, we compute all live-through pointers and set them to Deopt.
          // But Deopt only needs live-ins.
          // So if we witnessed that a live-through IR value is killed, then it
          // means that we can safely remove it here, no matter whether it has any
          // real usage elsewhere.
          if (MO.isKill()) {
            KilledFakeUsage.push_back(Idx);
            continue;
          }
          FakeUsage[MO.getReg()].push_back(&MI);
        }
        if (!KilledFakeUsage.empty())
          RemoveFakeUsage(MI, KilledFakeUsage);
      } else {
        for (unsigned Idx = 0, E = MI.getNumOperands(); Idx < E; ++Idx) {
          const MachineOperand &MO = MI.getOperand(Idx);
          if (!MO.isReg())
            continue;
          if (MO.isDef())
            continue;
          if (!Register::isVirtualRegister(MO.getReg())) {
            continue;
          }
          RealUsage.insert(MO.getReg());
        }
      }
    }
  }

  for (auto I = FakeUsage.begin(), E = FakeUsage.end(); I != E; ++I) {
    Register Reg = I->first;
    if (RealUsage.count(Reg))
      continue;
    SmallVector<MachineInstr*, 8> &Usage = I->second;
    assert(!Usage.empty() && "invariant");
    for (auto MI : Usage) {
      SmallVector<unsigned, 4> RegUseIdxs;
      StatepointOpers SO(MI);
      for (unsigned Idx = SO.getNumDeoptArgsIdx(), E = SO.getNumGCPtrIdx(); Idx < E;
          ++Idx) {
        const MachineOperand &MO = MI->getOperand(Idx);
        if (MO.isReg() && MO.getReg() == Reg)
          RegUseIdxs.push_back(Idx);
      }

      RemoveFakeUsage(*MI, RegUseIdxs);
    }

    Changed = true;
  }

  return Changed;
}

bool GoFixupPreRegAlloc::runOnMachineFunction(MachineFunction &fn) {
  MF = &fn;

  // This is used for Go function only.
  if (!MF->getFunction().hasGC() || MF->getFunction().getGC() != "go") {
    return false;
  }

  TRI = MF->getSubtarget().getRegisterInfo();

  bool Changed = false;
  Changed |= removeUnusedPointers();
  return Changed;
}

FunctionPass *llvm::createGoFixupPreRegAllocPass() {
  return new GoFixupPreRegAlloc();
}
