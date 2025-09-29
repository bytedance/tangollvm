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
#include "llvm/ADT/Hashing.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/InstructionSimplify.h"
#include "llvm/Analysis/ProfileSummaryInfo.h"
#include "llvm/Analysis/ValueTracking.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Alignment.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/TangoLLVM/TangoLLVM.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/Local.h"
using namespace llvm;


#define DEBUG_TYPE "gorodata"

STATISTIC(NumSpecializedFunc, "Number of specialized functions");
STATISTIC(NumSpecializedCall, "Number of call instructions");

// Enable Go RO data optimization
static cl::opt<bool> EnableGoROData("go-rodata", cl::Hidden,
                                  cl::init(true));

static cl::opt<bool> EnableGoSpecialization("go-specialization", cl::Hidden,
                                  cl::init(true));

static cl::opt<bool> AggressiveSpecialization("go-aggressive-spec", cl::Hidden,
                                  cl::init(false));

static cl::opt<uint32_t> GoMaxSpecInlineSize("go-max-spec-inline-size", cl::Hidden,
                                  cl::init(2500));

static bool intrinsifyGoBuiltins(Function &F) {
  SmallVector<CallInst*, 4> MemClrs;
  SmallVector<CallInst*, 4> MemHashs;
  SmallVector<CallInst*, 4> IntHashs;
  SmallVector<CallInst*, 4> StrHashs;
  for (Instruction &I : instructions(F)) {
    CallInst *CI = dyn_cast<CallInst>(&I);
    if (!CI)
      continue;

    Function *Callee = CI->getCalledFunction();
    if (!Callee) {
      Callee = dyn_cast<Function>(CI->getCalledOperand()->stripPointerCasts());
      if (!Callee) {
        continue;
      }
    }

    StringRef Name = Callee->getName();
    if (Name == "runtime.memclrNoHeapPointers") {
      assert(CI->arg_size() == 2 && "invariant");
      if (auto * C = dyn_cast<ConstantInt>(CI->getArgOperand(1))) {
        if (C->getZExtValue() < 128) { // TODO: hard coded threshold here
          MemClrs.push_back(CI);
        }
      }
    } else if (Name == "runtime.memhash64" || Name == "runtime.memhash32") {
      IntHashs.push_back(CI);
    } else if (Name == "runtime.memhash") {
      MemHashs.push_back(CI);
    } else if (Name == "runtime.strhash") {
      StrHashs.push_back(CI);
    }
  }

  bool Changed = false;

  for (auto *CI : MemClrs) {
    if (TangoLLVM::intrinsifyMemClr(CI))
      Changed = true;
  }

  for (auto *CI : IntHashs) {
    if (TangoLLVM::intrinsifyMemHash32Or64(CI))
      Changed = true;
  }

  bool NewMemHashs = false;
  if (!StrHashs.empty()) {
    bool LocalChanged = false;
    for (auto *CI : StrHashs) {
      if (TangoLLVM::intrinsifyStrHash(CI)) {
        LocalChanged = true;
      }
    }
    if (LocalChanged) {
      Changed = true;
      NewMemHashs = true;
      for (BasicBlock &BB : F)
        SimplifyInstructionsInBlock(&BB);
    }
  }

  if (NewMemHashs) {
    // StrHash introduce new MemHash
    // Collect them again
    MemHashs.clear();
    for (Instruction &I : instructions(F)) {
      CallInst *CI = dyn_cast<CallInst>(&I);
      if (!CI)
        continue;

      Function *Callee = CI->getCalledFunction();
      if (!Callee) {
        continue;
      }

      StringRef Name = Callee->getName();
      if (Name == "runtime.memhash") {
        MemHashs.push_back(CI);
      }
    }
  }

  for (auto *CI : MemHashs) {
    if (TangoLLVM::intrinsifyMemHash(CI))
      Changed = true;
  }

  return Changed;
}

static bool isSupportedArg(const Value *V) {
  if (isa<ConstantInt>(V) || isa<ConstantFP>(V) || isa<ConstantPointerNull>(V))
    return true;
  if (auto *F = dyn_cast<Function>(V))
    return !F->isDeclaration();
  if (auto *G = dyn_cast<GlobalVariable>(V))
    return G->hasMetadata("go.rodata");

  return false;
}

const uint64_t AggressiveFlag = 1 << 31;

