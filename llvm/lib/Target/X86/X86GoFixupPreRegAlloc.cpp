//===- X86GoFixupPreRegAlloc.cpp - optimize usage of LEA instructions -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 ByteDance Ltd. and/or its affiliates.
//===----------------------------------------------------------------------===//
//
// This files defines fixups for Go code.
//
//===----------------------------------------------------------------------===//

#include "MCTargetDesc/X86BaseInfo.h"
#include "X86.h"
#include "X86InstrInfo.h"
#include "X86Subtarget.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/EquivalenceClasses.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/PostOrderIterator.h"
#include "llvm/CodeGen/MachineBasicBlock.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineFunctionPass.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/CodeGen/MachineOperand.h"
#include "llvm/CodeGen/MachinePostDominators.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/MachineLoopInfo.h"
#include "llvm/CodeGen/StackMaps.h"
#include "llvm/CodeGen/TargetOpcodes.h"
#include "llvm/CodeGen/TargetRegisterInfo.h"
#include "llvm/CodeGen/MachineDominators.h"
#include "llvm/InitializePasses.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Statepoint.h"
#include "llvm/TangoLLVM/TangoLLVM.h"
#include "llvm/Support/raw_ostream.h"
#include <cassert>
#include <cstdint>
#include <iterator>
#include <set>
#include <deque>

#define DEBUG_TYPE "x86-go-fixup-preregalloc"

using namespace llvm;

static cl::opt<bool>
    CloneStackPointers("x86-gogc-clone-stack-pointers", cl::Hidden, cl::init(true),
                     cl::desc("Clone LEA stack address to avoid spill and restore"));

static cl::opt<bool>
    DebugFixupPointers("debug-x86-gogc-fixup-pointers", cl::Hidden, cl::init(false),
                         cl::desc("Debug 'Fixup pointers'"));



namespace {

static void replaceReg(MachineOperand &MO, Register Reg) {
  auto subReg = MO.getSubReg();
  MO.setReg(Reg);
  MO.setSubReg(subReg);
}

static uint64_t getStatepointID(MachineInstr *MI) {
  assert(MI->getOpcode() == TargetOpcode::STATEPOINT && "must be statepoint");
  StatepointOpers SO(MI);
  uint64_t ID = MI->getOperand(SO.getIDPos()).getImm();
  return ID;
}

// If we know the MemOperand correspond to a pointer, we can infer its base pointer.
// This ignore all FrameIndex.
// The callback ProcessMemBase takes two arguments, the first is the possible base register
// and the second tells whether we are certain about the base register.
static void processMemOperandBase(MachineInstr &MI, int MemRefBegin,
    std::function<void(Register,bool)> ProcessMemBase) {
  const MachineOperand &MOBase = MI.getOperand(MemRefBegin + X86::AddrBaseReg);

  if (!MOBase.isReg()) {
    return; // may be FrameIndex
  }

  const MachineOperand &MOScale = MI.getOperand(MemRefBegin + X86::AddrScaleAmt);
  const MachineOperand &MOIndex = MI.getOperand(MemRefBegin + X86::AddrIndexReg);

  // Let's assume AddrBaseReg and AddrIndexReg contain Register
  assert(MOBase.isReg() && "invariant");
  assert(MOIndex.isReg() && "invariant");

  Register BaseReg = MOBase.getReg();
  Register IndexReg = MOIndex.getReg();

  if (Register::isVirtualRegister(BaseReg)) {
    // TODO: BaseReg may be physical register, e.g., RSP, or noreg.
    // In this case, we can conclude that MOIndex must not be a pointer.
    //
    // IndexReg and BaseReg could be the real base pointer.
    // We can make sure BaseReg is the real base if
    // 1). IndexReg is noreg
    // or
    // 2). Scale is not 1
    if (IndexReg == MCRegister::NoRegister) {
      // MOIndex is noreg and thus only AddrBaseReg can be Ptr
      ProcessMemBase(BaseReg, true);
    } else if (MOScale.isImm() && MOScale.getImm() != 1) {
      // We cannot scale on pointers so IndexReg should not be the BaseReg.
      // Also, since IndexReg is not a pointer, we can assume BaseReg may be a pointer.
      // Note that LEAQ may be used as ADD. There do exist cases where BaseReg and IndexReg
      // are neither pointers. But if MI access memory, one of them must be a pointer.
      ProcessMemBase(BaseReg, true);
    } else {
      ProcessMemBase(BaseReg, false);
      ProcessMemBase(IndexReg, false);
    }
  } else {
    // TODO: how to handle the case when BaseReg is not a virtual register?
    // BaseReg may be RSP or RIP. If Base is such a Physical register, it should not be index.
    if (BaseReg == MCRegister::NoRegister && Register::isVirtualRegister(IndexReg)) {
      if (MOScale.isImm() && MOScale.getImm() == 1) {
        // This is the rare case where IndexReg is used as the BaseReg.
        ProcessMemBase(IndexReg, true);
      }
    }
  }
}

static bool isSafeToDeleteGEP(MachineRegisterInfo *MRI, MachineInstr &MI) {
  bool SafeToDelete = true;
  // Examine each operand. See DeadMachineInstructionElim
  for (const MachineOperand &MO : MI.all_defs()) {
    Register Reg = MO.getReg();
    if (Reg.isPhysical()) {
      // The only thing we can safely delete is dead use on EFLAGS
      if (!MO.isDead() || Reg != X86::EFLAGS)
        SafeToDelete = false;
    } else {
      if (MO.isDead()) {
#ifndef NDEBUG
        // Basic check on the register. All of them should be 'undef'.
        for (auto &U : MRI->use_nodbg_operands(Reg))
          assert(U.isUndef() && "'Undef' use on a 'dead' register is found!");
#endif
        continue;
      }
      for (const MachineInstr &Use : MRI->use_nodbg_instructions(Reg)) {
        if (&Use != &MI)
          // This def has a non-debug use. Don't delete the instruction!
          SafeToDelete = false;
      }
    }
  }
  return SafeToDelete;
}

static Register isGEPWithConstantOffset(MachineInstr &MI, int64_t &Offset) {
  switch (MI.getOpcode()) {
  default:
    return 0;
  case X86::LEA64r: {
    MachineOperand &MOBase = MI.getOperand(1 + X86::AddrBaseReg);
    if (!MOBase.isReg())
      return 0; // may be frame index

    MachineOperand &MODisp = MI.getOperand(1 + X86::AddrDisp);
    if (!MODisp.isImm())
      return 0;

    MachineOperand &MOScale = MI.getOperand(1 + X86::AddrScaleAmt);
    MachineOperand &MOIndex = MI.getOperand(1 + X86::AddrIndexReg);

    assert(MOBase.isReg() && "invariant");
    assert(MOIndex.isReg() && "invariant");
    assert(MODisp.isImm() && "invariant");
    Register BaseReg = MOBase.getReg();
    Register IndexReg = MOIndex.getReg();

    if (Register::isVirtualRegister(BaseReg)) {
      if (IndexReg == MCRegister::NoRegister) {
        Offset = MODisp.getImm();
        return BaseReg; // GEP with constant offset
      }
    } else if (BaseReg == MCRegister::NoRegister) {
      if (Register::isVirtualRegister(IndexReg)) {
        if (MOScale.isImm() && MOScale.getImm() == 1) {
          Offset = MODisp.getImm();
          return IndexReg; // GEP with constant offset
        }
      }
    }

    return 0;
  }
  case X86::INC64r:
  case X86::ADD64ri8:
  case X86::ADD64ri32:
  case X86::DEC64r:
  case X86::SUB64ri8:
  case X86::SUB64ri32: {
    assert(MI.getOperand(1).isReg() && "invariant");
    Register Base = MI.getOperand(1).getReg();
    switch (MI.getOpcode()) {
    case X86::INC64r:
      Offset = 1;
      break;
    case X86::ADD64ri8:
    case X86::ADD64ri32:
      assert(MI.getOperand(2).isImm() && "invariant");
      Offset = MI.getOperand(2).getImm();
      break;
    case X86::DEC64r:
      Offset = -1;
      break;
    case X86::SUB64ri8:
    case X86::SUB64ri32:
      assert(MI.getOperand(2).isImm() && "invariant");
      Offset = -MI.getOperand(2).getImm();
      break;
    }
    return Base;
  }
  }

  return 0;
}

static void AddOperandToStatePoint(MachineInstr& MI, ArrayRef<Register> Regs) {
  assert(MI.getOpcode() == TargetOpcode::STATEPOINT);
  StatepointOpers SO(&MI);
  int64_t NumDeoptArgs = SO.getNumDeoptArgs();
  int64_t NumAddedArgs = int64_t(Regs.size());
  SmallVector<MachineOperand, 4> SavedOps;
  SavedOps.reserve(MI.getNumOperands() - SO.getNumGCPtrIdx());

  // E must be pre-computed because we have modify MI. SO may not work properly.
  unsigned E = SO.getNumGCPtrIdx() -  1;
  for (unsigned Idx = MI.getNumOperands() - 1; Idx >= E; Idx--) {
    SavedOps.push_back(MI.getOperand(Idx));
    MI.removeOperand(Idx);
  }
  for (auto R : Regs) {
    assert(Register::isVirtualRegister(R) && "invariant");
    MI.addOperand(MachineOperand::CreateReg(R, false));
  }
  for (auto &MO : llvm::reverse(SavedOps))
    MI.addOperand(MO);

  MI.getOperand(SO.getNumDeoptArgsIdx()).setImm(NumDeoptArgs + NumAddedArgs);
}

static const Register GoABIPtrReg[9] = {
  X86::RAX, X86::RBX, X86::RCX, X86::RDI, X86::RSI, X86::R8, X86::R9, X86::R10, X86::R11
};

static const char *TypeNames[8] = {
  "Unknown", "Pointer", "NotAPointer", "MayBeAPointer", "Zero", "Unsafe", "StackPointer", "NoneHeap"
};

class X86GoStatepointFixup {

  static void processFunctionArgReg(unsigned Begin, FunctionType *FTy, std::function<void(Register)> ProcessArgReg) {
    unsigned NoneFloatIndex = 0;
    unsigned NumParams = FTy->getNumParams();
    for (unsigned I = Begin; I < NumParams; ++I) {
      Type *ParamTy = FTy->getParamType(I);
      unsigned RegIdx = NoneFloatIndex;
      if (!(ParamTy->isFloatTy() || ParamTy->isDoubleTy()))
        NoneFloatIndex++;
      if (!ParamTy->isPointerTy())
        continue;
      Register ArgReg = getFunctionArgumentReg(RegIdx);
      ProcessArgReg(ArgReg);
    }
  }

  static void processFunctionReturnReg(Type *RetType, std::function<void(Register)> ProcessRetReg) {
    if (RetType->isPointerTy()) {
      ProcessRetReg(X86::RAX);
    } else if (StructType *ST = dyn_cast<StructType>(RetType)) {
      int NoneFloatIndex = 0;
      for (unsigned I = 0, E = ST->getNumElements(); I != E; ++I) {
        unsigned RegIdx = NoneFloatIndex;
        Type *ElemType = ST->getElementType(I);
        if (!(ElemType->isFloatTy() || ElemType->isDoubleTy()))
          NoneFloatIndex++;
        if (!ElemType->isPointerTy())
          continue;
        Register RetReg = getFunctionReturnReg(RegIdx);
        ProcessRetReg(RetReg);
      }
    }
  }

  static Register getFunctionReturnReg(unsigned Index) {
    assert(Index >= 0 && Index < 9 && "Go ABI");
    return GoABIPtrReg[Index];
  }

  // For pointers only
  static Register getFunctionArgumentReg(unsigned Index) {
    assert(Index >= 0 && Index < 9 && "Go ABI");
    return GoABIPtrReg[Index];
  }


