//===- X86GoFixupPostRegAlloc.cpp - optimize usage of LEA instructions -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 ByteDance Ltd. and/or its affiliates.
//===----------------------------------------------------------------------===//
//
// This file defines the pass that performs some optimizations with LEA
// instructions in order to improve performance and code size.
// Currently, it does two things:
// 1) If there are two LEA instructions calculating addresses which only differ
//    by displacement inside a basic block, one of them is removed.
// 2) Address calculations in load and store instructions are replaced by
//    existing LEA def registers where possible.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/X86BaseInfo.h"
#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/CodeGen/LiveIntervals.h"
#include "llvm/CodeGen/LiveVariables.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/StackMaps.h"
#include "llvm/CodeGen/TargetOpcodes.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/IR/Function.h"
#include "llvm/Support/raw_ostream.h"
#include <cassert>
#include <cstdint>
#include <iterator>

#define DEBUG_TYPE "x86-go-fixup-postregalloc"

using namespace llvm;

namespace {

class X86GoFixupPostRegAllocPass : public MachineFunctionPass {
public:
  X86GoFixupPostRegAllocPass() : MachineFunctionPass(ID) {
    initializeX86GoFixupPostRegAllocPassPass(*PassRegistry::getPassRegistry());
  }

  StringRef getPassName() const override { return "X86 Go Fixup PostRegAlloc"; }

  bool runOnMachineFunction(MachineFunction &MF) override;

  static char ID;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.setPreservesAll();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

private:
  MachineFunction *MF = nullptr;

  bool rewriteGCWriteBarrier();
};

} // end anonymous namespace

char X86GoFixupPostRegAllocPass::ID = 0;

FunctionPass *llvm::createX86GoFixupPostRegAlloc() { return new X86GoFixupPostRegAllocPass(); }
INITIALIZE_PASS(X86GoFixupPostRegAllocPass, DEBUG_TYPE, "X86 Fixup postregalloc pass", false,
                false)

bool X86GoFixupPostRegAllocPass::runOnMachineFunction(MachineFunction &fn) {
  MF = &fn;
  // This is used for Go function only.
  if (!MF->getFunction().hasGC() || MF->getFunction().getGC() != "go") {
    return false;
  }

  bool Changed = false;

  Changed |= rewriteGCWriteBarrier();

  return Changed;
}


bool X86GoFixupPostRegAllocPass::rewriteGCWriteBarrier() {
  bool Changed = false;
  SmallVector<MachineInstr*, 8> ToBeRemoved;
  const X86InstrInfo *TII = MF->getSubtarget<X86Subtarget>().getInstrInfo();
  const X86RegisterInfo *TRI = MF->getSubtarget<X86Subtarget>().getRegisterInfo();
  // Need to remove call setup of KeepAlive
  unsigned FrameSetupOpcode = TII->getCallFrameSetupOpcode();
  unsigned FrameDestroyOpcode = TII->getCallFrameDestroyOpcode();
  for (auto &MBB : *MF)
    for (auto &MI : MBB) {
      if (!MI.isCall())
        continue;

      if (MI.getOpcode() == TargetOpcode::STATEPOINT)
        continue;

      MachineOperand &Callee = MI.getOperand(0);
      if (!Callee.isGlobal())
        continue;


      const Function *F = dyn_cast<Function>(Callee.getGlobal());
      assert(F != nullptr);

      if (F->getName() == "runtime.KeepAlive") {
        // We must do it after RegAlloc because RegAlloc may not
        // spill a dead VirtReg.
        ToBeRemoved.push_back(&MI);
        // We must remove three instructions
        // 1. CallFrameSetup
        // 2. CallFrameDestory
        // 3. PassArg to RBX
        // RAX is a undef value
        // TODO: we still waste RBX here
        auto It = MI.getIterator();
        bool SetupFound = false;
        bool DestoryFound = false;
        // We use R14 to avoid spill of RBX
        bool ArgDefFound = false;

        auto defineR14 = [&](MachineInstr &MI) {
          for (const MCPhysReg &SubReg : TRI->subregs_inclusive(X86::R14)) {
            if (MI.definesRegister(SubReg, nullptr))
              return true;
          }
          return false;
        };

        do {
          It = std::prev(It);
          if (!SetupFound && It->getOpcode() == FrameSetupOpcode) {
            ToBeRemoved.push_back(&*It);
            SetupFound = true;
            if (ArgDefFound)
              break;
          }
          if (!ArgDefFound && defineR14(*It)) {
            ArgDefFound = true;
            if (!It->isInlineAsm()) {
              // The only valid define of R14 is InlineAsm for calling ABI0
              ToBeRemoved.push_back(&*It);
            }
            if (SetupFound)
              break;
          }
          // Optimization may move arg pass before FrameSetup.
          // e.g., os.(*rawConn).Read
          // Try our best and search in the same basic block.
        } while (It != MBB.begin());

        It = std::next(MI.getIterator());
        while (It != MBB.end()) {
          if (It->getOpcode() == FrameDestroyOpcode) {
            DestoryFound = true;
            ToBeRemoved.push_back(&*It);
            break;
          }
          It = std::next(It);
        }
        if (!SetupFound || !DestoryFound) {
          // dbgs() << "Search results: " << ArgDefFound << " " << SetupFound << " " << DestoryFound << "\n";
          // MF->dump();
          report_fatal_error("incorrect cleanup of phantom call to runtime.KeepAlive");
        }
        continue;
      }

      if (F->getName() != StringRef("runtime.gcWriteBarrier"))
        continue;

      unsigned ImplicitSrcIdx = 0;
      const char * NewSymbol = nullptr;
      for (unsigned I = 1, E = MI.getNumOperands(); I < E; I++) {
        MachineOperand &MO = MI.getOperand(I);
        if (!MO.isReg())
          continue;
        switch (MO.getReg()) {
        default:
          continue;
        case X86::RDI:
          continue;
        case X86::RAX:
          break;
        case X86::RBX:
          NewSymbol = "runtime.gcWriteBarrierBX";
          break;
        case X86::RCX:
          NewSymbol = "runtime.gcWriteBarrierCX";
          break;
        case X86::RDX:
          NewSymbol = "runtime.gcWriteBarrierDX";
          break;
        case X86::RSI:
          NewSymbol = "runtime.gcWriteBarrierSI";
          break;
        case X86::R8:
          NewSymbol = "runtime.gcWriteBarrierR8";
          break;
        case X86::R9:
          NewSymbol = "runtime.gcWriteBarrierR9";
          break;
        case X86::RBP:
          NewSymbol = "runtime.gcWriteBarrierRBP";
          break;
        }
        if (ImplicitSrcIdx == 0) {
          ImplicitSrcIdx = I;
        } else {
          report_fatal_error("found two implicit src reg");
        }
      }

      if (ImplicitSrcIdx == 0) {
        dbgs() << MI;
        report_fatal_error("cannot find implicit src reg");
      }

      if (NewSymbol != nullptr) {
        Callee.ChangeToES(NewSymbol, Callee.getTargetFlags());
        Changed = true;
      }
    }

  if (!ToBeRemoved.empty()) {
    Changed = true;
    for (auto *MI : ToBeRemoved)
      MI->eraseFromParent();
  }

  return Changed;
}