CallInst *GoRODataPass::specializeCall(ProfileSummaryInfo& PSI, Function &F, CallInst *CI, bool Aggressive) {
  Function* Callee = CI->getCalledFunction();
  if (Callee->hasMetadata("go.specialized"))
    return nullptr;

  auto MD = Callee->getMetadata("go.spec.cond");
  assert(MD != nullptr && "invariant");

  uint64_t ArgPos = dyn_cast<ConstantAsMetadata>(MD->getOperand(0))
    ->getValue()->getUniqueInteger().getZExtValue();

  // "CalleeName,,,{ArgNo,ArgVal},,,specialized"
  std::string SpecName;
  SpecName += Callee->getName();
  SpecName += ",,,";

  auto TakeArg = [&](Value *Arg, unsigned Pos) {
    if (Aggressive || ((ArgPos & AggressiveFlag) != 0))
      return isSupportedArg(Arg);
    if (ArgPos == 0)
      return Pos == 0;
    return (ArgPos & (1 << Pos)) != 0;
  };

  // Copy necessary attributes
  for (unsigned I = 0, E = Callee->arg_size(); I < E; I++) {
    Value *Arg = CI->getArgOperand(I);
    if (!TakeArg(Arg, I)) {
      continue;
    }
    assert(isSupportedArg(Arg) && "invariant");
    SpecName += std::to_string(I);
    SpecName += ",";

    if (ConstantInt *C = dyn_cast<ConstantInt>(Arg)) {
      SmallString<40> S;
      C->getValue().toString(S, 10, false);
      SpecName += S;
    } else if (ConstantFP *C = dyn_cast<ConstantFP>(Arg)) {
      SmallString<40> S;
      C->getValue().toString(S);
      SpecName += S;
    } else if (isa<ConstantPointerNull>(Arg)) {
      SpecName += "null";
    } else {
      SpecName += Arg->getName();
    }
    SpecName += ",";
  }
  // Mark this function is a specialized version in the object file
  SpecName += ",,specialized";

  Module *M = F.getParent();
  Function * NewCallee = M->getFunction(SpecName);
  if (NewCallee == nullptr) {
    // We must create a function with the same signature
    // even we know some arguments must be constants.
    // This is because some Go ABI related metadata are created
    // based on the signature. We do not want to compute the ABI
    // in LLVM before we have implemented full Go ABI in LLVM.
    NewCallee = Function::Create(Callee->getFunctionType(),
        Callee->getLinkage(), SpecName, *M);

    ValueToValueMapTy VMap;
    for (unsigned I = 0, E = Callee->arg_size(); I < E; I++) {
      Value *Arg = CI->getArgOperand(I);
      if (!TakeArg(Arg, I)) {
        VMap[Callee->getArg(I)] = NewCallee->getArg(I);
        continue;
      }
      assert(isa<Constant>(Arg) && "invariant: only constant can be shared");
      VMap[Callee->getArg(I)] = Arg;
    }
    SmallVector<ReturnInst *, 8> Returns;
    CloneFunctionInto(NewCallee, Callee, VMap, CloneFunctionChangeType::LocalChangesOnly,
        Returns);

    NewCallee->setMetadata("go.specialized", MDNode::get(Callee->getContext(), {}));

    runOnFunction(PSI, *NewCallee, true, true);

    ++NumSpecializedFunc;
  }

  if (Callee == NewCallee)
    return nullptr;

  SmallVector<Value*, 8> Args(CI->arg_begin(), CI->arg_end());
  CallInst *NewCI = CallInst::Create(NewCallee, Args);
  NewCI->setCallingConv(CI->getCallingConv());
  NewCI->copyMetadata(*CI);
  NewCI->setAttributes(CI->getAttributes());
  return NewCI;
}