  // For pointers only
  static Type* getFunctionArgumentType(Function &F, Register Reg) {
    int ArgIndex = -1;
    switch (Reg) {
      case X86::RAX:
        ArgIndex = 0;
        break;
      case X86::RBX:
        ArgIndex = 1;
        break;
      case X86::RCX:
        ArgIndex = 2;
        break;
      case X86::RDI:
        ArgIndex = 3;
        break;
      case X86::RSI:
        ArgIndex = 4;
        break;
      case X86::R8:
        ArgIndex = 5;
        break;
      case X86::R9:
        ArgIndex = 6;
        break;
      case X86::R10:
        ArgIndex = 7;
        break;
      case X86::R11:
        ArgIndex = 8;
        break;
      default:
        break;
    }

    if (ArgIndex == -1)
      return nullptr;

    if (ArgIndex >= int(F.arg_size()))
      return nullptr;

    return F.getArg(ArgIndex)->getType();
  }


  // e.g., Index
  enum {
    Unknown       = 0,
    Pointer       = 1,
    NotAPointer   = 2,
    MayBeAPointer = 3,
    Zero          = 4, // Zero could be used as a pointer or non-pointer
    Unsafe        = 5,
    StackPointer  = 6,
    NotHeap       = 7
  };

  MachineFunction *MF;
  const X86InstrInfo * TII;
  MachineRegisterInfo *MRI;
  MachineLoopInfo *MLI;
  MachineDominatorTree *MDT;
  MachinePostDominatorTree *MPDT;

  unsigned FrameSetupOpcode;
  unsigned FrameDestroyOpcode;

  // Statepoints from IR
  DenseMap<uint64_t, GCStatepointInst*> Statepoints;
  // For those pointers tracked in statepoints before fixup
  DenseSet<Register> TrackedPointers;

  // If A copied from B, then A and B are in the same class.
  // If B copied from a physical register or not defined by a copy,
  // then B is the lead.
  EquivalenceClasses<Register> CopiedRegisters;

  DenseMap<Register, unsigned> RegType;

  // If we add a pointer to a statepoint, we need to run
  // a new liveness analysis to ensure the pointer is tracked
  // by all statepoints that can reach the new statepoint.
  SmallSetVector<Register, 4> CheckingMissingStatepoints;

  // Worklist support for pointer inference
  SmallVector<Register> Worklist;
  DenseSet<Register> InWorklist;

  // Cache for pointer address check
  DenseMap<const Value*, bool> PointerAddresses;

  bool isCallGoAdHocABI(GCStatepointInst *SI) {
    if (Function * Fn = SI->getActualCalledFunction())
      return TangoLLVM::getGoAdHocABI(Fn->getName()) != GoAdHocABI::None;
    return false;
  }

  unsigned getRegType(Register Reg) {
    auto It = RegType.find(Reg);
    if (It != RegType.end()) {
      return It->second;
    }
    return Unknown;
  }

  bool isZeroValue(Register Reg) {
    if (!Register::isVirtualRegister(Reg))
      return false; // Maybe XMM15?
    if (getRegType(Reg) == Zero)
      return true;

    MachineInstr &MI = *MRI->def_begin(Reg)->getParent();

    if (MI.isCopy()) {
      assert(MI.getOperand(1).isReg() && "invariant");
      return isZeroValue(MI.getOperand(1).getReg());
    }

    switch (MI.getOpcode()) {
    default:
      break;
    case TargetOpcode::SUBREG_TO_REG:
      assert(MI.getOperand(2).isReg() && "invariant");
      return isZeroValue(MI.getOperand(2).getReg());
    case X86::MOV32r0:
      return true;
    }

    return false;
  }

  bool doUpdate(Register Reg, unsigned OldType, unsigned NewType) {
    assert(OldType != NewType && "caller check equality");
    if (DebugFixupPointers)
      dbgs() << "Update type from " << TypeNames[OldType] << " to " << TypeNames[NewType] << " for " << Register::virtReg2Index(Reg) << "\n";
    RegType[Reg] = NewType;
    return true;
  };

  bool isRegFromGEP(Register Reg) {
    if (Register::isPhysicalRegister(Reg))
      return false;

    MachineInstr &MI = *MRI->def_begin(Reg)->getParent();
    if (MI.isCopy()) {
      MachineOperand &MO1 = MI.getOperand(1);
      assert(MO1.isReg() && "invariant");
      return isRegFromGEP(MO1.getReg());
    }

    if (MI.isPHI()) {
      return false;
    }

    switch (MI.getOpcode()) {
      default:
        return false;
      case X86::LEA64r:
      case X86::INC64r:
      case X86::ADD64ri8:
      case X86::ADD64ri32:
      case X86::DEC64r:
      case X86::SUB64ri8:
      case X86::SUB64ri32:
      case X86::ADD64rr:
      case X86::SUB64rr:
      case X86::ADD64rm:
      case X86::SUB64rm:
        return true;
    }
  }

  bool isStackPointer(Register Reg) {
    return getRegType(Reg) == StackPointer;
  }

  void updateTypeToAllCopy(Register Reg, unsigned Type) {
    assert(Reg.isVirtual() && "invariant");
    auto I = CopiedRegisters.findValue(Reg);
    if (I == CopiedRegisters.end()) {
      updateType(Reg, Type);
      return;
    }
    auto L = CopiedRegisters.findLeader(I);
    if (L == CopiedRegisters.member_end()) {
      updateType(Reg, Type);
      return;
    }

    for (; L != CopiedRegisters.member_end(); ++L) {
      Register CopiedReg = *L;
      updateType(CopiedReg, Type);
    }
  }

  bool updateType(Register Reg, unsigned Type) {
    if (Reg == 0 || !Register::isVirtualRegister(Reg))
      return false;

    auto It = RegType.find(Reg);
    if (It == RegType.end()) {
      return doUpdate(Reg, Unknown, Type);
    }

    unsigned OldType = It->second;
    if (OldType == Type)
      return false;

    // StackPointer has the highest priority
    if (OldType == StackPointer)
      return false;

    if (Type == StackPointer) {
      return doUpdate(Reg, OldType, Type);
    }

    // Then none-heap pointers
    if (OldType == NotHeap)
      return false;

    if (Type == NotHeap)
      return doUpdate(Reg, OldType, Type);

    // Then Zero value
    if (OldType == Zero) {
      return false;
    }

    if (Type == Zero) {
      return doUpdate(Reg, OldType, Type);
    }

    if (OldType == MayBeAPointer) {
      assert((Type == Unsafe || Type == NotAPointer || Type == Pointer) && "invariant");
      return doUpdate(Reg, OldType, Type);
    }

    if (OldType == Unsafe)
      return false;

    assert((OldType == NotAPointer || OldType == Pointer) && "invariant");
    if (Type == MayBeAPointer) {
      return false;
    }

    if (resolveConflict(Reg, OldType, Type)) {
      return true;
    }

    return false;
  }

  bool resolveConflict(Register Reg, unsigned OldType, unsigned NewType) {
    if (isZeroValue(Reg)) {
      updateType(Reg, Zero);
      return true;
    }

    assert(OldType != NewType && "invariant");
    assert((OldType == Pointer || OldType == NotAPointer) && "invariant");
    assert((NewType == Pointer || NewType == NotAPointer) && "invariant");
    return doUpdate(Reg, OldType, Unsafe);
  }

  // Check whether V is a pointer to pointer.
  // If ALL load and store to V is a pointer value, we can conclude
  // V is a pointer to a pointer.
  bool isPointerAddr(const Value *V) {
    auto I = PointerAddresses.find(V);
    if (I != PointerAddresses.end())
      return I->second;

    // We assume that a Value is a pointer address if we can
    // load a pointer from the address or store a pointer to the address.
    bool FoundPointer = false;
    bool FoundNonPointer = false;
    for (auto &U : V->uses()) {
      if (LoadInst *LI = dyn_cast<LoadInst>(U.getUser())) {
        if (LI->getType()->isPointerTy()) {
          FoundPointer = true;
        } else {
          FoundNonPointer = true;
        }
      } else if (StoreInst *SI = dyn_cast<StoreInst>(U.getUser())) {
        if (SI->getPointerOperand() != V)
          continue; // Not write to V
        if (SI->getValueOperand()->getType()->isPointerTy()) {
          FoundPointer = true;
        } else {
          FoundNonPointer = true;
        }
      }
    }
    bool Found = FoundPointer && !FoundNonPointer;
    PointerAddresses[V] = Found;
    return Found;
  }

  void printRegType(Register Reg) {
    auto Type = getRegType(Reg);
    dbgs() << TypeNames[Type] << ": " << Register::virtReg2Index(Reg) << "\n";
  }

  bool isNotAPointer(Register Reg) {
    return getRegType(Reg) == NotAPointer;
  }

  bool isPointer(Register Reg) {
    return getRegType(Reg) == Pointer;
  }

  // Return true if Reg is a base pointer from which we need to handle
  // all derived pointers.
  bool isGCPointerBase(Register Reg) {
    auto Type = getRegType(Reg);
    return Type == Pointer || Type == Unsafe || Type == StackPointer;
  }

  // We will add these pointers to state points.
  // We ignore stack pointers here (i.e., leaq over rsp or frame index)
  // because they should be handled in cloneStackPointer.
  bool isPointerOrUnsafe(Register Reg) {
    auto Type = getRegType(Reg);
    return Type == Pointer || Type == Unsafe;
  }

  bool isPointerOrMayBeAPointerOrUnsafeOrStack(Register Reg) {
    auto Type = getRegType(Reg);
    return Type == Pointer || Type == MayBeAPointer || Type == Unsafe || Type == StackPointer;
  }

  bool isZero(Register Reg) {
    return getRegType(Reg) == Zero;
  }

  void addCopy(MachineInstr &MI) {
    MachineOperand &MO0 = MI.getOperand(0);
    MachineOperand &MO1 = MI.getOperand(1);
    if (!MO0.isReg() || !MO1.isReg())
      return;

    if (MO0.getSubReg() != 0 || MO1.getSubReg() != 0)
      return;

    Register Reg0 = MO0.getReg();
    if (!Register::isVirtualRegister(Reg0))
      return;

    Register Reg1 = MO1.getReg();
    if (!Register::isVirtualRegister(Reg1))
      return;

    CopiedRegisters.unionSets(Reg0, Reg1);
  }

  bool isClonedPtr(Register Reg1, Register Reg2) {
    if (!Register::isVirtualRegister(Reg1) || !Register::isVirtualRegister(Reg2))
      return false;

    return CopiedRegisters.isEquivalent(Reg1, Reg2);
  }

  void addToWorklistConditional(Register Reg, bool IsPointer) {
    if (IsPointer)
      addToWorklistAsPointer(Reg);
    else
      addToWorklistAsMayBeAPointer(Reg);
  }

  void addToWorklist(Register Reg) {
    assert(Register::isVirtualRegister(Reg) && "invariant");
    if (InWorklist.count(Reg) != 0)
      return;
    Worklist.push_back(Reg);
    InWorklist.insert(Reg);
  }

  Register removeFromWorklist() {
    assert(!Worklist.empty() && "must not be empty");

    Register Reg = Worklist.pop_back_val();
    InWorklist.erase(Reg);
    return Reg;
  }

  void addToWorklistAsPointer(Register Reg) {
    if (Reg == 0 || !Register::isVirtualRegister(Reg))
      return;
    if (updateType(Reg, Pointer)) {
      if (DebugFixupPointers)
        dbgs() << "Add to worklist as a pointer: " << Register::virtReg2Index(Reg) << "\n";
      addToWorklist(Reg);
    }
  }

  // TODO: this function adds the pointer Reg to the worklist
  // as a MayBeAPointer. In theory, we should check whether Reg
  // can be identified as a Pointer after some new pointers have
  // been identified. However, we currently have not implement such
  // algorithms. We left it as future work if the current implementation
  // does not work well.
  void addToWorklistAsMayBeAPointer(Register Reg) {
    if (Reg == 0 || !Register::isVirtualRegister(Reg))
      return;
    if (updateType(Reg, MayBeAPointer))
      addToWorklist(Reg);
  }