bool GoRODataPass::specializeGoBuiltins(ProfileSummaryInfo& PSI, Function &F, bool UseInline) {
  if (!EnableGoSpecialization)
    return false;

  SmallVector<CallInst*, 16> Targets;

  // If specialization is too aggressive, we
  // will result in excessively large binaries.
  for (Instruction &I : instructions(F)) {
    CallInst *CI = dyn_cast<CallInst>(&I);
    if (!CI)
      continue;

    if (CI->arg_empty())
      continue;

    Function *Callee = CI->getCalledFunction();
    if (!Callee)
      continue;

    if (Callee->isDeclaration() || Callee->isIntrinsic())
      continue;

    if (Callee->hasMetadata("go.specialized"))
      continue;

    // if (Callee->hasFnAttribute(Attribute::NoInline))
    //   continue;
    auto MD = Callee->getMetadata("go.spec.cond");
    if (MD == nullptr) {
      continue;
    }

    uint64_t ArgPos = dyn_cast<ConstantAsMetadata>(MD->getOperand(0))
      ->getValue()->getUniqueInteger().getZExtValue();

    if (ArgPos != 0) {
      bool Passed = true;
      const unsigned E = CI->arg_size();
      assert(E <= 64 && "no more args");
      for (unsigned I = 0; I < E; I++) {
        if ((ArgPos & (1 << I)) == 0)
          continue;
        if (!isSupportedArg(CI->getArgOperand(I))) {
          Passed = false;
          break;
        }
      }
      if (!Passed)
        continue;
    } else {
      if (!isSupportedArg(CI->getArgOperand(ArgPos)))
        continue;
    }

    Targets.push_back(CI);
  }

  if (Targets.empty())
    return false;

  bool Changed = false;
  for (CallInst *CI : Targets) {
    LLVM_DEBUG(dbgs() << "GoROData: " << F.getName() << ": try to specialize a call " << *CI
        << ", UseInline=" << UseInline << ", InsnCount=" << F.getInstructionCount()
        << "\n");

    Function *Callee = CI->getCalledFunction();
    bool DoInline = UseInline && !Callee->hasFnAttribute(Attribute::NoInline)
      && F.getInstructionCount() < GoMaxSpecInlineSize;
    if (DoInline) {
      InlineFunctionInfo FnInfo;
      auto InlineRes = InlineFunction(*CI, FnInfo);
      if (InlineRes.isSuccess()) {
        Changed = true;
        LLVM_DEBUG(dbgs() << "GoROData: " << F.getName() << ": inline a specialize function " << Callee->getName() << "\n");
      }
    } else {
      CallInst *NewCI = specializeCall(PSI, F, CI, AggressiveSpecialization);
      if (NewCI != nullptr) {
        Changed = true;
        NewCI->insertBefore(CI);
        CI->replaceAllUsesWith(NewCI);
        CI->eraseFromParent();
        ++NumSpecializedCall;
        LLVM_DEBUG(dbgs() << "GoROData: " << F.getName() << ": create a specialize call " << *NewCI << "\n");
      }
    }
  }

  return Changed;
}

bool GoRODataPass::replaceLoadROData(Function &F) {
  bool Changed = false;

  const DataLayout &DL = F.getParent()->getDataLayout();

  SmallSetVector<LoadInst*, 32> Worklist;
  for (Instruction &I : instructions(F)) {
    LoadInst *LI = dyn_cast<LoadInst>(&I);
    if (LI)
      Worklist.insert(LI);
  }

  while (!Worklist.empty()) {
    LoadInst *LI = Worklist.pop_back_val();

    Constant *NewValue = resolveLoadROData(LI, DL);
    if (NewValue != nullptr) {
      LLVM_DEBUG(dbgs() << "GoROData: " << F.getName() << ": replace" << *LI
          << " by constant value ";
          if (NewValue->hasName()) { dbgs() << NewValue->getName(); } else { dbgs() <<  *NewValue; }
          dbgs() << "\n");
      for (auto &U : LI->uses()) {
        if (LoadInst *NewLI = dyn_cast<LoadInst>(U.getUser())) {
          Worklist.insert(NewLI);
          continue;
        }
      }

      LI->replaceAllUsesWith(NewValue);
      LI->eraseFromParent();
      Changed = true;
    }
  }

  if (Changed) {
    for (BasicBlock &BB : F)
      SimplifyInstructionsInBlock(&BB);
  }

  return Changed;
}

bool GoRODataPass::runOnFunction(ProfileSummaryInfo& PSI, Function &F, bool Specialize, bool UseInline) {
  bool Changed = false;
  while (true) {
    bool IterChanged = replaceLoadROData(F);
    IterChanged |= intrinsifyGoBuiltins(F);
    auto Hot = F.hasProfileData() ? PSI.isFunctionEntryHot(&F) : true;
    if (Specialize && Hot && specializeGoBuiltins(PSI, F, UseInline)) {
      IterChanged = true;
    }
    if (!IterChanged) {
      break;
    }
    Changed = true;
  }
  return Changed;
}

PreservedAnalyses GoRODataPass::run(Module &M, ModuleAnalysisManager &MAM) {
  if (!EnableGoROData)
    return PreservedAnalyses::all();

  auto &PSI = MAM.getResult<ProfileSummaryAnalysis>(M);
  bool Changed = false;

  for (Function &F : M) {
    if (F.isDeclaration())
      continue;

    if (F.isIntrinsic())
      continue;

    Changed |= runOnFunction(PSI, F,  Options.Specialize, false);
  }

  if (!Changed)
    return PreservedAnalyses::all();

  return PreservedAnalyses::none() ;
}

struct PtrAndOff {
  GlobalVariable *GV;
  uint64_t Offset;
};

static PtrAndOff findBasePtrAndOff(Value *V, const DataLayout &DL);

static PtrAndOff findBasePtrAndOffGEP(GEPOperator *GEP, const DataLayout &DL) {
    unsigned BitWidth = DL.getIndexTypeSizeInBits(GEP->getType());
    APInt GEPOffset(BitWidth, 0);
    if (!GEP->accumulateConstantOffset(DL, GEPOffset)) {
      return {};
    }
    auto PO = findBasePtrAndOff(GEP->getPointerOperand(), DL);
    return {PO.GV, PO.Offset + GEPOffset.getZExtValue()};
}

static PtrAndOff findBasePtrAndOff(Value *V, const DataLayout &DL) {
  if (auto *GEP = dyn_cast<GetElementPtrInst>(V)) {
    return findBasePtrAndOffGEP(cast<GEPOperator>(GEP), DL);
  }

  if (auto *CE = dyn_cast<ConstantExpr>(V)) {
    if (CE->getOpcode() == Instruction::GetElementPtr) {
      auto *GEP = cast<GEPOperator>(CE);
      return findBasePtrAndOffGEP(GEP, DL);
    }
    return {};
  }

  if (auto *BC = dyn_cast<BitCastInst>(V)) {
    return findBasePtrAndOff(BC->getOperand(0), DL);
  }

  if (auto *GV = dyn_cast<GlobalVariable>(V)) {
    return {GV, 0};
  }

  return {};
}

static GoROData buildRODataRelocation(MDNode *ROData) {
  GoROData data;
  auto *MDRel = cast<MDNode>(ROData->getOperand(1));
  for (unsigned i = 0; i + 4 < MDRel->getNumOperands(); i += 5) {
    auto getI64 = [&](unsigned idx)->int64_t {
      auto *C = cast<ConstantAsMetadata>(MDRel->getOperand(i+idx));
      return cast<ConstantInt>(C->getValue())->getZExtValue();
    };

    int64_t enabled = getI64(0);
    uint64_t off    = (uint64_t)getI64(1);
    uint64_t siz    = (uint64_t)getI64(2);
    int64_t addend  = getI64(3);

    StringRef sym;
    if (auto *S = dyn_cast<MDString>(MDRel->getOperand(i+4)))
      sym = S->getString();

    data.relocs.push_back({off, siz, addend, enabled, sym.str()});
  }

  return data;
}