  bool isCopyFromPhysReg(MachineInstr &MI) {
    if (!MI.isCopy())
      return false;
    MachineOperand &MO1 = MI.getOperand(1);
    if (!MO1.isReg())
      return false;
    return Register::isPhysicalRegister(MO1.getReg());
  }

  bool checkRegSameValue(MachineInstr &MI, Register Reg2) {
    assert(Register::isVirtualRegister(Reg2) && "must be virtual register");
    MachineInstr &MI2 = *MRI->def_begin(Reg2)->getParent();

    if (TII->produceSameValue(MI, MI2, MRI)) {
      if (isCopyFromPhysReg(MI)) {
        return false;
      }
      return true;
    }
    return false;
  }

  bool isStatepointDeoptUse(MachineInstr *MI, unsigned No) {
    if (MI->getOpcode() != TargetOpcode::STATEPOINT)
      return false;

    StatepointOpers SO(MI);
    unsigned Idx = SO.getNumDeoptArgsIdx();
    unsigned E = SO.getNumGCPtrIdx();
    return No >= Idx && No < E;
  }

  bool removeUnusedPointer(Register Reg) {
    Register Base = isGEPWithConstantOffset(Reg);
    if (Base == 0)
      return false;
    return removeUnusedPointer(Reg, Base);
  }

  // Try to remove a no-real-use Reg from all statepoints.
  // If we can remove the Reg, we should add its Base to
  // the corresponding statepoint to keep objects live.
  // NOTE: the caller is responsible to delete the MI that
  // defines Reg.
  bool removeUnusedPointer(Register Reg, Register Base);
  bool checkUnusedPointers();

  void collectGCStatepoints();
  void processMemoryMI(MachineInstr &MI);
  Register isGEPWithConstantOffset(Register Reg) {
    MachineInstr &MI = *MRI->def_begin(Reg)->getParent();
    int64_t Offset = 0;
    return ::isGEPWithConstantOffset(MI, Offset);
  }
  void processBase(Register VirtReg, MachineInstr &MI);
  void processDerived(Register BaseReg, unsigned No, MachineInstr &MI);
  void collectUntrackedPointers(DenseMap<Register, SmallVector<MachineInstr*, 4>> &PointerToStatepoints,
    std::function<bool(Register)> NeedTrack);
  void collectPointers();
  void collectCopy();
  void collectZeroAndNoneHeapPointerValues();
  void collectInitialPointers();

  bool checkStatepoints(std::function<bool(Register)> NeedTrack);
  Register clonePointerBeforeNonPHIUse(Register Reg, MachineInstr *DefMI, MachineBasicBlock *PosMBB, MachineInstr *Pos);
  // We may need to make multiple copy for the PHI because PHI may share the same value for different incoming block.
  void clonePointerBeforePHIUse(Register Reg, MachineInstr *DefMI, MachineInstr *UseMI);
public:
  X86GoStatepointFixup(MachineFunction *MF, const X86InstrInfo *TII,
      MachineRegisterInfo *MRI, MachineLoopInfo *MLI, MachineDominatorTree *MDT,
      MachinePostDominatorTree *MPDT)
    : MF(MF), TII(TII), MRI(MRI), MLI(MLI), MDT(MDT), MPDT(MPDT),
    FrameSetupOpcode(TII->getCallFrameSetupOpcode()),
    FrameDestroyOpcode(TII->getCallFrameDestroyOpcode()) {}

  bool run();

};

class X86GoFixupPreRegAllocPass : public MachineFunctionPass {
public:
  X86GoFixupPreRegAllocPass() : MachineFunctionPass(ID) {
    initializeX86GoFixupPreRegAllocPassPass(*PassRegistry::getPassRegistry());
  }

  StringRef getPassName() const override { return "X86 Go Fixup PreRegAlloc"; }

  bool runOnMachineFunction(MachineFunction &MF) override;

  static char ID;

  void getAnalysisUsage(AnalysisUsage &AU) const override {
    AU.addRequired<MachinePostDominatorTreeWrapperPass>();
    AU.addRequired<MachineDominatorTreeWrapperPass>();
    AU.addRequired<MachineLoopInfoWrapperPass>();
    AU.addPreserved<MachinePostDominatorTreeWrapperPass>();
    AU.addPreserved<MachineDominatorTreeWrapperPass>();
    AU.addPreserved<MachineLoopInfoWrapperPass>();
    MachineFunctionPass::getAnalysisUsage(AU);
  }

private:
  MachineFunction *MF = nullptr;
  const X86RegisterInfo *TRI = nullptr;
  const X86InstrInfo * TII = nullptr;
  MachineRegisterInfo *MRI = nullptr;
  MachineLoopInfo *MLI = nullptr;
  MachineDominatorTree *MDT = nullptr;
  MachinePostDominatorTree *MPDT = nullptr;

  bool isGoStackPointerRegister(Register Reg) {
    StringRef RegName = TRI->getRegAsmName(Reg);
    return RegName == "RSP";
  }

  bool optimizeOpSP();
  bool foldStackPointers();
  bool cloneStackPointers();
  bool cloneRdiIfNeeded();
};

} // end anonymous namespace

char X86GoFixupPreRegAllocPass::ID = 0;

FunctionPass *llvm::createX86GoFixupPreRegAlloc() { return new X86GoFixupPreRegAllocPass(); }
INITIALIZE_PASS_BEGIN(X86GoFixupPreRegAllocPass, DEBUG_TYPE, "X86 Fixup prealloc pass", false, false)
INITIALIZE_PASS_DEPENDENCY(MachineDominatorTreeWrapperPass)
INITIALIZE_PASS_DEPENDENCY(MachinePostDominatorTreeWrapperPass)
INITIALIZE_PASS_DEPENDENCY(MachineLoopInfoWrapperPass)
INITIALIZE_PASS_END(X86GoFixupPreRegAllocPass, DEBUG_TYPE, "X86 Fixup prealloc pass", false, false)

struct RemoveRedundantCopy {
  MachineInstr * Copy;
  Register Src; // SP
  Register Dst; // to be replaced

  bool replace(MachineInstr &MI);
};

bool RemoveRedundantCopy::replace(MachineInstr &MI) {
  bool Changed = false;
  for (MachineOperand &MO : MI.operands()) {
    if (MO.isReg() && MO.getReg() == Dst) {
      if (MO.isDef()) {
        report_fatal_error("cannot define in function body");
      }
      replaceReg(MO, Src);
      Changed = true;
    }
  }
  return Changed;
}


bool X86GoFixupPreRegAllocPass::runOnMachineFunction(MachineFunction &fn) {
  MF = &fn;
  // This is used for Go function only.
  if (!MF->getFunction().hasGC() || MF->getFunction().getGC() != "go") {
    return false;
  }

  TRI = MF->getSubtarget<X86Subtarget>().getRegisterInfo();
  TII = MF->getSubtarget<X86Subtarget>().getInstrInfo();
  MRI = &MF->getRegInfo();

  MDT = &getAnalysis<MachineDominatorTreeWrapperPass>().getDomTree();
  MPDT = &getAnalysis<MachinePostDominatorTreeWrapperPass>().getPostDomTree();
  MLI = &getAnalysis<MachineLoopInfoWrapperPass>().getLI();

  bool Changed = false;

  Changed |= optimizeOpSP();
  Changed |= foldStackPointers();
  Changed |= cloneStackPointers();
  Changed |= cloneRdiIfNeeded();

  X86GoStatepointFixup Fixup(MF, TII, MRI, MLI, MDT, MPDT);
  Changed |= Fixup.run();

  return Changed;
}

static bool isTwoAddrUse(MachineInstr &MI, Register Reg, Register &DstReg) {
  for (unsigned i = 0, NumOps = MI.getNumOperands(); i != NumOps; ++i) {
    const MachineOperand &MO = MI.getOperand(i);
    if (!MO.isReg() || !MO.isUse() || MO.getReg() != Reg)
      continue;
    unsigned ti;
    if (MI.isRegTiedToDefOperand(i, &ti)) {
      DstReg = MI.getOperand(ti).getReg();
      return true;
    }
  }
  return false;
}

static bool usedInPhi(MachineInstr &MI, std::set<MachineInstr*> &visited) {
  if (!visited.insert(&MI).second) {
    return false;
  }
  bool result = false;
  assert(MI.getNumDefs() > 0 && "COPY must have 1 def.");

  unsigned Reg = MI.getOperand(0).getReg();
  if (Register::isPhysicalRegister(Reg)) {
    return false;
  }
  llvm::MachineFunction *MF = MI.getParent()->getParent();
  llvm::MachineRegisterInfo &MRI = MF->getRegInfo();

  for (auto &Use : MRI.use_nodbg_instructions(Reg)) {
    if (Use.isPHI()) {
      return true;
    }
  }
  return result;
}

bool X86GoFixupPreRegAllocPass::optimizeOpSP() {
  bool Changed = false;

  while (true) {
    std::vector<RemoveRedundantCopy> RemoveOpSP;

    for (auto &MBB : *MF) {
      for (auto &MI : MBB) {
        if (MI.isCopy()) {
          const MachineOperand &Dst = MI.getOperand(0);
          const MachineOperand &Src = MI.getOperand(1);
          if (Src.isReg() && Register::isPhysicalRegister(Src.getReg()) && isGoStackPointerRegister(Src.getReg())) {
            if (!Dst.isReg() || !Register::isVirtualRegister(Dst.getReg())) {
              continue;
            }
            std::set<MachineInstr*> visited;
            if (usedInPhi(MI, visited)) {
              continue;
            }
            RemoveOpSP.push_back({&MI, Src.getReg(), Dst.getReg()});
          }
          continue;
        }
      }
    }

    if (RemoveOpSP.empty()) {
      break;
    }

    for (auto & Op : RemoveOpSP) {
      for (auto &MBB : *MF) {
        std::vector<MachineInstr*> ToBeReplaced;
        for (auto &MI : MBB) {
          if (Op.Copy == &MI) {
            continue;
          }
          Register DstReg;
          if (isTwoAddrUse(MI, Op.Dst, DstReg)) {
            switch (MI.getOpcode()) {
              case X86::ADD64ri8:
              case X86::ADD64ri32: {
                // TODO: duffzero may produce -0x20, rsp
                // cannot access out-of-bounds value with OpSP
                // MachineOperand &Imm = MI.getOperand(2);
                // assert(Imm.isImm() && Imm.getImm() >= 0 && "invariant: must be positive imm");
                break;
              }
              case X86::SUB64ri8:
              case X86::SUB64ri32: {
                // MachineOperand &Imm = MI.getOperand(2);
                // // cannot access out-of-bounds value with OpSP
                // assert(Imm.isImm() && Imm.getImm() <= 0 && "invariant: must be negative imm");
                break;
              }
              case X86::INC64r:
                break;
              default:
#if !defined(NDEBUG) || defined(LLVM_ENABLE_DUMP)
                dbgs() << "cannot handle MI: " << MI << "\n";
                MF->dump();
#endif
                report_fatal_error("TODO: do not know how to handle two address");
            }

            // we cannot define RSP here so we cannot simply replace rsp
            ToBeReplaced.push_back(&MI);
            continue;
          }
          // other cases:
          Op.replace(MI);
        }
        for (MachineInstr *MI : ToBeReplaced) {
          // TODO: it is very dangerous here because ADD/SUB may redefine eflags
          // but LEAQ does not. Fortunately, OpSP should only be used compute address.
          // Nevertheless, we still need add some check here.
          MachineInstr *NewMI = TII->convertToThreeAddress(*MI, nullptr, nullptr);
          if (!NewMI)
            report_fatal_error("cannot convert to three address");
          Op.replace(*NewMI);
          MI->eraseFromParent();
        }
      }
      Op.Copy->eraseFromParent();
    }

    Changed = true;
  }

  return Changed;
}