Constant *GoRODataPass::resolveLoadROData(LoadInst *LI, const DataLayout &DL) {
  auto PO = findBasePtrAndOff(LI->getPointerOperand(), DL);
  GlobalVariable *GV = PO.GV;
  if (GV == nullptr) {
    return nullptr;
  }

  uint64_t Offset = PO.Offset;
  auto *ROData = GV->getMetadata("go.rodata");
  if (!ROData) {
    return nullptr;
  }
  const unsigned PtrSizeInBits = DL.getPointerSizeInBits();
  assert(PtrSizeInBits == 64 && "64bit only");
  Type *ValType = LI->getType();
  // TODO: ValSize could be zero in LLVM but non-zero in Go, e.g., empty struct
  auto ValSize = DL.getTypeStoreSize(ValType);

  StringRef Data = cast<MDString>(ROData->getOperand(0))->getString();
  // TODO: load from all zero data is zero, even the offset is out of bounds
  if (Offset >= Data.size() || (Offset + ValSize) > Data.size()) {
    if (GV->hasMetadata("go.zerorodata")) {
      return Constant::getNullValue(ValType);
    }
    return nullptr;
  }

  Module *M = LI->getParent()->getParent()->getParent();

  if (ROData->getNumOperands() == 2) {
    // load cache
    GoROData data;
    auto It = GVMap.find(GV);
    if (It == GVMap.end()) {
      data = buildRODataRelocation(ROData);
      GVMap[GV] = data;
    } else {
      data = It->second;
    }

    for (auto Reloc: data.relocs) {
    // Function *F = LI->getParent()->getParent();
    // dbgs() << F->getName() << " GlobalVariable: "
    //   << *LI << ", GV="
    //   << *GV << ", Reloc=" << *Reloc << "\n";
      uint64_t Off = Reloc.off;
      uint64_t Siz = Reloc.size;

      // We must stop constant replacement if the Offset+ValSize has overlap with the Reloc
      if (Off < Offset) {
        if (Off + Siz > Offset)
          return nullptr; // overlap
        continue;
      }

      if (Off > Offset) {
        if (Offset + ValSize > Off)
          return nullptr; // overlap
        break;
      }

      if (Reloc.enabled == 0) {
        return nullptr;
      }

      // Now, we can only return a pointer or pointer-like integer
      if (!(ValType->isPointerTy() || ValType->isIntegerTy(PtrSizeInBits)))
        return nullptr;

      // Reloc.Siz must be PtrSize
      if (Siz != PtrSizeInBits/8)
        return nullptr;

      Value *Ptr = nullptr;
      StringRef PtrName = Reloc.sym;
      if (Reloc.sym.empty()) {
        return nullptr;
      }

      Ptr = M->getGlobalVariable(PtrName);
      if (Ptr == nullptr)
        Ptr = M->getFunction(PtrName);
      if (Ptr == nullptr)
        return nullptr; // other constants are not supported yet

      assert(Ptr->getType()->isPointerTy() && "must be a global");

      uint64_t Add = Reloc.addend;

      Constant *NewValue = nullptr;
      if (Add != 0) {
        GlobalVariable *PtrGV = dyn_cast<GlobalVariable>(Ptr);
        if (!PtrGV)
          return nullptr;

        if (!PtrGV->getValueType()->isIntegerTy(8)) {
          return nullptr;
        }
        // Add is the offset in bytes. We can only make a GEP here
        // if the GV is i8*.
        NewValue = ConstantExpr::getGetElementPtr(PtrGV->getValueType(), PtrGV,
            ConstantInt::get(DL.getIndexType(PtrGV->getType()), Add));
      } else {
        NewValue = dyn_cast<Constant>(Ptr);
      }

      Type *Expected = LI->getType();

      if (Expected != NewValue->getType()) {
        // TODO: type cast would be messy.
        Type *Got = NewValue->getType();
        if (Expected->isIntegerTy(PtrSizeInBits) && Got->isPointerTy()) {
          NewValue = ConstantExpr::getPtrToInt(NewValue, Expected);
        } else {
          // TODO:
          return nullptr;
        }
      }

      return NewValue;
    }

  } else if (ROData->getNumOperands() > 2) {
    // TODO: llvm-link may merge ROData
    return nullptr;
  }

  // Load raw bytes from Data
  // We can only load integer or float
  // TODO: endian?
  if (!(ValType->isIntegerTy() || ValType->isFloatTy() || ValType->isDoubleTy()))
    return nullptr;

  const char * DataPtr = Data.data() + Offset;

  if (ValType->isIntegerTy()) {
    switch (ValType->getIntegerBitWidth()) {
    case 1: {
      int8_t val = *(const int8_t*)DataPtr;
      return ConstantInt::get(ValType, val == 0 ? 0 : 1);
    }
    case 8: {
      int8_t val = *(const int8_t*)DataPtr;
      return ConstantInt::get(ValType, uint64_t(val));
    }
    case 16: {
      int16_t val = *(const int16_t*)DataPtr;
      return ConstantInt::get(ValType, uint64_t(val));
    }
    case 32: {
      int32_t val = *(const int32_t*)DataPtr;
      return ConstantInt::get(ValType, uint64_t(val));
    }
    case 64: {
      int64_t val = *(const int64_t*)DataPtr;
      return ConstantInt::get(ValType, uint64_t(val));
    }
    default:
      return nullptr;
    }
  }

  if (ValType->isFloatTy()) {
    assert(sizeof(float) == 4 && "invariant");
    double val = (double)(*(const float*)DataPtr);
    return ConstantFP::get(ValType, val);
  }

  if (ValType->isDoubleTy()) {
    assert(sizeof(double) == 8 && "invariant");
    double val = *(const double*)DataPtr;
    return ConstantFP::get(ValType, val);
  }

  return nullptr;
}

static bool expectZeroBytes(StringRef Bytes, size_t Pos, size_t Len) {
  if (Pos + Len > (size_t)Bytes.size()) return false;
  for (size_t i = 0; i < Len; ++i)
    if (Bytes[Pos + i] != 0) return false;
  return true;
}

static APInt readAPIntBits(StringRef Bytes, size_t Off,
                           unsigned BitWidth, bool LittleEndian) {
  assert((BitWidth % 8) == 0 && "bit width must be byte-multiple");
  const unsigned N = BitWidth / 8;
  APInt Val(BitWidth, 0);
  if (LittleEndian) {
    for (unsigned i = 0; i < N; ++i)
      Val |= APInt(BitWidth, (uint64_t)(uint8_t)Bytes[Off + i]) << (8 * i);
  } else {
    for (unsigned i = 0; i < N; ++i)
      Val |= APInt(BitWidth, (uint64_t)(uint8_t)Bytes[Off + i]) << (8 * (N - 1 - i));
  }
  return Val;
}

static Constant* buildConstFromBytes(Type *T, StringRef Bytes, size_t &Off,
                                     const DataLayout &DL, LLVMContext &C,
                                     std::string *Err = nullptr,
                                     bool AllowNonZeroPointerWithoutReloc = false) {
  auto emitErr = [&](StringRef S) -> Constant* {
    if (Err) *Err = S.str();
    return nullptr;
  };

  const bool LE = DL.isLittleEndian();
  const uint64_t Store = DL.getTypeStoreSize(T);
  const uint64_t Alloc = DL.getTypeAllocSize(T);

  if (Off + Store > (uint64_t)Bytes.size())
    return emitErr("insufficient bytes for type store size");

  auto allZeroRange = [&](size_t From, size_t Len) -> bool {
    return expectZeroBytes(Bytes, From, Len);
  };

  if (auto *ITy = dyn_cast<IntegerType>(T)) {
    unsigned BW = ITy->getBitWidth();
    assert(Store == BW / 8 && "integer store size mismatch");
    APInt Bits = readAPIntBits(Bytes, Off, BW, LE);
    Off += Store;
    return ConstantInt::get(ITy, Bits);
  }

  if (T->isHalfTy() || T->isBFloatTy() || T->isFloatTy() ||
      T->isDoubleTy() || T->isFP128Ty() || T->isX86_FP80Ty() ) {
    unsigned BW = 8 * Store;
    APInt Bits = readAPIntBits(Bytes, Off, BW, LE);
    Off += Store;
    Type *IBTy = IntegerType::get(C, BW);
    Constant *IBits = ConstantInt::get(IBTy, Bits);
    return ConstantExpr::getBitCast(IBits, T);
  }

  if (auto *PTy = dyn_cast<PointerType>(T)) {
    uint64_t Need = DL.getPointerSize(PTy->getAddressSpace());
    if (Need != Store)
      return emitErr("pointer store size mismatch with DL");
    bool AllZero = allZeroRange(Off, Need);
    if (AllZero) {
      Off += Need;
      return ConstantPointerNull::get(PTy);
    }
    if (!AllowNonZeroPointerWithoutReloc)
      return emitErr("non-zero pointer without relocation is unsupported");
    APInt Bits = readAPIntBits(Bytes, Off, 8 * Need, LE);
    Off += Need;
    Type *IBTy = IntegerType::get(C, 8 * Need);
    Constant *AsInt = ConstantInt::get(IBTy, Bits);
    return ConstantExpr::getIntToPtr(AsInt, PTy);
  }

  if (auto *ATy = dyn_cast<ArrayType>(T)) {
    SmallVector<Constant*, 16> Elems;
    Elems.reserve(ATy->getNumElements());
    Type *EltTy = ATy->getElementType();
    uint64_t EltStore = DL.getTypeStoreSize(EltTy);
    uint64_t EltAlloc = DL.getTypeAllocSize(EltTy);
    size_t Base = Off; // 数组起点

    for (uint64_t i = 0, n = ATy->getNumElements(); i < n; ++i) {
      size_t Expected = Base + i * EltAlloc;
      if (Off < Expected) {
        if (!allZeroRange(Off, Expected - Off))
          return emitErr("non-zero bytes found in array element tail padding");
        Off = Expected;
      } else if (Off > Expected) {
        return emitErr("array element cursor exceeded expected position");
      }

      if (Constant *EC = buildConstFromBytes(EltTy, Bytes, Off, DL, C, Err,
                                             AllowNonZeroPointerWithoutReloc))
        Elems.push_back(EC);
      else
        return nullptr;

      size_t NextExpected = Base + (i + 1) * EltAlloc;
      if (Off < NextExpected) {
        if (!allZeroRange(Off, NextExpected - Off))
          return emitErr("non-zero bytes found in array element tail padding");
        Off = NextExpected;
      } else if (Off > NextExpected) {
        return emitErr("array element decode overran element stride");
      }
    }
    assert(Off == Base + Alloc && "array size accounting mismatch");
    return ConstantArray::get(ATy, Elems);
  }

  if (auto *VTy = dyn_cast<FixedVectorType>(T)) {
    SmallVector<Constant*, 16> Elems;
    unsigned N = VTy->getNumElements();
    Elems.reserve(N);
    Type *EltTy = VTy->getElementType();
    for (unsigned i = 0; i < N; ++i) {
      if (Constant *EC = buildConstFromBytes(EltTy, Bytes, Off, DL, C, Err,
                                             AllowNonZeroPointerWithoutReloc))
        Elems.push_back(EC);
      else
        return nullptr;
    }
    return ConstantVector::get(Elems);
  }

  if (T->isScalableTy()) {
    return emitErr("scalable vector is not supported in rodata decode");
  }

  if (auto *STy = dyn_cast<StructType>(T)) {
    const StructLayout *SL = DL.getStructLayout(STy);
    SmallVector<Constant*, 16> Fields;
    Fields.reserve(STy->getNumElements());

    size_t Base = Off;
    for (unsigned i = 0, n = STy->getNumElements(); i < n; ++i) {
      Type *EltTy = STy->getElementType(i);
      uint64_t EltOff = SL->getElementOffset(i);
      size_t Expected = Base + (size_t)EltOff;

      if (Off < Expected) {
        if (!allZeroRange(Off, Expected - Off))
          return emitErr("non-zero bytes found in struct inter-field padding");
        Off = Expected;
      } else if (Off > Expected) {
        return emitErr("struct field cursor exceeded expected position");
      }

      if (Constant *FC = buildConstFromBytes(EltTy, Bytes, Off, DL, C, Err,
                                             AllowNonZeroPointerWithoutReloc))
        Fields.push_back(FC);
      else
        return nullptr;
    }
    size_t StructEnd = Base + (size_t)SL->getSizeInBytes();
    if (Off < StructEnd) {
      if (!allZeroRange(Off, StructEnd - Off))
        return emitErr("non-zero bytes found in struct tail padding");
      Off = StructEnd;
    } else if (Off > StructEnd) {
      return emitErr("struct decode overran struct size");
    }

    return ConstantStruct::get(STy, Fields);
  }

  return emitErr("unsupported IR type in rodata decode");
}