// Index may not be constant and Scale may not be 1.
static bool isConstantStackAddress(MachineInstr &MI, int64_t &Offset) {
  // LEA64r
  if (MI.getOpcode() != X86::LEA64r)
    return false;

  MachineOperand &MOBase = MI.getOperand(1 + X86::AddrBaseReg);
  // We only care about RSP
  if (!MOBase.isFI() && !(MOBase.isReg() && MOBase.getReg() == X86::RSP))
    return false;

  MachineOperand &MODisp = MI.getOperand(1 + X86::AddrDisp);
  assert(MODisp.isImm() && "invariant");

  Offset = MODisp.getImm();
  return true;
}

// We must run foldStackPointers before cloneStackPointers.
// By this way, all newly created stack pointers can be fixed
// by cloneStackPointers as well.
bool X86GoFixupPreRegAllocPass::foldStackPointers() {
  bool Changed = false;
  auto onlyImplicitlyDefinedDeadEFLAGS = [&](MachineInstr &MI) {
    for (const MachineOperand &MO : MI.implicit_operands()) {
      assert(MO.isReg() && "invariant");
      if (MO.isDead())
        continue;
      if (MO.isDef() && MO.isImplicit()) {
        if (!(MO.isReg() && MO.getReg() == X86::EFLAGS)) {
          report_fatal_error("TODO: cannot handle implicitly defined values");
        }
        return false;
      }
    }
    return true;
  };

  for (MachineBasicBlock &MBB : *MF) {
    for (MachineInstr &MI : MBB) {
      int64_t Offset = 0;
      if (isConstantStackAddress(MI, Offset)) {
        // Now check all use instrs
        assert(MI.getOperand(0).isReg() && "invariant");
        Register Reg = MI.getOperand(0).getReg();
        SmallSetVector<MachineInstr*, 4> UseMIs;
        for (auto &UseMI : llvm::make_early_inc_range(MRI->use_instructions(Reg))) {
          if (&UseMI == &MI)
            continue;
          int64_t Offset2 = 0;
          if (isGEPWithConstantOffset(UseMI, Offset2)) {
            // TODO: Offset + Offset2 should be in bounds.
            if (onlyImplicitlyDefinedDeadEFLAGS(UseMI)) {
              // If only dead eflags is implicitly defined, we can replace this GEP with a LEA
              // We can clone the
              Register UseReg = UseMI.getOperand(0).getReg();
              Register NewReg = MRI->createVirtualRegister(MRI->getRegClass(UseReg));
              // Create a NEW LEA64r by cloning the base pointer
              MachineInstr &NewMI = TII->duplicate(*UseMI.getParent(), UseMI.getIterator(), MI);
              assert(NewMI.getOpcode() == X86::LEA64r && "invariant");
              // LEAQ
              assert(NewMI.getOperand(0).isReg() && "invariant");

              // update the cloned Reg to new reg
              replaceReg(NewMI.getOperand(0), NewReg);
              NewMI.getOperand(1 + X86::AddrDisp).setImm(Offset + Offset2);

              MRI->replaceRegWith(UseReg, NewReg);
              UseMI.eraseFromParent();
              Changed = true;
            }
          }
        }
      }
    }
  }


  return Changed;
}


// If a stack address load instruction lives through a statepoint,
// we should copy it to a position after the statepoint.
// By this case, we can avoid spill and restore stack locations.
// LEAQ has no side-effects. We should be able to insert LEAQ in any places.
bool X86GoFixupPreRegAllocPass::cloneStackPointers() {
  if (!CloneStackPointers)
    return false;

  DenseMap<Register, MachineInstr*> Reg2StackAddrLoad;
  int FrameIndex;
  for (MachineBasicBlock &MBB : *MF)
    for (MachineInstr &MI : MBB)
      if (auto Reg = TII->isLoadStackSlotAddress(MI, FrameIndex))
        if (Register::isVirtualRegister(Reg) && !MRI->use_empty(Reg)) {
          Reg2StackAddrLoad[Reg] = &MI; // record for late copy
        }

  if (Reg2StackAddrLoad.empty())
    return false;

  bool Changed = false;

  // Map to frame
  DenseMap<MachineBasicBlock*, DenseMap<Register, Register>> LiveOutValues;

  // We need to record the first use point of a reset stack address.
  // An instruction may access multiple pointers, e.g., move a stack
  // pointer into another stack pointer. So we need to use a map.
  DenseMap<MachineInstr *, DenseMap<Register, Register>> Redefine;

  std::deque<MachineBasicBlock*> Worklist;
  BitVector InList(MF->getNumBlockIDs());

  auto JoinDataFlow = [&](MachineBasicBlock *MBB, DenseMap<Register, Register> &Live) {
    for (MachineBasicBlock *Pred : MBB->predecessors()) {
      DenseMap<Register, Register> &PredLiveOut = LiveOutValues[Pred];
      for (auto It : PredLiveOut) {
        auto It2 = Live.find(It.first);
        // should not exist conflict
        if (It2 != Live.end()) {
          if (It2->second != It.second) {
            // Conflicts may happen. If happened, we must introduce a new virtual register
            // here to resolve the conflict. Here we insert 0 and let following uses to
            // to create a new virtual register.
            It2->second = 0;
          }
        } else {
          Live.insert(It);
        }
      }
    }
  };

  auto ProcessMBB = [&](MachineBasicBlock *MBB) {
    // Step 0: initialize the LiveIn of the first instruction.
    DenseMap<Register, Register> Live;
    JoinDataFlow(MBB, Live);

    for (auto &MI : *MBB) {
      // PHI is handled differently. If a stack address is used by PHI, we may need
      // to store its value to stack or copy to some physical address to support PHI.
      if (MI.isPHI())
        continue;

      // Live is the LiveIn of MI.
      if (auto Reg = TII->isLoadStackSlotAddress(MI, FrameIndex)) {
        Live.insert(std::make_pair(Reg, Reg));
        continue;
      }

      // Add uses
      for (const MachineOperand &MO : MI.operands()) {
        if (MO.isReg() && MO.isUse() && Register::isVirtualRegister(MO.getReg())) {
          Register Reg = MO.getReg();
          if (Reg2StackAddrLoad.count(Reg)) {
            assert(Live.count(Reg) && "must be defined");
            Register NewReg = Live[Reg];

            if (NewReg == 0) {
              DenseMap<Register, Register> &RDefs = Redefine[&MI];
              if (RDefs.count(Reg)) {
                NewReg = RDefs[Reg];
                assert(NewReg != 0 && "invariant");
              } else {
                // TODO: assert 64-bit
                Register NewReg = MRI->createVirtualRegister(MRI->getRegClass(MO.getReg()));
                RDefs[Reg] = NewReg;
              }
              Live[Reg] = NewReg;
            }
          }
        }
      }

      // add kill
      if (MI.getOpcode() == TargetOpcode::STATEPOINT) {
        // A statepoint resets all defined stack address.
        for (auto &It : Live) {
          It.second = 0;
        }
      }
    }

    // Update Worklist
    DenseMap<Register, Register> &LiveOut = LiveOutValues[MBB];
    if (Live != LiveOut) {
      LiveOut = Live;
      for (MachineBasicBlock * Succ: MBB->successors()) {
        if (InList.test(Succ->getNumber()))
          continue;
        Worklist.push_back(Succ);
        InList.set(Succ->getNumber());
      }
    }
  };

  // inverse post order. Here, post_order should be efficient.
  std::vector<MachineBasicBlock*> IPO;
  IPO.reserve(MF->getNumBlockIDs());
  for (MachineBasicBlock *MBB : post_order(MF)) {
    IPO.push_back(MBB);
  }

  // forward data flow analysis
  for (MachineBasicBlock *MBB : llvm::reverse(IPO)) {
    ProcessMBB(MBB);
  }

  // solving: propagate until reaching the fixed point
  while (!Worklist.empty()) {
    MachineBasicBlock *MBB = Worklist.front();
    Worklist.pop_front();
    InList.reset(MBB->getNumber());
    ProcessMBB(MBB);
  }

  // Map new reg to the defining instruction
  DenseMap<Register, MachineInstr*> Cloned;
  // epilogue: insert LEAQ
  for (MachineBasicBlock *MBB : llvm::reverse(IPO)) {
    // still compute a valid live-in
    DenseMap<Register, Register> Live;
    JoinDataFlow(MBB, Live);

    for (MachineInstr &MI : *MBB) {
      if (MI.isPHI())
        continue;

      // Live is the LiveIn of MI.
      if (auto Reg = TII->isLoadStackSlotAddress(MI, FrameIndex)) {
        Live.insert(std::make_pair(Reg, Reg));
      } else {
        // Now insert redefine introduced by use
        if (Redefine.count(&MI)) {
          DenseMap<Register, Register> &RDefs = Redefine[&MI];
          for (auto It : RDefs) {
            Live[It.first] = It.second;
          }
        }
      }

      // Check use
      for (MachineOperand &MO : MI.operands()) {
        if (MO.isReg() && MO.isUse() && Register::isVirtualRegister(MO.getReg())) {
          Register Reg = MO.getReg();
          if (Reg2StackAddrLoad.count(Reg)) {
            assert(Live.count(Reg) && "must be defined");
            Register NewReg = Live[Reg];
            assert(NewReg != 0 && "must create first");
            if (NewReg != Reg) {
              if (!Cloned.count(NewReg)) {
                MachineInstr *OldMI = Reg2StackAddrLoad[Reg];
                assert(OldMI != nullptr && "invariant");
                MachineInstr &NewMI = TII->duplicate(*MBB, MI.getIterator(), *OldMI);
                // LEAQ
                assert(NewMI.getOperand(0).isReg() && "invariant");
                assert(NewMI.getOperand(0).getReg() == MO.getReg() && "invariant");

                // update the cloned Reg to new reg
                replaceReg(NewMI.getOperand(0), NewReg);
                Cloned[NewReg] = &NewMI;
              }
              // Now update MO
              replaceReg(MO, NewReg);
              Changed = true;
            }
          }
        }
      }

      // update KILL
      if (MI.getOpcode() == TargetOpcode::STATEPOINT) {
        // A statepoint kills all defined stack address.
        for (auto &It : Live) {
          It.second = 0;
        }
      }
    }
  }

  // For PHI
  for (MachineBasicBlock &MBB : *MF) {
    for (MachineInstr &MI : MBB) {
      if (!MI.isPHI())
        continue;

      for (unsigned I = 1, E = MI.getNumOperands(); I != E; I += 2) {
        Register Reg = MI.getOperand(I).getReg();
        if (!Register::isVirtualRegister(Reg))
          continue;
        if (!Reg2StackAddrLoad.count(Reg)) {
          continue;
        }
        MachineBasicBlock *PreMBB = MI.getOperand(I + 1).getMBB();
        DenseMap<Register, Register> &LiveOut = LiveOutValues[PreMBB];
        auto It = LiveOut.find(Reg);
        if (It != LiveOut.end() && It->second != 0) {
          replaceReg(MI.getOperand(I), It->second);
        } else {
          // insert a new cloned LEAQ
          MachineInstr *OldMI = Reg2StackAddrLoad[Reg];
          assert(OldMI != nullptr && "invariant");

          MachineInstr *Pos = nullptr;
          for (auto &MI : llvm::reverse(*PreMBB)) {
            if (MI.isPHI() || MI.getOpcode() == TargetOpcode::STATEPOINT) {
              break;
            }
            if (OldMI == &MI) {
              Pos = nullptr;
              break;
            }
            Pos = &MI;
            if (MI.isBranch())
              continue;
          }

          if (Pos != nullptr) {
            Register NewReg = MRI->createVirtualRegister(MRI->getRegClass(Reg));
            MachineInstr &NewMI = TII->duplicate(*PreMBB, Pos->getIterator(), *OldMI);
            // LEAQ
            assert(NewMI.getOperand(0).isReg() && "invariant");
            assert(NewMI.getOperand(0).getReg() == MI.getOperand(I).getReg() && "invariant");

            // update the cloned Reg to new reg
            replaceReg(NewMI.getOperand(0), NewReg);
            Changed = true;

            // Now update MO
            replaceReg(MI.getOperand(I), NewReg);
            // Modifies LiveOut anyway
            LiveOut[Reg] = NewReg;
          }
        }
      }
    }
  }

  if (Changed) {
    for (auto &It : Reg2StackAddrLoad) {
      Register Reg = It.second->getOperand(0).getReg();
      if (MRI->use_empty(Reg)) {
        // Remove the unused original LEA
        It.second->eraseFromParent();
      }
    }
  }

  return Changed;
}