GlobalVariable* GORoDataInitializerPass::materializeROInit(GlobalVariable &G, Module &M) {
  auto *Top = dyn_cast_or_null<MDNode>(G.getMetadata("go.rodata"));
  if (!Top) return nullptr;

  auto *MDBytes = dyn_cast<MDString>(Top->getOperand(0));
  if (!MDBytes) return nullptr;

  auto *Zero = dyn_cast_or_null<MDNode>(G.getMetadata("go.zerorodata"));
  if (Zero) return nullptr;

  StringRef Bytes = MDBytes->getString();
  GoROData data;
  if (Top->getNumOperands() == 2) {
    // load cache
    auto It = GVMap.find(&G);
    if (It == GVMap.end()) {
      data = buildRODataRelocation(Top);
      GVMap[&G] = data;
    } else {
      data = It->second;
    }

    for (auto reloc : data.relocs) {
      if (reloc.enabled == 0 || reloc.sym.empty()) {
        return nullptr;
      }
    }
  }

  auto R = data.relocs;
  auto &DL = M.getDataLayout();
  if (R.empty() && isa<StructType>(G.getValueType())) {
      LLVMContext &C = M.getContext();
      Type *T = G.getValueType();
      size_t Off = 0;
      std::string Err;
      auto AllZero = [&]{
        for (char ch : Bytes) if (ch != 0) return false;
        return true;
      }();
      Constant *Init = nullptr;
      if (AllZero) {
        if (T->isAggregateType())      Init = ConstantAggregateZero::get(T);
        else if (isa<IntegerType>(T))  Init = ConstantInt::get(T, 0);
        else if (T->isFloatingPointTy()) Init = ConstantFP::getNegativeZero(T);
        else if (isa<PointerType>(T))  Init = ConstantPointerNull::get(cast<PointerType>(T));
      }

      if (!Init) {
        // Init = buildConstFromBytes(T, Bytes, Off, DL, C, &Err /*, AllowNonZeroPtr*/);
        // if (!Init || Off != Bytes.size()) {
        //   // dbgs() << "materializeROInit failed for " << G.getName()
        //   //        << ": " << Err << "\n";
        //   return nullptr;
        // }
        return nullptr;
      }
      auto *Mat = new GlobalVariable(M, T, /*isConstant=*/true,
      GlobalValue::LinkOnceODRLinkage, Init, G.getName() + ".mat");
      if (auto A = G.getAlign()) Mat->setAlignment(*A);
      else Mat->setAlignment(Align(DL.getABITypeAlign(T)));
      Mat->setMetadata("go.rodata.materialized", MDNode::get(C, {}));
      return Mat;
  }

  sort(R, [](auto &a, auto &b){ return a.off < b.off; });

  LLVMContext &C = M.getContext();
  Type *I8  = Type::getInt8Ty(C);
  Type *Ptr = PointerType::getUnqual(C);

  SmallVector<Type*,   16> FieldsT;
  SmallVector<Constant*,16> FieldsV;

  uint64_t cur = 0;
  auto addBytes = [&](uint64_t len) {
    if (!len) return;
    auto slice = ArrayRef<uint8_t>((const uint8_t*)Bytes.data() + cur, len);
    FieldsT.push_back(ArrayType::get(I8, len));
    FieldsV.push_back(ConstantDataArray::get(C, slice));
    cur += len;
  };

  for (auto &re : R) {
    if (re.off > cur) addBytes(re.off - cur);
    Constant *P = nullptr;
    if (!re.sym.empty()) {
      if (GlobalValue *GV = M.getNamedValue(re.sym)) {
        P = ConstantExpr::getBitCast(GV, Ptr);
      }
    }
    if (re.addend != 0) {
      P = ConstantExpr::getInBoundsGetElementPtr(
            I8, P, ConstantInt::get(Type::getInt64Ty(C), re.addend));
    }

    if (P == nullptr) {
      return nullptr;
    }
    assert(P && "not null.");
    FieldsT.push_back(Ptr);
    FieldsV.push_back(P);
    cur = re.off + 8;
  }
  if (cur < Bytes.size()) addBytes(Bytes.size() - cur);

  StructType *ST = StructType::get(C, FieldsT, /*packed=*/false);
  Constant   *Init = ConstantStruct::get(ST, FieldsV);
  // assert(!isa<StructType>(G.getValueType()) && "not structure type.");
  auto *Mat = new GlobalVariable(M, ST,
      /*isConstant=*/true,
      GlobalValue::LinkOnceODRLinkage,
      Init,
      G.getName() + ".mat");

  Mat->setUnnamedAddr(GlobalValue::UnnamedAddr::Global);
  Mat->setAlignment(Align(8));
  Mat->setMetadata("go.rodata.materialized",
                   MDNode::get(C, {}));
  return Mat;
}

PreservedAnalyses GORoDataInitializerPass::run(Module &M, ModuleAnalysisManager &) {
  for (GlobalVariable &GV : M.globals()) {
    GlobalVariable *NG = materializeROInit(GV, M);
    if (NG == nullptr) {
      continue;
    }
    std::string OldName = GV.getName().str();
    GV.setName(OldName + ".old");
    auto *Aliasee = ConstantExpr::getBitCast(NG, PointerType::get(GV.getValueType(), GV.getAddressSpace()));
    auto *GA = GlobalAlias::create(
        GV.getValueType(), /*addrspace=*/GV.getAddressSpace(), GlobalValue::ExternalLinkage,
        /*Name=*/OldName + "$newalias",
        Aliasee, &M);
    GV.replaceAllUsesWith(GA);
  }
  return PreservedAnalyses::none();
}

PreservedAnalyses GORoDataRestorePass::run(Module &M, ModuleAnalysisManager &) {
  SmallVector<GlobalAlias*, 16> Targets;
  for (GlobalAlias &A : M.aliases()) {
    if (!A.hasName()) continue;
    if (!A.getName().ends_with("$newalias")) continue;
    Targets.push_back(&A);
  }

  for (GlobalAlias *GA : Targets) {
    StringRef AliasName = GA->getName();                // "<base>$newalias"
    StringRef Base = AliasName.drop_back(strlen("$newalias"));

    GlobalVariable *Old = M.getGlobalVariable((Base + ".old").str());
    GlobalVariable *Mat = M.getGlobalVariable((Base + ".mat").str());

    GlobalVariable *Rest = Old;
    if (!Rest) {
      assert(Rest && "old global value should live.");
    } else {
      if (Old->getName() != Base) Old->setName(Base);
      Old->setLinkage(GlobalValue::ExternalLinkage);
      Old->setInitializer(nullptr);
      Old->setConstant(false);
    }

    GA->replaceAllUsesWith(Rest);
    GA->eraseFromParent();
    if (Mat) Mat->eraseFromParent();
  }

  return PreservedAnalyses::none();
}