// entry:
//   lea 0x40(%rsp),%rcx
//   add $-0x20,%rcx
//   mov %rcx,0x1c8(%rsp)
//...
// loope_header:
//   statepoint             // 0x1c8(%rsp) not get updated when copystack
//   mov 0x1c8(%rsp), rdi
//   duffzero
//   ...
// loop_body:
//   mov 0xd0(rsp), rax
bool X86GoFixupPreRegAllocPass::cloneRdiIfNeeded() {
  bool Changed = false;
  uint16_t ChangedNum = 0;

  auto GetFuncName = [](MachineInstr* MI) -> StringRef {
    if (MI->isCall()) {
      for (const MachineOperand &MO : MI->operands()) {
        if (MO.isGlobal()) {
          const GlobalValue *GV = MO.getGlobal();
          if (const Function *F = dyn_cast<Function>(GV)) {
            return F->getName();
          }
        }
      }
    }
    return "";
  };
  // duffzero -> rdi
  DenseMap<MachineInstr*, MachineInstr*> Calls;
  for (MachineBasicBlock &MBB : *MF) {
    for (MachineInstr &MI : MBB) {
      if (GetFuncName(&MI) == "runtime.duffzero") {
        MachineInstr *Pre = MI.getPrevNode();
        if (MachineOperand &MO = Pre->getOperand(0);
            Pre->isCopy() && MO.isReg() && MO.getReg() == X86::RDI &&
            Register::isVirtualRegister(Pre->getOperand(1).getReg()))
          Calls.insert({&MI, Pre});
      }
    }
  }
  if (Calls.empty())
    return false;

  auto GetDef = [&](MachineInstr* MI, uint8_t& Found) -> MachineInstr* {
    int FrameIndex;
    Register Reg = TII->isLoadStackSlotAddress(*MI, FrameIndex);
    if (Reg) {
      Found |= 0x2;
      return nullptr;
    }
    if (MI->isCopy() && MI->getOperand(1).isReg()) {
      Reg = MI->getOperand(1).getReg();
    } else if (MI->getOpcode() == X86::ADD64ri32 ||
               MI->getOpcode() == X86::ADD64ri8 ||
               MI->getOpcode() == X86::SUB64ri32 ||
               MI->getOpcode() == X86::SUB64ri8) {
      Found |= 0x1;
      Reg = MI->getOperand(1).getReg();
    }
    if (Register::isVirtualRegister(Reg))
      return MRI->getVRegDef(Reg);

    return nullptr;
  };

  for (auto &P : Calls) {
    SmallVector<MachineInstr*> Stack;
    MachineInstr* Src = P.second;
    // todo cross state point check
    uint8_t Found = 0x0;
    while (MachineInstr* Def = GetDef(Src, Found)) {
      Stack.push_back(Def);
      Src = Def;
    }
    if (Found != 0x3)
      continue;

    Changed = true;
    ChangedNum++;

    Register PreReg = MCRegister::NoRegister;
    SmallVector<MachineInstr*> OldMIs;
    while (!Stack.empty()) {
      MachineInstr *OldMI = Stack.pop_back_val();
      OldMIs.push_back(OldMI);
      Register OldReg = OldMI->getOperand(0).getReg();
      unsigned OldSubReg = OldMI->getOperand(0).getSubReg();
      Register NewReg = MRI->createVirtualRegister(MRI->getRegClass(OldReg));
      MachineInstr &NewMI = TII->duplicate(*P.first->getParent(), P.second, *OldMI);
      NewMI.getOperand(0).setReg(NewReg);
      if (OldSubReg)
        NewMI.getOperand(0).setSubReg(OldSubReg);
      if (Register::isVirtualRegister(PreReg)) {
        unsigned OldPreSubReg = OldMI->getOperand(1).getSubReg();
        NewMI.getOperand(1).setReg(PreReg);
        if (OldPreSubReg)
          NewMI.getOperand(1).setSubReg(OldPreSubReg);
      }
      PreReg = NewReg;
      if (Stack.empty())
        P.second->getOperand(1).setReg(NewReg);
    }
    while (!OldMIs.empty()) {
      MachineInstr* MI = OldMIs.pop_back_val();
      if (MRI->use_empty(MI->getOperand(0).getReg()))
        MI->eraseFromParent();
    }
  }

  return Changed;
}


void X86GoStatepointFixup::collectGCStatepoints() {
  for (auto &I : instructions(MF->getFunction())) {
    if (GCStatepointInst* SI = dyn_cast<GCStatepointInst>(&I)) {
      Statepoints[SI->getID()] = SI;
    }
  }
}

bool X86GoStatepointFixup::run() {
  // Collect GCStatepoint in LLVM IR
  collectGCStatepoints();

  if (Statepoints.empty())
    return false;

  // Run collect pointers
  collectPointers();

  bool Changed = checkUnusedPointers();

  if (DebugFixupPointers) {
    dbgs() << "Dump pointer type for " << MF->getName() << "\n";
    for (auto It : RegType) {
      Register Reg = It.first;
      unsigned Type = It.second;

      MachineInstr &MI = *MRI->def_begin(Reg)->getParent();
      dbgs() << TypeNames[Type] << ": " << Register::virtReg2Index(Reg) << "\n";
    }
  }

  // We track MayBeAPointer here for diagnostic
  // TODO: we cannot prove a MayBeAPointer is a real pointer.
  // We only process Pointer because we assume that the GoStatepoints pass at LLVM IR level can precisely
  // capture all pointers.
  // TODO: we can identify a pointer if it is used by the state point and memory read and write.
  // We may miss a pointer if the pointer is the result value of a load or the source value of a store.
  auto NeedTrack = [&](Register Reg) {
    auto Type = getRegType(Reg);
    return Type == Pointer || Type == Unsafe;
  };

  Changed |= checkStatepoints(NeedTrack);

  // We need to repeatedly run checkStatepoints
  // because once we cloned a GEP, we may need to run the check again
  // to make sure that the base of the GEP is live until the cloned GEP is defined.
  while (!CheckingMissingStatepoints.empty()) {
    // Make a copy;
    SmallSetVector<Register, 4> NewPointers = CheckingMissingStatepoints;
    CheckingMissingStatepoints.clear();
    // We do incremental checking
    // TODO: here we need to also repeatedly clone stack pointers if
    // the stack pointer is the base of any moved GEP.
    Changed |= checkStatepoints([&](Register Reg) {
      if (NewPointers.count(Reg)) {
        assert(NeedTrack(Reg) && "must be pointer");
        return true;
      }
      return false;
    });
  }
  return Changed;
}

// This function attempts to infer the base pointer of a memory operand.
// NOTE: we may miss some important pointers. If that untracked pointer triggers a bug,
// we should try to figure out which optimization leads to the untracked pointer.
void X86GoStatepointFixup::processMemoryMI(MachineInstr &MI) {
  if (!MI.mayLoadOrStore())
    return;
  // TRAP is a special instruction has mayLoadOrStore
  if (MI.getOpcode() == X86::TRAP)
    return;
  if (DebugFixupPointers)
    dbgs() << "Process memory MI: " << MI << "\n";
  // Not all MMO relate to load or store of memory.
  // This excludes LEA. If MI access memory, the MMO must relate to a pointer.
  const MCInstrDesc &Desc = MI.getDesc();
  // MemoryOperandNo returns the first Idx of the MMO.
  int MemRefBegin = X86II::getMemoryOperandNo(Desc.TSFlags);
  auto *MMO = MI.memoperands_empty() ? nullptr : MI.memoperands().front();
  bool HasMMOValue = MMO != nullptr && MMO->getValue() != nullptr;
  if (MemRefBegin >= 0 && HasMMOValue) {
    const Value *Ptr = MMO->getValue();
    // TODO: Ptr is the base pointer.
    if (isPointerAddr(Ptr) && MMO->getOffset() == 0) {
      // Check load or store
      switch (MI.getOpcode()) {
      case X86::MOV64mr: {
        // store
        assert(MI.getOperand(X86::AddrNumOperands).isReg() && "invariant");
        Register Val = MI.getOperand(X86::AddrNumOperands).getReg();
        if (DebugFixupPointers) {
          dbgs() << "Find a stored ptr: " << Register::virtReg2Index(Val) << " " << *Ptr << "\n";
        }
        addToWorklistAsPointer(Val);
        break;
      }
      case X86::MOV64rm: {
        assert(MI.getOperand(0).isReg() && "invariant");
        Register Val = MI.getOperand(0).getReg();
        if (DebugFixupPointers) {
          dbgs() << "Find a loaded ptr: " << Register::virtReg2Index(Val) << " " << *Ptr << "\n";
        }
        addToWorklistAsPointer(Val);
        break;
      }
      default:
        break;
      }
    }

    // If a MemRef correspond to a MMO IR Value, it is likely to be a pointer
    // that relates to some IR value computed in GoStatepoints.cpp.
    // Here, we may still miss some pointers that are produced by DAG or MIR optimizations.
    // An example is duffcopy/duffzero. For those pointers, we should fix it in other places
    // use different specific approaches.
    MemRefBegin += X86II::getOperandBias(Desc);
    processMemOperandBase(MI, MemRefBegin, [&](Register Reg, bool IsPointer) {
          addToWorklistConditional(Reg, IsPointer);
        });
  }
}

void X86GoStatepointFixup::processBase(Register VirtReg, MachineInstr &MI) {
  if (DebugFixupPointers)
    dbgs() << "Process base: " << MI << "\n";

  auto Opcode = MI.getOpcode();

  // Find the base pointer if there is one.
  switch (Opcode) {
  default: {
    if (MI.isCopy()) {
      const MachineOperand &MO = MI.getOperand(1);
      if (MO.isReg() && MO.getSubReg() == 0) {
        Register SrcReg = MO.getReg();
        addToWorklistAsPointer(SrcReg);
      }
    } else if (MI.isPHI()) {
      for (unsigned I = 1, E = MI.getNumOperands(); I != E; I += 2) {
        assert(MI.getOperand(I).isReg() && "invariant");
        Register Reg = MI.getOperand(I).getReg();
        // addToWorklistAsPointer(Reg);
        // Reg may be a zero.
        addToWorklistAsMayBeAPointer(Reg);
      }
    } else {
      // We have to assume that MI itself is the base pointer.
      // dbgs() << "NoBaseMI: " << MI << "\n";
    }
    break;
  }
  case X86::CMOV64rm: {
    // CMOV is conditional. The inputs may be a Zero or a Pointer.
    // Since we have no state that can indicate a value is MayBeZeroOrPointer,
    // we use MayBeAPointer instead.
    assert(MI.getOperand(1).isReg() && "invariant");
    addToWorklistAsMayBeAPointer(MI.getOperand(1).getReg());
    break;
  }
  case X86::CMOV64rr: {
    assert(MI.getOperand(1).isReg() && "invariant");
    assert(MI.getOperand(2).isReg() && "invariant");
    addToWorklistAsMayBeAPointer(MI.getOperand(1).getReg());
    addToWorklistAsMayBeAPointer(MI.getOperand(2).getReg());
    break;
  }
  case X86::LEA64r: {
    processMemOperandBase(MI, 1, [&](Register Reg, bool IsPointer) {
          addToWorklistConditional(Reg, IsPointer);
        });
    break;
  }
  case X86::INC64r:
  case X86::DEC64r:
  case X86::ADD64ri8:
  case X86::ADD64ri32:
  case X86::SUB64ri8:
  case X86::SUB64ri32: {
    const MachineOperand &MO = MI.getOperand(1);
    assert(MO.isReg() && "invariant");
    addToWorklistAsPointer(MO.getReg());
    break;
  }
  case X86::ADD64rr: {
    Register Reg1 = MI.getOperand(1).getReg();
    Register Reg2 = MI.getOperand(2).getReg();
    addToWorklistAsMayBeAPointer(Reg1);
    addToWorklistAsMayBeAPointer(Reg2);
    break;
  }
  case X86::SUB64rr: {
    Register Reg1 = MI.getOperand(1).getReg();
    addToWorklistAsMayBeAPointer(Reg1);
    break;
  }
  case X86::ADD64rm:
  case X86::SUB64rm: {
    const MachineOperand &Op1 = MI.getOperand(1);
    assert(Op1.isReg() && "invariant");
    Register Reg1 = Op1.getReg();
    addToWorklistAsMayBeAPointer(Reg1);
    break;
  }
  }
}

void X86GoStatepointFixup::processDerived(Register BaseReg, unsigned No, MachineInstr &MI) {
  assert(isPointerOrMayBeAPointerOrUnsafeOrStack(BaseReg) && "invariant");
  if (DebugFixupPointers)
    dbgs() << "Process derived: " << MI << "\n";

  if (MI.isCopy()) {
    assert(MI.getOperand(0).isReg() && "invariant");
    if (isGCPointerBase(BaseReg)) {
      // Unsafe is also a valid pointer when there is arithmetic operations.
      //
      // Here, we used some inferred facts. Since we did not employee a fixed-pointer
      // iteration. We may fail to identify some pointers here.
      // For examples, if we have the following derived pointers:
      //   A -> B -> C.
      // If we first process C and has no knowledge whether B is a pointer or not,
      // we have to conservatively assume that B may be a pointer. Later, we find that A
      // is a Pointer and then we can conclude B is a pointer. If B is marked as a Pointer,
      // then we can mark C as a pointer as well. B will be added to the worklist again
      // if its type is updated to Pointer.
      addToWorklistAsPointer(MI.getOperand(0).getReg());
    } else {
      addToWorklistAsMayBeAPointer(MI.getOperand(0).getReg());
    }
    return;
  } else if (MI.isPHI()) {
    assert(MI.getOperand(0).isReg() && "invariant");
    addToWorklistAsMayBeAPointer(MI.getOperand(0).getReg());
    return;
  }

  auto Opcode = MI.getOpcode();
  switch (Opcode) {
  default:
    break;
  case X86::CMOV64rm:
  case X86::CMOV64rr: {
    assert(MI.getOperand(0).isReg() && "invariant");
    // Same as PHI
    // The other operand may not be a pointer.
    // BaseReg may be zero.
    addToWorklistAsMayBeAPointer(MI.getOperand(0).getReg());
    break;
  }
  case X86::LEA64r: {
    assert(MI.getOperand(0).isReg() && "invariant");
    Register Reg = MI.getOperand(0).getReg();
    MachineOperand &MOScale = MI.getOperand(1 + X86::AddrScaleAmt);
    MachineOperand &MOIndex = MI.getOperand(1 + X86::AddrIndexReg);

    assert(MOIndex.isReg() && "invariant");

    Register IndexReg = MOIndex.getReg();

    if (isGCPointerBase(BaseReg)) {
      if (No == 1 + X86::AddrBaseReg) {
        if (IndexReg == MCRegister::NoRegister) {
          // MOIndex is noreg and thus only AddrBaseReg can be Ptr
          addToWorklistAsPointer(Reg);
        } else if (MOScale.isImm() && MOScale.getImm() != 1) {
          addToWorklistAsPointer(Reg);
        } else {
          addToWorklistAsMayBeAPointer(Reg);
        }
      } else if (No == 1 + X86::AddrIndexReg) {
        addToWorklistAsMayBeAPointer(Reg);
      } else {
        // Ignore
      }
    } else {
      addToWorklistAsMayBeAPointer(Reg);
    }
    break;
  }
  case X86::INC64r:
  case X86::ADD64ri8:
  case X86::ADD64ri32:
  case X86::DEC64r:
  case X86::SUB64ri8:
  case X86::SUB64ri32: {
    // We can assume a pointer with constant offset is also a pointer.
    // If the offset comes from the frontend, it is a valid offset.
    // LLVM should not have knowledge to generate an invalid offset.
    assert(No == 1 && "invariant");
    assert(MI.getOperand(0).isReg() && "invariant");
    if (isGCPointerBase(BaseReg)) // Unsafe pointer is also pointer
      addToWorklistAsPointer(MI.getOperand(0).getReg());
    else
      addToWorklistAsMayBeAPointer(MI.getOperand(0).getReg());
    break;
  }
  case X86::ADD64rr: {
    // Simply assume target is a pointer.
    // Both Reg could be pointers and can lead to some
    // conflict.
    assert(MI.getOperand(0).isReg() && "invariant");
    Register Reg = MI.getOperand(0).getReg();
    addToWorklistAsMayBeAPointer(Reg);
    break;
  }
  case X86::SUB64rr: {
    assert(MI.getOperand(0).isReg() && "invariant");
    Register Reg = MI.getOperand(0).getReg();
    if (No == 1) {
      // Be conservative
      addToWorklistAsMayBeAPointer(Reg);
    } else if (No == 2) {
      // if we sub some value with a pointer, the result is not a pointer.
    } else {
      llvm_unreachable("unreachable");
    }
    break;
  }
  case X86::ADD64rm:
  case X86::SUB64rm: {
    assert(MI.getOperand(0).isReg() && "invariant");
    Register Reg = MI.getOperand(0).getReg();
    addToWorklistAsMayBeAPointer(Reg);
    break;
  }
  }
}

void X86GoStatepointFixup::collectCopy() {
  for (MachineBasicBlock &MBB : *MF) {
    for (MachineInstr &MI : MBB) {
      if (MI.isCopy()) {
        addCopy(MI);
      }
    }
  }
}

void X86GoStatepointFixup::collectZeroAndNoneHeapPointerValues() {
  for (MachineBasicBlock &MBB : *MF) {
    for (MachineInstr &MI : MBB) {
      auto UpdateReg0Type = [&](unsigned Type) {
        assert(MI.getOperand(0).isReg() && "invariant");
        Register Reg = MI.getOperand(0).getReg();
        if (Register::isVirtualRegister(Reg)) {
          // updateType(Reg, Type);
          // update type to all copy
          updateTypeToAllCopy(Reg, Type);
        }
      };


      if (MI.isCopy()) {
        MachineOperand MO = MI.getOperand(1);
        if (!MO.isReg())
          continue;

        Register Reg = MO.getReg();

        if (Register::isPhysicalRegister(Reg)) {
          if (/*Reg == X86::R14 ||*/
              Reg == X86::RBP || Reg == X86::RSP) {
            UpdateReg0Type(StackPointer);
          }
        }

        continue;
      }

      switch (MI.getOpcode()) {
      case X86::MOV32r0:
        UpdateReg0Type(Zero);
        break;
      case TargetOpcode::SUBREG_TO_REG:
        assert(MI.getOperand(2).isReg() && "invariant");
        if (isZero(MI.getOperand(2).getReg())) {
          UpdateReg0Type(Zero);
        }
        break;
      case TargetOpcode::IMPLICIT_DEF:
        // Undef
        UpdateReg0Type(NotHeap);
        break;
      case X86::LEA64r: {
        const MachineOperand &MOBase = MI.getOperand(1 + X86::AddrBaseReg);
        if (MOBase.isReg()) {
          if (MOBase.getReg() == X86::RIP) {
            UpdateReg0Type(NotHeap);
          } else if (MOBase.getReg() == X86::RSP || MOBase.getReg() == X86::RBP) {
            UpdateReg0Type(StackPointer);
          }
        } else if (MOBase.isFI()) {
          UpdateReg0Type(StackPointer);
        }
        break;
      }
      default:
        break;
      }
    }
  }
}

void X86GoStatepointFixup::collectInitialPointers() {
  // TODO: does MBB visiting order matter?
  for (MachineBasicBlock &MBB : *MF) {
    for (MachineInstr &MI : MBB) {
      if (MI.getOpcode() == TargetOpcode::STATEPOINT) {
        StatepointOpers SO(&MI);

        if (DebugFixupPointers)
          dbgs() << "Process statepoint: " << MI << "\n";

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

          Register VirtReg = MO.getReg();
          assert(MRI->hasOneDef(VirtReg) && "TODO: can handle single def right now");
          addToWorklistAsPointer(VirtReg);
          TrackedPointers.insert(VirtReg);
        }

        uint64_t ID = MI.getOperand(SO.getIDPos()).getImm();
        assert(Statepoints.count(ID) && "invariant");
        if (!isCallGoAdHocABI(Statepoints[ID])) {
          FunctionType *FTy = Statepoints[ID]->getActualFunctionType();
          unsigned NumParams = FTy->getNumParams();
          MachineBasicBlock *CurMBB = MI.getParent();
          unsigned Begin = 0;
          if (TangoLLVM::isGoClosureCall(Statepoints[ID])) {
            Begin = 1;
          }
          auto ProcessArgReg = [&](Register ParamReg) {
            // search backward until we find the first copy to the ParamReg
            // We must have a ADJSTACK right?
            auto It = MI.getIterator();
            assert(It != CurMBB->begin() && "must have ADJSTACK??");
            do {
              It = std::prev(It);
              if (It->getOpcode() == FrameSetupOpcode) {
                break;
              }
              if (!It->isCopy())
                continue;

              if (!It->getOperand(0).isReg())
                continue;

              if (ParamReg != It->getOperand(0).getReg())
                continue;

              if (!It->getOperand(1).isReg())
                continue;

              Register SrcReg = It->getOperand(1).getReg();
              addToWorklistAsPointer(SrcReg);
              break;
            } while (It != CurMBB->begin());
          };

          processFunctionArgReg(Begin, FTy, ProcessArgReg);

          auto ProcessRetReg = [&](Register RetReg) {
            auto It = std::next(MI.getIterator());
            for (;It != CurMBB->end(); It = std::next(It)) {
              if (It->getOpcode() == FrameDestroyOpcode) {
                break;
              }
              if (!It->isCopy())
                continue;

              if (!It->getOperand(1).isReg())
                continue;

              if (RetReg != It->getOperand(1).getReg())
                continue;

              if (!It->getOperand(0).isReg())
                continue;

              Register DstReg = It->getOperand(0).getReg();
              addToWorklistAsPointer(DstReg);
              break;
            }
          };

          Type *RetType = FTy->getReturnType();
          processFunctionReturnReg(RetType, ProcessRetReg);
        }
      } else if (MI.mayLoadOrStore()) {
        processMemoryMI(MI);
      } else if (MI.getOpcode() == X86::LEA64r) {
        int64_t Offset;

        // TODO: for other stack address with non-constant offset,
        // we may not be able to identify it here.
        if (!isConstantStackAddress(MI, Offset)) {
          continue;
        }

        // TODO: isConstantStackAddress should compute the same result as isLoadStackSlotAddress.
        int FrameIndex;
        assert(TII->isLoadStackSlotAddress(MI, FrameIndex) && "invariant");

        assert(MI.getOperand(0).isReg() && "invariant");
        Register Reg = MI.getOperand(0).getReg();
        assert(isStackPointer(Reg) && "invariant");
        // All ADD, SUB and LEA
        for (auto &U : MRI->use_operands(Reg)) {
          processDerived(Reg, U.getOperandNo(), *U.getParent());
        }
      }
    }
  }

  // Init:
  // Use parameter types of this function
  auto &Entry = MF->front();

  for (MachineInstr &MI : Entry) {
    if (!MI.isCopy())
      break;

    Register DstReg;
    Register SrcReg;
    if (MI.getOperand(0).isReg() && Register::isVirtualRegister(MI.getOperand(0).getReg()))
      DstReg = MI.getOperand(0).getReg();
    if (MI.getOperand(1).isReg() && !Register::isVirtualRegister(MI.getOperand(1).getReg()))
      SrcReg = MI.getOperand(1).getReg();

    if (DstReg == 0 || SrcReg == 0)
      break;

    Type *ArgTy = getFunctionArgumentType(MF->getFunction(), SrcReg);
    if (ArgTy == nullptr)
      break;

    if (ArgTy->getScalarType()->isPointerTy()) {
      // A compile time pointer must be a runtime pointer.
      if (DebugFixupPointers)
        dbgs() << "Process pointer type: " << MI << "\n";
      addToWorklistAsPointer(DstReg);
    } else {
      // A compile time non-pointer may also be a runtime pointer.
      if (DebugFixupPointers)
        dbgs() << "Process non-pointer type: " << MI << "\n";
      updateType(DstReg, NotAPointer);
    }
  }

  Type *RetType = MF->getFunction().getReturnType();
  if (TangoLLVM::hasPointerTy(RetType)) {
    auto ProcessRetReg = [&](Register RetReg, MachineBasicBlock &MBB) {
      for (auto It = MBB.rbegin(); It != MBB.rend(); ++It) {
        if (!It->isCopy())
          continue;

        if (!It->getOperand(0).isReg())
          continue;

        if (RetReg != It->getOperand(0).getReg())
          continue;

        if (!It->getOperand(1).isReg())
          continue;

        Register SrcReg = It->getOperand(1).getReg();
        if (DebugFixupPointers)
          dbgs() << "Process pointer return type: " << *It << "\n";

        addToWorklistAsPointer(SrcReg);
        break;
      }
    };

    for (MachineBasicBlock &MBB : *MF) {
      auto T = MBB.getFirstTerminator();
      if (T == MBB.end() || T->getOpcode() != X86::RET)
        continue;

      auto ProcessRetInMBB = [&](Register Reg) {
        ProcessRetReg(Reg, MBB);
      };

      processFunctionReturnReg(RetType, ProcessRetInMBB);
    }
  }
}

void X86GoStatepointFixup::collectPointers() {
  if (DebugFixupPointers)
    dbgs() << "Start to collect pointers for " << MF->getName() << "\n";

  collectCopy();
  collectZeroAndNoneHeapPointerValues();
  collectInitialPointers();

  while (!Worklist.empty()) {
    // Solve:
    while (!Worklist.empty()) {
      Register Reg = removeFromWorklist();
      // Lets see if we can get the base pointer of this pointer;
      assert(Register::isVirtualRegister(Reg) && "invariant");

      // TODO: we assume a VirtReg can only be defined once in this pass.
      // Therefore, it can only have one base.
      MachineInstr &MI = *MRI->def_begin(Reg)->getParent();

      if (isPointer(Reg)) {
        // TODO: what about Unsafe pointers?
        processBase(Reg, MI);

        for (auto &U : MRI->use_operands(Reg)) {
          processDerived(Reg, U.getOperandNo(), *U.getParent());
        }
      } else {
        if (DebugFixupPointers)
          dbgs() << "Ignore Reg: " << Register::virtReg2Index(Reg) << " -> " << TypeNames[getRegType(Reg)] << "\n";
        // When Reg is added to the worklist, it must be a pointer or may be a pointer.
        // However, the pointer may be zero or has been updated to non-pointer.
        // We are not sure whether a Zero is a valid pointer or just a zero index.
      }
    }

    for (auto &It : RegType) {
      Register Reg = It.first;
      unsigned Type = It.second;
      if (Type != MayBeAPointer)
        continue;

      assert(Register::isVirtualRegister(Reg) && "must be virtual register");

      MachineInstr &MI = *MRI->def_begin(Reg)->getParent();

      for (auto &It2 : RegType) {
        Register Reg2 = It2.first;
        unsigned Type2 = It2.second;
        if (Type2 != Pointer && Type2 != Unsafe)
          continue;

        if (checkRegSameValue(MI, Reg2)) {
          // Unsafe is also a pointer.
          if (updateType(Reg, Pointer)) {
            addToWorklist(Reg);
          }
        }
      }
    }
  }
}

// We can remove all unused pointers before running fixup.
// If a pointer is unused, we have no need to fix it.
bool X86GoStatepointFixup::checkUnusedPointers() {
  bool Changed = false;
  for (MachineBasicBlock &MBB : *MF) {
    for (MachineInstr &MI : llvm::make_early_inc_range(MBB)) {
      int64_t Offset = 0;
      Register Base = ::isGEPWithConstantOffset(MI, Offset);
      if (Base == 0 || Base.isPhysical() || !isGCPointerBase(Base))
        continue; // We cannot safely add Physical registers to statepoints.

      // TODO: avoid recursively removing unused pointers.
      // It may happen in the future but this should never happen at present.
      if (isGEPWithConstantOffset(Base))
        continue;

      Register Reg = MI.getOperand(0).getReg();
      if (!isPointerOrUnsafe(Reg))
        continue;

      if (removeUnusedPointer(Reg, Base)) {
        if (isSafeToDeleteGEP(MRI, MI)) {
          if (DebugFixupPointers)
            dbgs() << "Removing dead MI: " << MI << "\n";
          MI.eraseFromParent();
          Changed = true;
        }
      }
    }
  }

  return Changed;
}

bool X86GoStatepointFixup::removeUnusedPointer(Register Reg, Register Base) {
  bool HasRealUse = false;
  SmallSetVector<MachineInstr*, 8> Statepoints;
  for (auto &U : MRI->use_operands(Reg)) {
    MachineInstr *MI = U.getParent();
    if (!isStatepointDeoptUse(MI, U.getOperandNo())) {
      HasRealUse = true;
      break;
    }
    Statepoints.insert(MI);
  }

  if (HasRealUse)
    return false;

  // No real use and no fake use.
  if (Statepoints.empty())
    return true;

  assert(isGCPointerBase(Base) && "Base must be a pointer");
  assert(!isGEPWithConstantOffset(Base) && "TODO: we need to recursively fix this pointer");

  // NOTE: we can only remove constant-offset GEP. A removable constant-offset GEP
  // cannot be used as the base of another removable GEP.
  for (MachineInstr *MI : Statepoints) {
    StatepointOpers SO(MI);
    bool FindBase = false;
    int64_t Removed = 0;
    for (unsigned Idx = SO.getNumDeoptArgsIdx(), E = SO.getNumGCPtrIdx(); Idx < E; ++Idx) {
      const MachineOperand &MO = MI->getOperand(Idx);
      if (!MO.isReg())
        continue;
      if (MO.isDef())
        continue;
      if (!Register::isVirtualRegister(MO.getReg())) {
        continue;
      }

      if (MO.getReg() == Reg) {
        MI->removeOperand(Idx);
        ++Removed;
        assert(Idx > 0 && "should not be negative");
        --Idx;
        continue;
      }

      if (isClonedPtr(MO.getReg(), Base)) {
        FindBase = true;
        continue;
      }
    }

    assert(Removed > 0 && "");

    if (Removed > 0) {
      int64_t NumDeoptArgs = SO.getNumDeoptArgs();
      assert(NumDeoptArgs >= Removed && "invariant");
      MI->getOperand(SO.getNumDeoptArgsIdx()).setImm(NumDeoptArgs - Removed);
    }

    if (!FindBase) {
      AddOperandToStatePoint(*MI, {Base});
      // We add base to the statepoint. We need further check
      // to ensure all pointers are added.
      CheckingMissingStatepoints.insert(Base);
    }
  }

  return true;
}

//
Register X86GoStatepointFixup::clonePointerBeforeNonPHIUse(Register Reg, MachineInstr *DefMI, MachineBasicBlock *PosMBB, MachineInstr *Pos) {
  assert((Pos == nullptr || !Pos->isPHI()) && "must not be PHI");
  Register NewReg = MRI->createVirtualRegister(MRI->getRegClass(Reg));

  // DefMI should be GEP that has Opcode ADD/SUB
  auto hasImplicitlyDefinedEFLAGS = [&](MachineInstr *MI) {
    for (const MachineOperand &MO : MI->implicit_operands()) {
      assert(MO.isReg() && "invariant");
      if (MO.isDef() && MO.isImplicit()) {
        if (!(MO.isReg() && MO.getReg() == X86::EFLAGS)) {
          report_fatal_error("TODO: cannot handle implicitly defined values");
        }
        return true;
      }
    }
    return false;
  };

  // Pointer should only implicitly define EFLAS
  if (Pos != nullptr && hasImplicitlyDefinedEFLAGS(DefMI)) {
    // We clone a pointer and make an implicit def.
    // Lets find some places where we can safely place the cloned pointer
    // after a statepoint but before any instruction that have the same implicit def
    // in this basic block.
    auto I = Pos->getIterator();
    auto E = &PosMBB->front();

    auto MOIsEFLAGS = [&](const MachineOperand &MO) {
      return MO.isReg() && MO.getReg() == X86::EFLAGS;
    };

    do {
      if (llvm::any_of(I->all_defs(), MOIsEFLAGS)) {
        // If I defines EFLAGS, we can safely put cloned pointer before it.
        Pos = &*I;
        break;
      }

      if (&*I == E)
        break;
      I = std::prev(I);
      // FrameSetup and FrameDestroy implicitly defines EFLAGS
      if (I->isCall())
        break;
    } while (true);
  }

  auto InsertBefore = Pos == nullptr ? PosMBB->end() : Pos->getIterator();
  MachineInstr &NewMI = TII->duplicate(*PosMBB, InsertBefore, *DefMI);
  // LEAQ
  assert(NewMI.getOperand(0).isReg() && "invariant");
  assert(NewMI.getOperand(0).getReg() == Reg && "invariant");

  // update the cloned Reg to new reg
  replaceReg(NewMI.getOperand(0), NewReg);
  if (DebugFixupPointers) {
    dbgs() << "Clone a pointer " << Register::virtReg2Index(Reg) << " to " << Register::virtReg2Index(NewReg) << "@" << NewMI;
    if (Pos == nullptr) {
      dbgs() << " at the end of " << PosMBB->getName() << "\n";
    } else {
      dbgs() << " before " << *Pos << " in " << PosMBB->getName() << "\n";
    }
  }
  return NewReg;
}

void X86GoStatepointFixup::clonePointerBeforePHIUse(Register Reg, MachineInstr *DefMI, MachineInstr *UseMI) {
  assert(UseMI->isPHI() && "must be PHI");
  for (unsigned I = 1, E = UseMI->getNumOperands(); I != E; I += 2) {
    assert(UseMI->getOperand(I).isReg() && "invariant");
    Register InReg = UseMI->getOperand(I).getReg();
    if (Reg != InReg)
      continue;
    MachineBasicBlock *InMBB = UseMI->getOperand(I + 1).getMBB();
    MachineInstr* Pos = nullptr;
    auto Term = InMBB->getFirstTerminator();
    if (Term != InMBB->end()) {
      Pos = &*InMBB->getFirstTerminator();
    }
    Register NewReg = clonePointerBeforeNonPHIUse(Reg, DefMI, InMBB, Pos);
    replaceReg(UseMI->getOperand(I), NewReg);
  }
}

void X86GoStatepointFixup::collectUntrackedPointers(DenseMap<Register, SmallVector<MachineInstr*, 4>> &PointerToStatepoints,
    std::function<bool(Register)> NeedTrack) {
  // Now we can do a liveness analysis to check if all live-out pointers of a STATEPOINT
  // are tracked by the STATEPOINT.
  // NOTE: Only track virtual registers.
  DenseMap<MachineBasicBlock*, SmallSetVector<Register, 8>> LiveInValues;

  DenseMap<MachineInstr*, SmallSetVector<Register, 8>> StatepointLiveOutValues;

  std::deque<MachineBasicBlock*> Worklist;
  BitVector InList(MF->getNumBlockIDs());

  auto addToWorklist = [&](MachineBasicBlock *MBB) {
    if (InList.test(MBB->getNumber()))
      return;
    Worklist.push_back(MBB);
    InList.set(MBB->getNumber());
  };

  auto ProcessMBB = [&](MachineBasicBlock *MBB) {
    // Step 0: initialize the LiveOut of the last instruction.
    SmallSetVector<Register, 8> Live;
    for (MachineBasicBlock *Succ : MBB->successors()) {
      SmallSetVector<Register, 8> &SuccLiveIn = LiveInValues[Succ];
      Live.set_union(SuccLiveIn);
      // Process PHI
      for (auto &MI : *Succ) {
        if (!MI.isPHI())
          break;
        for (unsigned I = 1, E = MI.getNumOperands(); I != E; I += 2) {
          MachineBasicBlock *Pred = MI.getOperand(I+1).getMBB();
          if (Pred != MBB)
            continue;
          assert(MI.getOperand(I).isReg() && "invariant");
          Register Reg = MI.getOperand(I).getReg();
          if (!Register::isVirtualRegister(Reg) || !NeedTrack(Reg))
            continue;
          Live.insert(Reg);
        }
      }
    }
    // Collected generated pointers
    for (auto &MI : llvm::reverse(*MBB)) {
      if (MI.isPHI()) {
        MachineOperand &MO = MI.getOperand(0);
        assert(MO.isReg() && "invariant");
        if (Register::isVirtualRegister(MO.getReg()) && NeedTrack(MO.getReg())) {
          Live.remove(MO.getReg());
        }
        continue;
      }
      // Live is the LiveOut of MI.
      if (MI.getOpcode() == TargetOpcode::STATEPOINT) {
        StatepointLiveOutValues[&MI] = Live;
      }
      for (const MachineOperand &MO : MI.operands()) {
        if (MO.isReg() && Register::isVirtualRegister(MO.getReg())) {
          if (!NeedTrack(MO.getReg()))
            continue;

          // Def comes first in operands.
          if (MO.isDef()) {
            Live.remove(MO.getReg());
          } else {
            Live.insert(MO.getReg());
          }
        }
      }
    }

    // Update Worklist
    SmallSetVector<Register, 8> &LiveIn = LiveInValues[MBB];
    if (Live.size() != LiveIn.size()) {
      LiveIn = Live;
      for (MachineBasicBlock * Pred : MBB->predecessors()) {
        addToWorklist(Pred);
      }
    }
  };

  // prologue: collect initial data flow facts
  for (MachineBasicBlock *MBB : post_order(MF)) {
    ProcessMBB(MBB);
  }

  // solving: propagate until reaching the fixed point
  while (!Worklist.empty()) {
    MachineBasicBlock *MBB = Worklist.front();
    Worklist.pop_front();
    InList.reset(MBB->getNumber());
    ProcessMBB(MBB);
  }

  for (auto It : StatepointLiveOutValues) {
    MachineInstr *MI = It.first;
    SmallSetVector<Register, 8> &LiveOut = It.second;

    SmallSetVector<Register, 8> Tracked;
    StatepointOpers SO(MI);
    for (unsigned Idx = SO.getNumDeoptArgsIdx(), E = SO.getNumGCPtrIdx(); Idx < E;
        ++Idx) {
      const MachineOperand &MO = MI->getOperand(Idx);
      if (!MO.isReg())
        continue;
      if (MO.isDef())
        continue;
      if (!Register::isVirtualRegister(MO.getReg())) {
        continue;
      }

      Tracked.insert(MO.getReg());
    }

    LiveOut.set_subtract(Tracked);

    {
      // Eliminate any copy
      for (auto I = LiveOut.begin(); I != LiveOut.end();) {
        Register Reg = *I;
        if (llvm::any_of(Tracked, [&](Register Reg2) { return isClonedPtr(Reg, Reg2); })) {
          I = LiveOut.erase(I);
        } else {
          ++I;
        }
      }
    }

    // Custom NeedTrack may add MayBeAPointer. We have to remove all of them here to avoid errors.
    // TODO: we cannot prove a MayBeAPointer is a real pointer.
    // We only process Pointer and Unsafe because we assume that the GoStatepoints pass at LLVM IR
    // level can precisely capture all pointers.
    LiveOut.remove_if([&](Register Reg) { return getRegType(Reg) == MayBeAPointer; });

    if (DebugFixupPointers && !LiveOut.empty()) {
      uint64_t ID = MI->getOperand(SO.getIDPos()).getImm();
      dbgs() << "Dump untracked pointer for " << MF->getName() << " at STATEPOINT " << ID << ":";
      for (auto Reg: LiveOut) {
        auto Type = getRegType(Reg);
        dbgs() << " " << Register::virtReg2Index(Reg) << ":" << TypeNames[Type];
      }
      dbgs() << "\n";
    }

    // Build the map from missing pointers to statepoints
    for (auto Reg: LiveOut) {
      PointerToStatepoints[Reg].push_back(MI);
    }
  }
}

bool X86GoStatepointFixup::checkStatepoints(std::function<bool(Register)> NeedTrack) {
  // A mapping between missing pointers and statepoints.
  DenseMap<Register, SmallVector<MachineInstr*, 4>> PointerToStatepoints;
  collectUntrackedPointers(PointerToStatepoints, NeedTrack);

  if (PointerToStatepoints.empty())
    return false;

  bool Changed = false;

  // A helper function that does batch update to improve performance
  DenseMap<MachineInstr*, SmallVector<Register, 4>> PendingPointers;
  auto AddPtrToStatepoints = [&](Register Ptr, SmallVector<MachineInstr*, 4> &Statepoints) {
    for (MachineInstr *MI : Statepoints) {
      PendingPointers[MI].push_back(Ptr);
    }
  };

  // TODO: we need a worklist in case we need to move base first.
  // However, we currently only move and clone constant offset GEPs.
  // If the base of a constant-offset GEP is also a constant-offset GEP,
  // we can fold them into one GEP.
  for (auto I : PointerToStatepoints) {
    Register Ptr = I.first;
    SmallVector<MachineInstr*, 4> &Statepoints = I.second;

    Register Base = isGEPWithConstantOffset(Ptr);
    if (!Base) {
      // We assume only GEP with constant offset are unsafe to be added to statepoints.
      AddPtrToStatepoints(Ptr, Statepoints);
      continue;
    }

    if (isGEPWithConstantOffset(Base)) {
      report_fatal_error("TODO: we need to fold constant GEP first");
    }

    MachineInstr &DefMI = *MRI->def_begin(Ptr)->getParent();

    if (Base.isPhysical() || !isPointerOrUnsafe(Base)) {
      if (DebugFixupPointers) {
        dbgs() << "GEP with NonePointer Base Type: " << TypeNames[getRegType(Base)] <<
          " " << DefMI  << "\n";
      }
      // If the Base is not a pointer, we cannot add Base to the statepoint. We then cannot
      // clone GEP. We have to assume the GEP is safe.
      AddPtrToStatepoints(Ptr, Statepoints);
      continue;
    }

    assert(Register::isVirtualRegister(Base) && "cloning physical registers is difficult");
    assert(!(isGEPWithConstantOffset(Base) && PointerToStatepoints.count(Base)) && "TODO: we could either fold GEP or process base first");
    // In theory Base should not be Zero but in practice it could be.
    // If the base is a stack object, we need to fold the constant GEP into
    assert(isGCPointerBase(Base) && "TODO: invariant for constant off set GEP");

    auto CheckAndDeleteDefMI = [&](MachineInstr &MI) {
      if (isSafeToDeleteGEP(MRI, MI)) {
        if (DebugFixupPointers) {
          dbgs() << "SafeToDelete: " << MI << "\n";
        }
        MI.eraseFromParent();
        Changed = true;
      } else {
        if (DebugFixupPointers) {
          dbgs() << "UnsafeToDelete: " << MI << "\n";
        }
      }
    };

    // If the pointer can be removed, we have no need to add it to the
    // missing statepoint.
    if (removeUnusedPointer(Ptr, Base)) {
      CheckAndDeleteDefMI(DefMI);
      continue;
    }

    // We are about to cloning the pointer.
    // Mark changed to true.
    Changed = true;
    // Clone to every use
    {
      SmallSetVector<MachineInstr*, 8> Uses;

      // We use dominator tree to avoid duplicate cloning of pointers.
      // If UseA dominates UseB, we can share the cloned pointer of
      // UseA to UseB.
      // If UseA dominates UseB and UseC dominates UseB, then either UseA dominates
      // UseC or UseC dominates UseA. This is because DominatorTree is a tree.
      EquivalenceClasses<MachineInstr*> GroupedUses;

      // The size of uses of MIR is not significant.
      for (auto &MI : MRI->use_instructions(Ptr)) {
        if (&MI == &DefMI)
          continue;

        if (!Uses.insert(&MI))
          continue;

        // insert everything into union-find set
        GroupedUses.insert(&MI);
        // Uses is not big in practice
        for (MachineInstr *Added : Uses) {
          if (MDT->dominates(Added, &MI)) {
            GroupedUses.unionSets(Added, &MI);
          } else if (MDT->dominates(&MI, Added)) {
            GroupedUses.unionSets(&MI, Added);
          }
        }
      }

      bool SawStore = false;
      if (!DefMI.isSafeToMove(nullptr, SawStore)) {
        report_fatal_error("TODO: must be safe to move constant GEP");
      }

      // We have to compute a real leader here.
      // TODO: modify EquivalenceClasses.h to support custom comparator
      // using MDT.
      DenseMap<const MachineInstr*, Register> LeaderToNewReg;
      for (auto I = GroupedUses.begin(), E = GroupedUses.end(); I != E; ++I) {
        if (!I->isLeader())
          continue;

        const MachineInstr *DomUse = nullptr;
        for (auto MI = GroupedUses.member_begin(I); MI != GroupedUses.member_end(); ++MI) {
          const MachineInstr *M = *MI;
          if (DomUse == nullptr || MDT->dominates(M, DomUse)) {
            DomUse = M;
          }
        }
        assert(DomUse != nullptr && "invariant");

#if !defined(NDEBUG) || defined(LLVM_ENABLE_DUMP)
        // Double check to ensure that DomUse dominates all in the group.
        for (auto MI = GroupedUses.member_begin(I); MI != GroupedUses.member_end(); ++MI) {
          const MachineInstr *M = *MI;
          assert(MDT->dominates(DomUse, M) && "invariant");
        }
#endif

        if (DomUse->isPHI())
          continue;

        // If DomUse is not a PHI, we can pre-compute a NewReg for sharing.
        MachineInstr *Pos = const_cast<MachineInstr*>(DomUse);
        Register NewReg = clonePointerBeforeNonPHIUse(Ptr, &DefMI, Pos->getParent(), Pos);
        LeaderToNewReg[I->getData()] = NewReg;
      }

      // Do the actual cloning
      for (MachineInstr *MI : Uses) {
        const MachineInstr *Leader = GroupedUses.getLeaderValue(MI);
        auto I = LeaderToNewReg.find(Leader);
        Register NewReg = 0;
        if (I == LeaderToNewReg.end()) {
          // The real leader is a PHI
          assert(Leader->isPHI() && "invariant");
          if (MI->isPHI()) {
            clonePointerBeforePHIUse(Ptr, &DefMI, MI);
            continue; // we are done because the clone will update the reg in PHI
          } else {
            NewReg = clonePointerBeforeNonPHIUse(Ptr, &DefMI, MI->getParent(), MI);
          }
        } else {
          NewReg = I->second;
        }

        // Update all operands
        for (auto &MO : MI->operands()) {
          if (!MO.isReg() || MO.isDef())
            continue;
          if (MO.getReg() == Ptr)
            replaceReg(MO, NewReg);
        }
      }

      // also check whether Base needs to be added to missing
      CheckingMissingStatepoints.insert(Base);
    }

    // Now we can try to delete DefMI
    CheckAndDeleteDefMI(DefMI);
  }

  if (!PendingPointers.empty()) {
    Changed = true;
    for (auto I : PendingPointers) {
      MachineInstr *ST = I.first;
      SmallVector<Register, 4> &Pointers = I.second;
      AddOperandToStatePoint(*ST, Pointers);
    }
  }

  return Changed;
}
