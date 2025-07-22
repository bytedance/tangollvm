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

#include "llvm-c/Core.h"
#include "llvm-c/TangoLLVM.h"
#include "llvm/ADT/APInt.h"
#include "llvm/IR/Attributes.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/DebugLoc.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/TangoLLVM/TangoLLVM.h"
#include "llvm/Support/CBindingWrapping.h"

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
void TangoLLVM::updateMetadataForInlining(Function *Callee, Function *Caller) {
  if (auto CalleeMarker = Callee->getMetadata("go.marker.reloc")) {
    if (auto CallerMarker = Caller->getMetadata("go.marker.reloc")) {
      SmallVector<Metadata*, 32> MDs(CallerMarker->op_begin(), CallerMarker->op_end());
      MDs.append(CalleeMarker->op_begin(), CalleeMarker->op_end());
      Caller->setMetadata("go.marker.reloc", MDNode::get(Caller->getContext(), MDs));
    } else {
      Caller->setMetadata("go.marker.reloc", CalleeMarker);
    }
  }


  int64_t CalleeArgAreaSize = getArgAreaSize(Callee);
  int64_t CallerArgAreaSize = getArgAreaSize(Caller);
  // no need to update if the caller has enough arg area
  if (CallerArgAreaSize < CalleeArgAreaSize) {
    // Caller's go.callee.args.area is too small, update
    auto newSize = ConstantAsMetadata::get(ConstantInt::get(
          Type::getInt64Ty(Caller->getContext()), CalleeArgAreaSize));
    auto newType =
      ConstantAsMetadata::get(ConstantAggregateZero::get(ArrayType::get(
              Type::getInt8Ty(Caller->getContext()), CalleeArgAreaSize)));
    Caller->setMetadata("go.callee.args.area",
        MDNode::get(Caller->getContext(), {newSize, newType}));

  }
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
  Module *M = Fn->getParent();

  // 0). Create a new function with empty name
  Function *NewFn = Function::Create(Fn->getFunctionType(), Fn->getLinkage(), "", M);

  // 1). Replace the old function by the new function
  Fn->replaceAllUsesWith(NewFn);
  NewFn->setCallingConv(Fn->getCallingConv());
  NewFn->copyMetadata(Fn, 0);
  NewFn->setMetadata(LLVMContext::MD_dbg, nullptr);
  NewFn->setAttributes(Fn->getAttributes());
  NewFn->takeName(Fn);

  // 2). Update the name of the old function
  std::string NewName = Name.str();
  NewName += "..obsolete";
  Fn->setName(StringRef(NewName));
  Fn->addFnAttr("go.obsolete");

  assert(NewFn->getName() == Name && "no renaming");
}

Function *TangoLLVM::createGoFunction(FunctionType *Ty, GlobalValue::LinkageTypes Linkage,
      StringRef N, Module *M) {
  GlobalValue *GV = M->getNamedValue(N);
  if (GV) {
    if (isa<Function>(GV))
      report_fatal_error("check existing function before creating a new one");

    if (!isa<GlobalVariable>(GV))
      report_fatal_error("only global variable can be replaced");
  }

  Function *NewFn = Function::Create(Ty, Linkage, N, M);
  NewFn->setMetadata("go.global", MDNode::get(M->getContext(), {}));

  if (GV) {
    Value *NewGV = NewFn;
    if (GV->getType() != NewFn->getType()) {
      NewGV = ConstantExpr::getBitCast(NewFn, GV->getType());
    }
    GV->replaceAllUsesWith(NewGV);
    NewFn->takeName(GV);
    GV->eraseFromParent();
  }

  assert(NewFn->getName() == N && "no renaming");
  return NewFn;
}

GlobalVariable *TangoLLVM::createGoGlobalVariable(Type *Ty, GlobalValue::LinkageTypes Linkage,
      StringRef N, Module *M) {
  GlobalValue *GV = M->getNamedValue(N);
  if (GV) {
    report_fatal_error("check existing global variable before creating a new one");
    return nullptr;
  }

  GlobalVariable *G = new GlobalVariable(*M, Ty, /*IsConstant*/ false, Linkage, nullptr, N);
  G->setMetadata("go.global", MDNode::get(M->getContext(), {}));
  return G;
}


} // namespace llvm

// llvm-c/TangoLLVM.h

using namespace llvm;

void LLVMInitializeAll(void) {
  LLVMInitializeAllTargets();
  LLVMInitializeAllTargetMCs();
  LLVMInitializeAllTargetInfos();
  LLVMInitializeAllAsmParsers();
  LLVMInitializeAllAsmPrinters();
}

LLVMMetadataRef LLVMConstantAsMetadata(LLVMValueRef C) {
  return wrap(ConstantAsMetadata::get(unwrap<Constant>(C)));
}

LLVMValueRef LLVMConstAggregateZero(LLVMTypeRef Ty) {
  return wrap(ConstantAggregateZero::get(unwrap<Type>(Ty)));
}

LLVMTypeRef LLVMGetFunctionType(LLVMValueRef Fn) {
  return wrap(unwrap<Function>(Fn)->getValueType());
}

LLVMMetadataRef LLVMMDString2(LLVMContextRef C, const char *Str, unsigned SLen) {
  return wrap(MDString::get(*unwrap(C), StringRef(Str, SLen)));
}

LLVMMetadataRef LLVMMDNode2(LLVMContextRef C, LLVMMetadataRef *MDs,
                            unsigned Count) {
  return wrap(
      MDNode::get(*unwrap(C), ArrayRef<Metadata *>(unwrap(MDs), Count)));
}

void LLVMAddNamedMetadataOperand2(LLVMModuleRef M, const char *name,
                                  LLVMMetadataRef Val) {
  NamedMDNode *N = unwrap(M)->getOrInsertNamedMetadata(name);
  if (!N)
    return;
  if (!Val)
    return;
  N->addOperand(unwrap<MDNode>(Val));
}

void LLVMGlobalObjectAddMetadata(LLVMValueRef Global, unsigned KindID, LLVMMetadataRef MD) {
  llvm::MDNode *N = MD ? llvm::unwrap<llvm::MDNode>(MD) : nullptr;
  llvm::GlobalObject *O = llvm::unwrap<llvm::GlobalObject>(Global);
  O->addMetadata(KindID, *N);
}

void LLVMSetMetadata2(LLVMValueRef Inst, unsigned KindID, LLVMMetadataRef MD) {
  MDNode *N = MD ? unwrap<MDNode>(MD) : nullptr;
  unwrap<Instruction>(Inst)->setMetadata(KindID, N);
}

void LLVMGoSetCurrentDebugLocation(LLVMBuilderRef Bref, unsigned Line,
                                  unsigned Col, LLVMMetadataRef Scope,
                                  LLVMMetadataRef InlinedAt) {
  if (!Scope)
    unwrap(Bref)->SetCurrentDebugLocation(DebugLoc());
  else
    unwrap(Bref)->SetCurrentDebugLocation(DILocation::get(
        unwrap<MDNode>(Scope)->getContext(), Line, Col, unwrap<MDNode>(Scope),
        InlinedAt ? unwrap<MDNode>(InlinedAt) : nullptr));
}

LLVMDebugLocMetadata LLVMGoGetCurrentDebugLocation(LLVMBuilderRef Bref) {
  const auto& Loc = unwrap(Bref)->getCurrentDebugLocation();
  const auto* InlinedAt = Loc.getInlinedAt();
  const LLVMDebugLocMetadata md{
    Loc.getLine(),
    Loc.getCol(),
    wrap(Loc.getScope()),
    InlinedAt == nullptr ? nullptr : wrap(InlinedAt->getRawInlinedAt()),
  };
  return md;
}

LLVMValueRef LLVMGoGetInlineAsm(LLVMTypeRef Ty, char *AsmString,
                                size_t AsmStringSize, char *Constraints,
                                size_t ConstraintsSize, LLVMBool HasSideEffects,
                                LLVMBool IsAlignStack,
                                LLVMInlineAsmDialect Dialect, LLVMBool CanThrow)
{
  return LLVMGetInlineAsm(Ty, AsmString,
                          AsmStringSize, Constraints,
                          ConstraintsSize, HasSideEffects,
                          IsAlignStack,
                          Dialect, CanThrow);
}

LLVMValueRef LLVMBuildMemCpyInline(LLVMBuilderRef B,
                             LLVMValueRef Dst, unsigned DstAlign,
                             LLVMValueRef Src, unsigned SrcAlign,
                             LLVMValueRef Size) {
  return wrap(unwrap(B)->CreateMemCpyInline(unwrap(Dst), MaybeAlign(DstAlign),
                                      unwrap(Src), MaybeAlign(SrcAlign),
                                      unwrap(Size)));
}

LLVMValueRef LLVMBuildGEPByOffset(LLVMBuilderRef B, LLVMValueRef Base, LLVMTypeRef ElemType, uint64_t Off) {
  Value* BasePtr = unwrap(Base);
  Type * Ty = BasePtr->getType();
  if (!Ty->isPointerTy()) {
    // caller should check the error case
    return nullptr;
  }

  BasicBlock* BB = unwrap(B)->GetInsertBlock();
  const DataLayout &DL = BB->getModule()->getDataLayout();
  APInt Offset = APInt(64, Off, false);

  Type *SrcElemTy = unwrap(ElemType); // getValueElementType(BasePtr);
  Type *ElemTy = SrcElemTy; // getGEPIndicesForOffset may modify ElemTy
  SmallVector<APInt> Indices = DL.getGEPIndicesForOffset(ElemTy, Offset);
  if (!Offset.isZero())
    return nullptr;

  SmallVector<Value*, 8> NewIndices;
  for (auto &I : Indices) {
    NewIndices.push_back(ConstantInt::get(BB->getContext(), I));
  }

  return wrap(unwrap(B)->CreateGEP(SrcElemTy, BasePtr, NewIndices));
}

void LLVMAddCallSiteFnAttribute(LLVMValueRef C, LLVMAttributeRef A) {
  unwrap<CallBase>(C)->addFnAttr(unwrap(A));
}

void LLVMRemoveFunctionBody(LLVMValueRef C) {
  Function * Fn = unwrap<Function>(C);
  if (Fn->empty()) {
    return;
  }
  std::vector<BasicBlock*> BBS;
  BBS.reserve(Fn->size());
  BasicBlock *Entry = &Fn->getEntryBlock();
  // 1. unlink all instructions
  for (BasicBlock &BB : *Fn) {
    BBS.push_back(&BB);
    for (Instruction &I : BB) {
        if (I.use_empty()) {
            continue;
        }
        I.replaceAllUsesWith(UndefValue::get(I.getType()));
    }
  }
  // unlink all basic blocks except Entry
  for (BasicBlock *BB : BBS) {
    if (BB == Entry) {
      continue;
    }
    BB->replaceAllUsesWith(Entry);
    BB->eraseFromParent();
  }
  // Remove entry
  Entry->eraseFromParent();
  Fn->setSubprogram(nullptr);
}


LLVMValueRef LLVMBuildIntrinsic(LLVMBuilderRef B, unsigned IntrinsicID,
      LLVMTypeRef *Types,
      unsigned NumTypes,
      LLVMValueRef *Args,
      unsigned NumArgs, const char *Name) {
  return wrap(unwrap(B)->CreateIntrinsic((Intrinsic::ID)IntrinsicID,
        ArrayRef(unwrap(Types), NumTypes),
        ArrayRef(unwrap(Args), NumArgs), nullptr, Name));
}

TangoLLVMEnv* TangoLLVMCreateTangoLLVMEnvironment() {

  TangoLLVMEnv *ENV = new TangoLLVMEnv;
  memset(ENV, 0, sizeof(TangoLLVMEnv));

#define TANGO_LLVM_ASSIGN_ENV_MEMBER(name) \
  ENV->TangoLLVMIsA##name = &LLVMIsA##name;
  LLVM_FOR_EACH_VALUE_SUBCLASS(TANGO_LLVM_ASSIGN_ENV_MEMBER)

  ENV->TangoLLVMInitializeAll = &LLVMInitializeAll;

  ENV->TangoLLVMConstantAsMetadata = &LLVMConstantAsMetadata;
  ENV->TangoLLVMConstAggregateZero = &LLVMConstAggregateZero;
  ENV->TangoLLVMGetFunctionType = &LLVMGetFunctionType;
  ENV->TangoLLVMMDString2 = &LLVMMDString2;
  ENV->TangoLLVMMDNode2 = &LLVMMDNode2;
  ENV->TangoLLVMGlobalObjectAddMetadata = &LLVMGlobalObjectAddMetadata;
  ENV->TangoLLVMAddNamedMetadataOperand2 = &LLVMAddNamedMetadataOperand2;
  ENV->TangoLLVMSetMetadata2 = &LLVMSetMetadata2;
  ENV->TangoLLVMGoSetCurrentDebugLocation = &LLVMGoSetCurrentDebugLocation;
  ENV->TangoLLVMGoGetCurrentDebugLocation = &LLVMGoGetCurrentDebugLocation;
  ENV->TangoLLVMGoGetInlineAsm = &LLVMGoGetInlineAsm;
  ENV->TangoLLVMBuildMemCpyInline = &LLVMBuildMemCpyInline;
  ENV->TangoLLVMBuildGEPByOffset = &LLVMBuildGEPByOffset;
  ENV->TangoLLVMAddCallSiteFnAttribute = &LLVMAddCallSiteFnAttribute;

  ENV->TangoLLVMShutdown = &LLVMShutdown;
  ENV->TangoLLVMGetVersion = &LLVMGetVersion;
  ENV->TangoLLVMCreateMessage = &LLVMCreateMessage;
  ENV->TangoLLVMDisposeMessage = &LLVMDisposeMessage;
  ENV->TangoLLVMContextCreate = &LLVMContextCreate;
  ENV->TangoLLVMGetGlobalContext = &LLVMGetGlobalContext;
  ENV->TangoLLVMContextSetDiagnosticHandler = &LLVMContextSetDiagnosticHandler;
  ENV->TangoLLVMContextGetDiagnosticHandler = &LLVMContextGetDiagnosticHandler;
  ENV->TangoLLVMContextGetDiagnosticContext = &LLVMContextGetDiagnosticContext;
  ENV->TangoLLVMContextSetYieldCallback = &LLVMContextSetYieldCallback;
  ENV->TangoLLVMContextShouldDiscardValueNames = &LLVMContextShouldDiscardValueNames;
  ENV->TangoLLVMContextSetDiscardValueNames = &LLVMContextSetDiscardValueNames;
  ENV->TangoLLVMContextDispose = &LLVMContextDispose;
  ENV->TangoLLVMGetDiagInfoDescription = &LLVMGetDiagInfoDescription;
  ENV->TangoLLVMGetDiagInfoSeverity = &LLVMGetDiagInfoSeverity;
  ENV->TangoLLVMGetMDKindIDInContext = &LLVMGetMDKindIDInContext;
  ENV->TangoLLVMGetMDKindID = &LLVMGetMDKindID;
  ENV->TangoLLVMGetEnumAttributeKindForName = &LLVMGetEnumAttributeKindForName;
  ENV->TangoLLVMGetLastEnumAttributeKind = &LLVMGetLastEnumAttributeKind;
  ENV->TangoLLVMCreateEnumAttribute = &LLVMCreateEnumAttribute;
  ENV->TangoLLVMGetEnumAttributeKind = &LLVMGetEnumAttributeKind;
  ENV->TangoLLVMGetEnumAttributeValue = &LLVMGetEnumAttributeValue;
  ENV->TangoLLVMCreateTypeAttribute = &LLVMCreateTypeAttribute;
  ENV->TangoLLVMGetTypeAttributeValue = &LLVMGetTypeAttributeValue;
  ENV->TangoLLVMCreateConstantRangeAttribute = &LLVMCreateConstantRangeAttribute;
  ENV->TangoLLVMCreateStringAttribute = &LLVMCreateStringAttribute;
  ENV->TangoLLVMGetStringAttributeKind = &LLVMGetStringAttributeKind;
  ENV->TangoLLVMGetStringAttributeValue = &LLVMGetStringAttributeValue;
  ENV->TangoLLVMIsEnumAttribute = &LLVMIsEnumAttribute;
  ENV->TangoLLVMIsStringAttribute = &LLVMIsStringAttribute;
  ENV->TangoLLVMIsTypeAttribute = &LLVMIsTypeAttribute;
  ENV->TangoLLVMGetTypeByName2 = &LLVMGetTypeByName2;
  ENV->TangoLLVMModuleCreateWithName = &LLVMModuleCreateWithName;
  ENV->TangoLLVMModuleCreateWithNameInContext = &LLVMModuleCreateWithNameInContext;
  ENV->TangoLLVMCloneModule = &LLVMCloneModule;
  ENV->TangoLLVMDisposeModule = &LLVMDisposeModule;
  ENV->TangoLLVMIsNewDbgInfoFormat = &LLVMIsNewDbgInfoFormat;
  ENV->TangoLLVMSetIsNewDbgInfoFormat = &LLVMSetIsNewDbgInfoFormat;
  ENV->TangoLLVMGetModuleIdentifier = &LLVMGetModuleIdentifier;
  ENV->TangoLLVMSetModuleIdentifier = &LLVMSetModuleIdentifier;
  ENV->TangoLLVMGetSourceFileName = &LLVMGetSourceFileName;
  ENV->TangoLLVMSetSourceFileName = &LLVMSetSourceFileName;
  ENV->TangoLLVMGetDataLayoutStr = &LLVMGetDataLayoutStr;
  ENV->TangoLLVMGetDataLayout = &LLVMGetDataLayout;
  ENV->TangoLLVMSetDataLayout = &LLVMSetDataLayout;
  ENV->TangoLLVMGetTarget = &LLVMGetTarget;
  ENV->TangoLLVMSetTarget = &LLVMSetTarget;
  ENV->TangoLLVMCopyModuleFlagsMetadata = &LLVMCopyModuleFlagsMetadata;
  ENV->TangoLLVMDisposeModuleFlagsMetadata = &LLVMDisposeModuleFlagsMetadata;
  ENV->TangoLLVMModuleFlagEntriesGetFlagBehavior = &LLVMModuleFlagEntriesGetFlagBehavior;
  ENV->TangoLLVMModuleFlagEntriesGetKey = &LLVMModuleFlagEntriesGetKey;
  ENV->TangoLLVMModuleFlagEntriesGetMetadata = &LLVMModuleFlagEntriesGetMetadata;
  ENV->TangoLLVMGetModuleFlag = &LLVMGetModuleFlag;
  ENV->TangoLLVMAddModuleFlag = &LLVMAddModuleFlag;
  ENV->TangoLLVMDumpModule = &LLVMDumpModule;
  ENV->TangoLLVMPrintModuleToFile = &LLVMPrintModuleToFile;
  ENV->TangoLLVMPrintModuleToString = &LLVMPrintModuleToString;
  ENV->TangoLLVMGetModuleInlineAsm = &LLVMGetModuleInlineAsm;
  ENV->TangoLLVMSetModuleInlineAsm2 = &LLVMSetModuleInlineAsm2;
  ENV->TangoLLVMAppendModuleInlineAsm = &LLVMAppendModuleInlineAsm;
  ENV->TangoLLVMGetInlineAsm = &LLVMGetInlineAsm;
  ENV->TangoLLVMGetInlineAsmAsmString = &LLVMGetInlineAsmAsmString;
  ENV->TangoLLVMGetInlineAsmConstraintString = &LLVMGetInlineAsmConstraintString;
  ENV->TangoLLVMGetInlineAsmDialect = &LLVMGetInlineAsmDialect;
  ENV->TangoLLVMGetInlineAsmFunctionType = &LLVMGetInlineAsmFunctionType;
  ENV->TangoLLVMGetInlineAsmHasSideEffects = &LLVMGetInlineAsmHasSideEffects;
  ENV->TangoLLVMGetInlineAsmNeedsAlignedStack = &LLVMGetInlineAsmNeedsAlignedStack;
  ENV->TangoLLVMGetInlineAsmCanUnwind = &LLVMGetInlineAsmCanUnwind;
  ENV->TangoLLVMGetModuleContext = &LLVMGetModuleContext;
  ENV->TangoLLVMGetTypeByName = &LLVMGetTypeByName;
  ENV->TangoLLVMGetFirstNamedMetadata = &LLVMGetFirstNamedMetadata;
  ENV->TangoLLVMGetLastNamedMetadata = &LLVMGetLastNamedMetadata;
  ENV->TangoLLVMGetNextNamedMetadata = &LLVMGetNextNamedMetadata;
  ENV->TangoLLVMGetPreviousNamedMetadata = &LLVMGetPreviousNamedMetadata;
  ENV->TangoLLVMGetNamedMetadata = &LLVMGetNamedMetadata;
  ENV->TangoLLVMGetOrInsertNamedMetadata = &LLVMGetOrInsertNamedMetadata;
  ENV->TangoLLVMGetNamedMetadataName = &LLVMGetNamedMetadataName;
  ENV->TangoLLVMGetNamedMetadataNumOperands = &LLVMGetNamedMetadataNumOperands;
  ENV->TangoLLVMGetNamedMetadataOperands = &LLVMGetNamedMetadataOperands;
  ENV->TangoLLVMAddNamedMetadataOperand = &LLVMAddNamedMetadataOperand;
  ENV->TangoLLVMGetDebugLocDirectory = &LLVMGetDebugLocDirectory;
  ENV->TangoLLVMGetDebugLocFilename = &LLVMGetDebugLocFilename;
  ENV->TangoLLVMGetDebugLocLine = &LLVMGetDebugLocLine;
  ENV->TangoLLVMGetDebugLocColumn = &LLVMGetDebugLocColumn;
  ENV->TangoLLVMAddFunction = &LLVMAddFunction;
  ENV->TangoLLVMGetNamedFunction = &LLVMGetNamedFunction;
  ENV->TangoLLVMGetFirstFunction = &LLVMGetFirstFunction;
  ENV->TangoLLVMGetLastFunction = &LLVMGetLastFunction;
  ENV->TangoLLVMGetNextFunction = &LLVMGetNextFunction;
  ENV->TangoLLVMGetPreviousFunction = &LLVMGetPreviousFunction;
  ENV->TangoLLVMSetModuleInlineAsm = &LLVMSetModuleInlineAsm;
  ENV->TangoLLVMGetTypeKind = &LLVMGetTypeKind;
  ENV->TangoLLVMTypeIsSized = &LLVMTypeIsSized;
  ENV->TangoLLVMGetTypeContext = &LLVMGetTypeContext;
  ENV->TangoLLVMDumpType = &LLVMDumpType;
  ENV->TangoLLVMPrintTypeToString = &LLVMPrintTypeToString;
  ENV->TangoLLVMInt1TypeInContext = &LLVMInt1TypeInContext;
  ENV->TangoLLVMInt8TypeInContext = &LLVMInt8TypeInContext;
  ENV->TangoLLVMInt16TypeInContext = &LLVMInt16TypeInContext;
  ENV->TangoLLVMInt32TypeInContext = &LLVMInt32TypeInContext;
  ENV->TangoLLVMInt64TypeInContext = &LLVMInt64TypeInContext;
  ENV->TangoLLVMInt128TypeInContext = &LLVMInt128TypeInContext;
  ENV->TangoLLVMIntTypeInContext = &LLVMIntTypeInContext;
  ENV->TangoLLVMInt1Type = &LLVMInt1Type;
  ENV->TangoLLVMInt8Type = &LLVMInt8Type;
  ENV->TangoLLVMInt16Type = &LLVMInt16Type;
  ENV->TangoLLVMInt32Type = &LLVMInt32Type;
  ENV->TangoLLVMInt64Type = &LLVMInt64Type;
  ENV->TangoLLVMInt128Type = &LLVMInt128Type;
  ENV->TangoLLVMIntType = &LLVMIntType;
  ENV->TangoLLVMGetIntTypeWidth = &LLVMGetIntTypeWidth;
  ENV->TangoLLVMHalfTypeInContext = &LLVMHalfTypeInContext;
  ENV->TangoLLVMBFloatTypeInContext = &LLVMBFloatTypeInContext;
  ENV->TangoLLVMFloatTypeInContext = &LLVMFloatTypeInContext;
  ENV->TangoLLVMDoubleTypeInContext = &LLVMDoubleTypeInContext;
  ENV->TangoLLVMX86FP80TypeInContext = &LLVMX86FP80TypeInContext;
  ENV->TangoLLVMFP128TypeInContext = &LLVMFP128TypeInContext;
  ENV->TangoLLVMPPCFP128TypeInContext = &LLVMPPCFP128TypeInContext;
  ENV->TangoLLVMHalfType = &LLVMHalfType;
  ENV->TangoLLVMBFloatType = &LLVMBFloatType;
  ENV->TangoLLVMFloatType = &LLVMFloatType;
  ENV->TangoLLVMDoubleType = &LLVMDoubleType;
  ENV->TangoLLVMX86FP80Type = &LLVMX86FP80Type;
  ENV->TangoLLVMFP128Type = &LLVMFP128Type;
  ENV->TangoLLVMPPCFP128Type = &LLVMPPCFP128Type;
  ENV->TangoLLVMFunctionType = &LLVMFunctionType;
  ENV->TangoLLVMIsFunctionVarArg = &LLVMIsFunctionVarArg;
  ENV->TangoLLVMGetReturnType = &LLVMGetReturnType;
  ENV->TangoLLVMCountParamTypes = &LLVMCountParamTypes;
  ENV->TangoLLVMGetParamTypes = &LLVMGetParamTypes;
  ENV->TangoLLVMStructTypeInContext = &LLVMStructTypeInContext;
  ENV->TangoLLVMStructType = &LLVMStructType;
  ENV->TangoLLVMStructCreateNamed = &LLVMStructCreateNamed;
  ENV->TangoLLVMGetStructName = &LLVMGetStructName;
  ENV->TangoLLVMStructSetBody = &LLVMStructSetBody;
  ENV->TangoLLVMCountStructElementTypes = &LLVMCountStructElementTypes;
  ENV->TangoLLVMGetStructElementTypes = &LLVMGetStructElementTypes;
  ENV->TangoLLVMStructGetTypeAtIndex = &LLVMStructGetTypeAtIndex;
  ENV->TangoLLVMIsPackedStruct = &LLVMIsPackedStruct;
  ENV->TangoLLVMIsOpaqueStruct = &LLVMIsOpaqueStruct;
  ENV->TangoLLVMIsLiteralStruct = &LLVMIsLiteralStruct;
  ENV->TangoLLVMGetElementType = &LLVMGetElementType;
  ENV->TangoLLVMGetSubtypes = &LLVMGetSubtypes;
  ENV->TangoLLVMGetNumContainedTypes = &LLVMGetNumContainedTypes;
  ENV->TangoLLVMArrayType = &LLVMArrayType;
  ENV->TangoLLVMArrayType2 = &LLVMArrayType2;
  ENV->TangoLLVMGetArrayLength = &LLVMGetArrayLength;
  ENV->TangoLLVMGetArrayLength2 = &LLVMGetArrayLength2;
  ENV->TangoLLVMPointerType = &LLVMPointerType;
  ENV->TangoLLVMPointerTypeIsOpaque = &LLVMPointerTypeIsOpaque;
  ENV->TangoLLVMPointerTypeInContext = &LLVMPointerTypeInContext;
  ENV->TangoLLVMGetPointerAddressSpace = &LLVMGetPointerAddressSpace;
  ENV->TangoLLVMVectorType = &LLVMVectorType;
  ENV->TangoLLVMScalableVectorType = &LLVMScalableVectorType;
  ENV->TangoLLVMGetVectorSize = &LLVMGetVectorSize;
  ENV->TangoLLVMGetConstantPtrAuthPointer = &LLVMGetConstantPtrAuthPointer;
  ENV->TangoLLVMGetConstantPtrAuthKey = &LLVMGetConstantPtrAuthKey;
  ENV->TangoLLVMGetConstantPtrAuthDiscriminator = &LLVMGetConstantPtrAuthDiscriminator;
  ENV->TangoLLVMGetConstantPtrAuthAddrDiscriminator = &LLVMGetConstantPtrAuthAddrDiscriminator;
  ENV->TangoLLVMVoidTypeInContext = &LLVMVoidTypeInContext;
  ENV->TangoLLVMLabelTypeInContext = &LLVMLabelTypeInContext;
  ENV->TangoLLVMX86MMXTypeInContext = &LLVMX86MMXTypeInContext;
  ENV->TangoLLVMX86AMXTypeInContext = &LLVMX86AMXTypeInContext;
  ENV->TangoLLVMTokenTypeInContext = &LLVMTokenTypeInContext;
  ENV->TangoLLVMMetadataTypeInContext = &LLVMMetadataTypeInContext;
  ENV->TangoLLVMVoidType = &LLVMVoidType;
  ENV->TangoLLVMLabelType = &LLVMLabelType;
  ENV->TangoLLVMX86MMXType = &LLVMX86MMXType;
  ENV->TangoLLVMX86AMXType = &LLVMX86AMXType;
  ENV->TangoLLVMTargetExtTypeInContext = &LLVMTargetExtTypeInContext;
  ENV->TangoLLVMGetTargetExtTypeName = &LLVMGetTargetExtTypeName;
  ENV->TangoLLVMGetTargetExtTypeNumTypeParams = &LLVMGetTargetExtTypeNumTypeParams;
  ENV->TangoLLVMGetTargetExtTypeTypeParam = &LLVMGetTargetExtTypeTypeParam;
  ENV->TangoLLVMGetTargetExtTypeNumIntParams = &LLVMGetTargetExtTypeNumIntParams;
  ENV->TangoLLVMGetTargetExtTypeIntParam = &LLVMGetTargetExtTypeIntParam;
  ENV->TangoLLVMTypeOf = &LLVMTypeOf;
  ENV->TangoLLVMGetValueKind = &LLVMGetValueKind;
  ENV->TangoLLVMGetValueName2 = &LLVMGetValueName2;
  ENV->TangoLLVMSetValueName2 = &LLVMSetValueName2;
  ENV->TangoLLVMDumpValue = &LLVMDumpValue;
  ENV->TangoLLVMPrintValueToString = &LLVMPrintValueToString;
  ENV->TangoLLVMPrintDbgRecordToString = &LLVMPrintDbgRecordToString;
  ENV->TangoLLVMReplaceAllUsesWith = &LLVMReplaceAllUsesWith;
  ENV->TangoLLVMIsConstant = &LLVMIsConstant;
  ENV->TangoLLVMIsUndef = &LLVMIsUndef;
  ENV->TangoLLVMIsPoison = &LLVMIsPoison;
  ENV->TangoLLVMIsAMDNode = &LLVMIsAMDNode;
  ENV->TangoLLVMIsAValueAsMetadata = &LLVMIsAValueAsMetadata;
  ENV->TangoLLVMIsAMDString = &LLVMIsAMDString;
  ENV->TangoLLVMGetValueName = &LLVMGetValueName;
  ENV->TangoLLVMSetValueName = &LLVMSetValueName;
  ENV->TangoLLVMGetFirstUse = &LLVMGetFirstUse;
  ENV->TangoLLVMGetNextUse = &LLVMGetNextUse;
  ENV->TangoLLVMGetUser = &LLVMGetUser;
  ENV->TangoLLVMGetUsedValue = &LLVMGetUsedValue;
  ENV->TangoLLVMGetOperand = &LLVMGetOperand;
  ENV->TangoLLVMGetOperandUse = &LLVMGetOperandUse;
  ENV->TangoLLVMSetOperand = &LLVMSetOperand;
  ENV->TangoLLVMGetNumOperands = &LLVMGetNumOperands;
  ENV->TangoLLVMConstNull = &LLVMConstNull;
  ENV->TangoLLVMConstAllOnes = &LLVMConstAllOnes;
  ENV->TangoLLVMGetUndef = &LLVMGetUndef;
  ENV->TangoLLVMGetPoison = &LLVMGetPoison;
  ENV->TangoLLVMIsNull = &LLVMIsNull;
  ENV->TangoLLVMConstPointerNull = &LLVMConstPointerNull;
  ENV->TangoLLVMConstInt = &LLVMConstInt;
  ENV->TangoLLVMConstIntOfArbitraryPrecision = &LLVMConstIntOfArbitraryPrecision;
  ENV->TangoLLVMConstIntOfString = &LLVMConstIntOfString;
  ENV->TangoLLVMConstIntOfStringAndSize = &LLVMConstIntOfStringAndSize;
  ENV->TangoLLVMConstReal = &LLVMConstReal;
  ENV->TangoLLVMConstRealOfString = &LLVMConstRealOfString;
  ENV->TangoLLVMConstRealOfStringAndSize = &LLVMConstRealOfStringAndSize;
  ENV->TangoLLVMConstIntGetZExtValue = &LLVMConstIntGetZExtValue;
  ENV->TangoLLVMConstIntGetSExtValue = &LLVMConstIntGetSExtValue;
  ENV->TangoLLVMConstRealGetDouble = &LLVMConstRealGetDouble;
  ENV->TangoLLVMConstStringInContext = &LLVMConstStringInContext;
  ENV->TangoLLVMConstStringInContext2 = &LLVMConstStringInContext2;
  ENV->TangoLLVMConstString = &LLVMConstString;
  ENV->TangoLLVMIsConstantString = &LLVMIsConstantString;
  ENV->TangoLLVMGetAsString = &LLVMGetAsString;
  ENV->TangoLLVMConstStructInContext = &LLVMConstStructInContext;
  ENV->TangoLLVMConstStruct = &LLVMConstStruct;
  ENV->TangoLLVMConstArray = &LLVMConstArray;
  ENV->TangoLLVMConstArray2 = &LLVMConstArray2;
  ENV->TangoLLVMConstNamedStruct = &LLVMConstNamedStruct;
  ENV->TangoLLVMGetAggregateElement = &LLVMGetAggregateElement;
  ENV->TangoLLVMGetElementAsConstant = &LLVMGetElementAsConstant;
  ENV->TangoLLVMConstVector = &LLVMConstVector;
  ENV->TangoLLVMConstantPtrAuth = &LLVMConstantPtrAuth;
  ENV->TangoLLVMGetConstOpcode = &LLVMGetConstOpcode;
  ENV->TangoLLVMAlignOf = &LLVMAlignOf;
  ENV->TangoLLVMSizeOf = &LLVMSizeOf;
  ENV->TangoLLVMConstNeg = &LLVMConstNeg;
  ENV->TangoLLVMConstNSWNeg = &LLVMConstNSWNeg;
  ENV->TangoLLVMConstNUWNeg = &LLVMConstNUWNeg;
  ENV->TangoLLVMConstNot = &LLVMConstNot;
  ENV->TangoLLVMConstAdd = &LLVMConstAdd;
  ENV->TangoLLVMConstNSWAdd = &LLVMConstNSWAdd;
  ENV->TangoLLVMConstNUWAdd = &LLVMConstNUWAdd;
  ENV->TangoLLVMConstSub = &LLVMConstSub;
  ENV->TangoLLVMConstNSWSub = &LLVMConstNSWSub;
  ENV->TangoLLVMConstNUWSub = &LLVMConstNUWSub;
  ENV->TangoLLVMConstMul = &LLVMConstMul;
  ENV->TangoLLVMConstNSWMul = &LLVMConstNSWMul;
  ENV->TangoLLVMConstNUWMul = &LLVMConstNUWMul;
  ENV->TangoLLVMConstXor = &LLVMConstXor;
  ENV->TangoLLVMConstGEP2 = &LLVMConstGEP2;
  ENV->TangoLLVMConstInBoundsGEP2 = &LLVMConstInBoundsGEP2;
  ENV->TangoLLVMConstGEPWithNoWrapFlags = &LLVMConstGEPWithNoWrapFlags;
  ENV->TangoLLVMConstTrunc = &LLVMConstTrunc;
  ENV->TangoLLVMConstPtrToInt = &LLVMConstPtrToInt;
  ENV->TangoLLVMConstIntToPtr = &LLVMConstIntToPtr;
  ENV->TangoLLVMConstBitCast = &LLVMConstBitCast;
  ENV->TangoLLVMConstAddrSpaceCast = &LLVMConstAddrSpaceCast;
  ENV->TangoLLVMConstTruncOrBitCast = &LLVMConstTruncOrBitCast;
  ENV->TangoLLVMConstPointerCast = &LLVMConstPointerCast;
  ENV->TangoLLVMConstExtractElement = &LLVMConstExtractElement;
  ENV->TangoLLVMConstInsertElement = &LLVMConstInsertElement;
  ENV->TangoLLVMConstShuffleVector = &LLVMConstShuffleVector;
  ENV->TangoLLVMBlockAddress = &LLVMBlockAddress;
  ENV->TangoLLVMGetBlockAddressFunction = &LLVMGetBlockAddressFunction;
  ENV->TangoLLVMGetBlockAddressBasicBlock = &LLVMGetBlockAddressBasicBlock;
  ENV->TangoLLVMConstInlineAsm = &LLVMConstInlineAsm;
  ENV->TangoLLVMGetGlobalParent = &LLVMGetGlobalParent;
  ENV->TangoLLVMIsDeclaration = &LLVMIsDeclaration;
  ENV->TangoLLVMGetLinkage = &LLVMGetLinkage;
  ENV->TangoLLVMSetLinkage = &LLVMSetLinkage;
  ENV->TangoLLVMGetSection = &LLVMGetSection;
  ENV->TangoLLVMSetSection = &LLVMSetSection;
  ENV->TangoLLVMGetVisibility = &LLVMGetVisibility;
  ENV->TangoLLVMSetVisibility = &LLVMSetVisibility;
  ENV->TangoLLVMGetDLLStorageClass = &LLVMGetDLLStorageClass;
  ENV->TangoLLVMSetDLLStorageClass = &LLVMSetDLLStorageClass;
  ENV->TangoLLVMGetUnnamedAddress = &LLVMGetUnnamedAddress;
  ENV->TangoLLVMSetUnnamedAddress = &LLVMSetUnnamedAddress;
  ENV->TangoLLVMGlobalGetValueType = &LLVMGlobalGetValueType;
  ENV->TangoLLVMHasUnnamedAddr = &LLVMHasUnnamedAddr;
  ENV->TangoLLVMSetUnnamedAddr = &LLVMSetUnnamedAddr;
  ENV->TangoLLVMGetAlignment = &LLVMGetAlignment;
  ENV->TangoLLVMSetAlignment = &LLVMSetAlignment;
  ENV->TangoLLVMGlobalSetMetadata = &LLVMGlobalSetMetadata;
  ENV->TangoLLVMGlobalEraseMetadata = &LLVMGlobalEraseMetadata;
  ENV->TangoLLVMGlobalClearMetadata = &LLVMGlobalClearMetadata;
  ENV->TangoLLVMGlobalCopyAllMetadata = &LLVMGlobalCopyAllMetadata;
  ENV->TangoLLVMDisposeValueMetadataEntries = &LLVMDisposeValueMetadataEntries;
  ENV->TangoLLVMValueMetadataEntriesGetKind = &LLVMValueMetadataEntriesGetKind;
  ENV->TangoLLVMValueMetadataEntriesGetMetadata = &LLVMValueMetadataEntriesGetMetadata;
  ENV->TangoLLVMAddGlobal = &LLVMAddGlobal;
  ENV->TangoLLVMAddGlobalInAddressSpace = &LLVMAddGlobalInAddressSpace;
  ENV->TangoLLVMGetNamedGlobal = &LLVMGetNamedGlobal;
  ENV->TangoLLVMGetFirstGlobal = &LLVMGetFirstGlobal;
  ENV->TangoLLVMGetLastGlobal = &LLVMGetLastGlobal;
  ENV->TangoLLVMGetNextGlobal = &LLVMGetNextGlobal;
  ENV->TangoLLVMGetPreviousGlobal = &LLVMGetPreviousGlobal;
  ENV->TangoLLVMDeleteGlobal = &LLVMDeleteGlobal;
  ENV->TangoLLVMGetInitializer = &LLVMGetInitializer;
  ENV->TangoLLVMSetInitializer = &LLVMSetInitializer;
  ENV->TangoLLVMIsThreadLocal = &LLVMIsThreadLocal;
  ENV->TangoLLVMSetThreadLocal = &LLVMSetThreadLocal;
  ENV->TangoLLVMIsGlobalConstant = &LLVMIsGlobalConstant;
  ENV->TangoLLVMSetGlobalConstant = &LLVMSetGlobalConstant;
  ENV->TangoLLVMGetThreadLocalMode = &LLVMGetThreadLocalMode;
  ENV->TangoLLVMSetThreadLocalMode = &LLVMSetThreadLocalMode;
  ENV->TangoLLVMIsExternallyInitialized = &LLVMIsExternallyInitialized;
  ENV->TangoLLVMSetExternallyInitialized = &LLVMSetExternallyInitialized;
  ENV->TangoLLVMAddAlias2 = &LLVMAddAlias2;
  ENV->TangoLLVMGetNamedGlobalAlias = &LLVMGetNamedGlobalAlias;
  ENV->TangoLLVMGetFirstGlobalAlias = &LLVMGetFirstGlobalAlias;
  ENV->TangoLLVMGetLastGlobalAlias = &LLVMGetLastGlobalAlias;
  ENV->TangoLLVMGetNextGlobalAlias = &LLVMGetNextGlobalAlias;
  ENV->TangoLLVMGetPreviousGlobalAlias = &LLVMGetPreviousGlobalAlias;
  ENV->TangoLLVMAliasGetAliasee = &LLVMAliasGetAliasee;
  ENV->TangoLLVMAliasSetAliasee = &LLVMAliasSetAliasee;
  ENV->TangoLLVMDeleteFunction = &LLVMDeleteFunction;
  ENV->TangoLLVMHasPersonalityFn = &LLVMHasPersonalityFn;
  ENV->TangoLLVMGetPersonalityFn = &LLVMGetPersonalityFn;
  ENV->TangoLLVMSetPersonalityFn = &LLVMSetPersonalityFn;
  ENV->TangoLLVMLookupIntrinsicID = &LLVMLookupIntrinsicID;
  ENV->TangoLLVMGetIntrinsicID = &LLVMGetIntrinsicID;
  ENV->TangoLLVMGetIntrinsicDeclaration = &LLVMGetIntrinsicDeclaration;
  ENV->TangoLLVMIntrinsicGetType = &LLVMIntrinsicGetType;
  ENV->TangoLLVMIntrinsicGetName = &LLVMIntrinsicGetName;
  ENV->TangoLLVMIntrinsicCopyOverloadedName = &LLVMIntrinsicCopyOverloadedName;
  ENV->TangoLLVMIntrinsicCopyOverloadedName2 = &LLVMIntrinsicCopyOverloadedName2;
  ENV->TangoLLVMIntrinsicIsOverloaded = &LLVMIntrinsicIsOverloaded;
  ENV->TangoLLVMGetFunctionCallConv = &LLVMGetFunctionCallConv;
  ENV->TangoLLVMSetFunctionCallConv = &LLVMSetFunctionCallConv;
  ENV->TangoLLVMGetGC = &LLVMGetGC;
  ENV->TangoLLVMSetGC = &LLVMSetGC;
  ENV->TangoLLVMGetPrefixData = &LLVMGetPrefixData;
  ENV->TangoLLVMHasPrefixData = &LLVMHasPrefixData;
  ENV->TangoLLVMSetPrefixData = &LLVMSetPrefixData;
  ENV->TangoLLVMGetPrologueData = &LLVMGetPrologueData;
  ENV->TangoLLVMHasPrologueData = &LLVMHasPrologueData;
  ENV->TangoLLVMSetPrologueData = &LLVMSetPrologueData;
  ENV->TangoLLVMAddAttributeAtIndex = &LLVMAddAttributeAtIndex;
  ENV->TangoLLVMGetAttributeCountAtIndex = &LLVMGetAttributeCountAtIndex;
  ENV->TangoLLVMGetAttributesAtIndex = &LLVMGetAttributesAtIndex;
  ENV->TangoLLVMGetEnumAttributeAtIndex = &LLVMGetEnumAttributeAtIndex;
  ENV->TangoLLVMGetStringAttributeAtIndex = &LLVMGetStringAttributeAtIndex;
  ENV->TangoLLVMRemoveEnumAttributeAtIndex = &LLVMRemoveEnumAttributeAtIndex;
  ENV->TangoLLVMRemoveStringAttributeAtIndex = &LLVMRemoveStringAttributeAtIndex;
  ENV->TangoLLVMAddTargetDependentFunctionAttr = &LLVMAddTargetDependentFunctionAttr;
  ENV->TangoLLVMCountParams = &LLVMCountParams;
  ENV->TangoLLVMGetParams = &LLVMGetParams;
  ENV->TangoLLVMGetParam = &LLVMGetParam;
  ENV->TangoLLVMGetParamParent = &LLVMGetParamParent;
  ENV->TangoLLVMGetFirstParam = &LLVMGetFirstParam;
  ENV->TangoLLVMGetLastParam = &LLVMGetLastParam;
  ENV->TangoLLVMGetNextParam = &LLVMGetNextParam;
  ENV->TangoLLVMGetPreviousParam = &LLVMGetPreviousParam;
  ENV->TangoLLVMSetParamAlignment = &LLVMSetParamAlignment;
  ENV->TangoLLVMAddGlobalIFunc = &LLVMAddGlobalIFunc;
  ENV->TangoLLVMGetNamedGlobalIFunc = &LLVMGetNamedGlobalIFunc;
  ENV->TangoLLVMGetFirstGlobalIFunc = &LLVMGetFirstGlobalIFunc;
  ENV->TangoLLVMGetLastGlobalIFunc = &LLVMGetLastGlobalIFunc;
  ENV->TangoLLVMGetNextGlobalIFunc = &LLVMGetNextGlobalIFunc;
  ENV->TangoLLVMGetPreviousGlobalIFunc = &LLVMGetPreviousGlobalIFunc;
  ENV->TangoLLVMGetGlobalIFuncResolver = &LLVMGetGlobalIFuncResolver;
  ENV->TangoLLVMSetGlobalIFuncResolver = &LLVMSetGlobalIFuncResolver;
  ENV->TangoLLVMEraseGlobalIFunc = &LLVMEraseGlobalIFunc;
  ENV->TangoLLVMRemoveGlobalIFunc = &LLVMRemoveGlobalIFunc;
  ENV->TangoLLVMMDStringInContext2 = &LLVMMDStringInContext2;
  ENV->TangoLLVMMDNodeInContext2 = &LLVMMDNodeInContext2;
  ENV->TangoLLVMMetadataAsValue = &LLVMMetadataAsValue;
  ENV->TangoLLVMValueAsMetadata = &LLVMValueAsMetadata;
  ENV->TangoLLVMGetMDString = &LLVMGetMDString;
  ENV->TangoLLVMGetMDNodeNumOperands = &LLVMGetMDNodeNumOperands;
  ENV->TangoLLVMGetMDNodeOperands = &LLVMGetMDNodeOperands;
  ENV->TangoLLVMReplaceMDNodeOperandWith = &LLVMReplaceMDNodeOperandWith;
  ENV->TangoLLVMMDStringInContext = &LLVMMDStringInContext;
  ENV->TangoLLVMMDString = &LLVMMDString;
  ENV->TangoLLVMMDNodeInContext = &LLVMMDNodeInContext;
  ENV->TangoLLVMMDNode = &LLVMMDNode;
  ENV->TangoLLVMCreateOperandBundle = &LLVMCreateOperandBundle;
  ENV->TangoLLVMDisposeOperandBundle = &LLVMDisposeOperandBundle;
  ENV->TangoLLVMGetOperandBundleTag = &LLVMGetOperandBundleTag;
  ENV->TangoLLVMGetNumOperandBundleArgs = &LLVMGetNumOperandBundleArgs;
  ENV->TangoLLVMGetOperandBundleArgAtIndex = &LLVMGetOperandBundleArgAtIndex;
  ENV->TangoLLVMBasicBlockAsValue = &LLVMBasicBlockAsValue;
  ENV->TangoLLVMValueIsBasicBlock = &LLVMValueIsBasicBlock;
  ENV->TangoLLVMValueAsBasicBlock = &LLVMValueAsBasicBlock;
  ENV->TangoLLVMGetBasicBlockName = &LLVMGetBasicBlockName;
  ENV->TangoLLVMGetBasicBlockParent = &LLVMGetBasicBlockParent;
  ENV->TangoLLVMGetBasicBlockTerminator = &LLVMGetBasicBlockTerminator;
  ENV->TangoLLVMCountBasicBlocks = &LLVMCountBasicBlocks;
  ENV->TangoLLVMGetBasicBlocks = &LLVMGetBasicBlocks;
  ENV->TangoLLVMGetFirstBasicBlock = &LLVMGetFirstBasicBlock;
  ENV->TangoLLVMGetLastBasicBlock = &LLVMGetLastBasicBlock;
  ENV->TangoLLVMGetNextBasicBlock = &LLVMGetNextBasicBlock;
  ENV->TangoLLVMGetPreviousBasicBlock = &LLVMGetPreviousBasicBlock;
  ENV->TangoLLVMGetEntryBasicBlock = &LLVMGetEntryBasicBlock;
  ENV->TangoLLVMInsertExistingBasicBlockAfterInsertBlock = &LLVMInsertExistingBasicBlockAfterInsertBlock;
  ENV->TangoLLVMAppendExistingBasicBlock = &LLVMAppendExistingBasicBlock;
  ENV->TangoLLVMCreateBasicBlockInContext = &LLVMCreateBasicBlockInContext;
  ENV->TangoLLVMAppendBasicBlockInContext = &LLVMAppendBasicBlockInContext;
  ENV->TangoLLVMAppendBasicBlock = &LLVMAppendBasicBlock;
  ENV->TangoLLVMInsertBasicBlockInContext = &LLVMInsertBasicBlockInContext;
  ENV->TangoLLVMInsertBasicBlock = &LLVMInsertBasicBlock;
  ENV->TangoLLVMDeleteBasicBlock = &LLVMDeleteBasicBlock;
  ENV->TangoLLVMRemoveBasicBlockFromParent = &LLVMRemoveBasicBlockFromParent;
  ENV->TangoLLVMMoveBasicBlockBefore = &LLVMMoveBasicBlockBefore;
  ENV->TangoLLVMMoveBasicBlockAfter = &LLVMMoveBasicBlockAfter;
  ENV->TangoLLVMGetFirstInstruction = &LLVMGetFirstInstruction;
  ENV->TangoLLVMGetLastInstruction = &LLVMGetLastInstruction;
  ENV->TangoLLVMHasMetadata = &LLVMHasMetadata;
  ENV->TangoLLVMGetMetadata = &LLVMGetMetadata;
  ENV->TangoLLVMSetMetadata = &LLVMSetMetadata;
  ENV->TangoLLVMInstructionGetAllMetadataOtherThanDebugLoc = &LLVMInstructionGetAllMetadataOtherThanDebugLoc;
  ENV->TangoLLVMGetInstructionParent = &LLVMGetInstructionParent;
  ENV->TangoLLVMGetNextInstruction = &LLVMGetNextInstruction;
  ENV->TangoLLVMGetPreviousInstruction = &LLVMGetPreviousInstruction;
  ENV->TangoLLVMInstructionRemoveFromParent = &LLVMInstructionRemoveFromParent;
  ENV->TangoLLVMInstructionEraseFromParent = &LLVMInstructionEraseFromParent;
  ENV->TangoLLVMDeleteInstruction = &LLVMDeleteInstruction;
  ENV->TangoLLVMGetInstructionOpcode = &LLVMGetInstructionOpcode;
  ENV->TangoLLVMGetICmpPredicate = &LLVMGetICmpPredicate;
  ENV->TangoLLVMGetFCmpPredicate = &LLVMGetFCmpPredicate;
  ENV->TangoLLVMInstructionClone = &LLVMInstructionClone;
  ENV->TangoLLVMIsATerminatorInst = &LLVMIsATerminatorInst;
  ENV->TangoLLVMGetNumArgOperands = &LLVMGetNumArgOperands;
  ENV->TangoLLVMSetInstructionCallConv = &LLVMSetInstructionCallConv;
  ENV->TangoLLVMGetInstructionCallConv = &LLVMGetInstructionCallConv;
  ENV->TangoLLVMSetInstrParamAlignment = &LLVMSetInstrParamAlignment;
  ENV->TangoLLVMAddCallSiteAttribute = &LLVMAddCallSiteAttribute;
  ENV->TangoLLVMGetCallSiteAttributeCount = &LLVMGetCallSiteAttributeCount;
  ENV->TangoLLVMGetCallSiteAttributes = &LLVMGetCallSiteAttributes;
  ENV->TangoLLVMGetCallSiteEnumAttribute = &LLVMGetCallSiteEnumAttribute;
  ENV->TangoLLVMGetCallSiteStringAttribute = &LLVMGetCallSiteStringAttribute;
  ENV->TangoLLVMRemoveCallSiteEnumAttribute = &LLVMRemoveCallSiteEnumAttribute;
  ENV->TangoLLVMRemoveCallSiteStringAttribute = &LLVMRemoveCallSiteStringAttribute;
  ENV->TangoLLVMGetCalledFunctionType = &LLVMGetCalledFunctionType;
  ENV->TangoLLVMGetCalledValue = &LLVMGetCalledValue;
  ENV->TangoLLVMGetNumOperandBundles = &LLVMGetNumOperandBundles;
  ENV->TangoLLVMGetOperandBundleAtIndex = &LLVMGetOperandBundleAtIndex;
  ENV->TangoLLVMIsTailCall = &LLVMIsTailCall;
  ENV->TangoLLVMSetTailCall = &LLVMSetTailCall;
  ENV->TangoLLVMGetTailCallKind = &LLVMGetTailCallKind;
  ENV->TangoLLVMSetTailCallKind = &LLVMSetTailCallKind;
  ENV->TangoLLVMGetNormalDest = &LLVMGetNormalDest;
  ENV->TangoLLVMGetUnwindDest = &LLVMGetUnwindDest;
  ENV->TangoLLVMSetNormalDest = &LLVMSetNormalDest;
  ENV->TangoLLVMSetUnwindDest = &LLVMSetUnwindDest;
  ENV->TangoLLVMGetCallBrDefaultDest = &LLVMGetCallBrDefaultDest;
  ENV->TangoLLVMGetCallBrNumIndirectDests = &LLVMGetCallBrNumIndirectDests;
  ENV->TangoLLVMGetCallBrIndirectDest = &LLVMGetCallBrIndirectDest;
  ENV->TangoLLVMGetNumSuccessors = &LLVMGetNumSuccessors;
  ENV->TangoLLVMGetSuccessor = &LLVMGetSuccessor;
  ENV->TangoLLVMSetSuccessor = &LLVMSetSuccessor;
  ENV->TangoLLVMIsConditional = &LLVMIsConditional;
  ENV->TangoLLVMGetCondition = &LLVMGetCondition;
  ENV->TangoLLVMSetCondition = &LLVMSetCondition;
  ENV->TangoLLVMGetSwitchDefaultDest = &LLVMGetSwitchDefaultDest;
  ENV->TangoLLVMGetAllocatedType = &LLVMGetAllocatedType;
  ENV->TangoLLVMIsInBounds = &LLVMIsInBounds;
  ENV->TangoLLVMSetIsInBounds = &LLVMSetIsInBounds;
  ENV->TangoLLVMGetGEPSourceElementType = &LLVMGetGEPSourceElementType;
  ENV->TangoLLVMGEPGetNoWrapFlags = &LLVMGEPGetNoWrapFlags;
  ENV->TangoLLVMGEPSetNoWrapFlags = &LLVMGEPSetNoWrapFlags;
  ENV->TangoLLVMAddIncoming = &LLVMAddIncoming;
  ENV->TangoLLVMCountIncoming = &LLVMCountIncoming;
  ENV->TangoLLVMGetIncomingValue = &LLVMGetIncomingValue;
  ENV->TangoLLVMGetIncomingBlock = &LLVMGetIncomingBlock;
  ENV->TangoLLVMGetNumIndices = &LLVMGetNumIndices;
  ENV->TangoLLVMGetIndices = &LLVMGetIndices;
  ENV->TangoLLVMCreateBuilderInContext = &LLVMCreateBuilderInContext;
  ENV->TangoLLVMCreateBuilder = &LLVMCreateBuilder;
  ENV->TangoLLVMPositionBuilder = &LLVMPositionBuilder;
  ENV->TangoLLVMPositionBuilderBeforeDbgRecords = &LLVMPositionBuilderBeforeDbgRecords;
  ENV->TangoLLVMPositionBuilderBefore = &LLVMPositionBuilderBefore;
  ENV->TangoLLVMPositionBuilderBeforeInstrAndDbgRecords = &LLVMPositionBuilderBeforeInstrAndDbgRecords;
  ENV->TangoLLVMPositionBuilderAtEnd = &LLVMPositionBuilderAtEnd;
  ENV->TangoLLVMGetInsertBlock = &LLVMGetInsertBlock;
  ENV->TangoLLVMClearInsertionPosition = &LLVMClearInsertionPosition;
  ENV->TangoLLVMInsertIntoBuilder = &LLVMInsertIntoBuilder;
  ENV->TangoLLVMInsertIntoBuilderWithName = &LLVMInsertIntoBuilderWithName;
  ENV->TangoLLVMDisposeBuilder = &LLVMDisposeBuilder;
  ENV->TangoLLVMGetCurrentDebugLocation2 = &LLVMGetCurrentDebugLocation2;
  ENV->TangoLLVMSetCurrentDebugLocation2 = &LLVMSetCurrentDebugLocation2;
  ENV->TangoLLVMSetInstDebugLocation = &LLVMSetInstDebugLocation;
  ENV->TangoLLVMAddMetadataToInst = &LLVMAddMetadataToInst;
  ENV->TangoLLVMBuilderGetDefaultFPMathTag = &LLVMBuilderGetDefaultFPMathTag;
  ENV->TangoLLVMBuilderSetDefaultFPMathTag = &LLVMBuilderSetDefaultFPMathTag;
  ENV->TangoLLVMSetCurrentDebugLocation = &LLVMSetCurrentDebugLocation;
  ENV->TangoLLVMGetCurrentDebugLocation = &LLVMGetCurrentDebugLocation;
  ENV->TangoLLVMBuildRetVoid = &LLVMBuildRetVoid;
  ENV->TangoLLVMBuildRet = &LLVMBuildRet;
  ENV->TangoLLVMBuildAggregateRet = &LLVMBuildAggregateRet;
  ENV->TangoLLVMBuildBr = &LLVMBuildBr;
  ENV->TangoLLVMBuildCondBr = &LLVMBuildCondBr;
  ENV->TangoLLVMBuildSwitch = &LLVMBuildSwitch;
  ENV->TangoLLVMBuildIndirectBr = &LLVMBuildIndirectBr;
  ENV->TangoLLVMBuildCallBr = &LLVMBuildCallBr;
  ENV->TangoLLVMBuildInvoke2 = &LLVMBuildInvoke2;
  ENV->TangoLLVMBuildInvokeWithOperandBundles = &LLVMBuildInvokeWithOperandBundles;
  ENV->TangoLLVMBuildUnreachable = &LLVMBuildUnreachable;
  ENV->TangoLLVMBuildResume = &LLVMBuildResume;
  ENV->TangoLLVMBuildLandingPad = &LLVMBuildLandingPad;
  ENV->TangoLLVMBuildCleanupRet = &LLVMBuildCleanupRet;
  ENV->TangoLLVMBuildCatchRet = &LLVMBuildCatchRet;
  ENV->TangoLLVMBuildCatchPad = &LLVMBuildCatchPad;
  ENV->TangoLLVMBuildCleanupPad = &LLVMBuildCleanupPad;
  ENV->TangoLLVMBuildCatchSwitch = &LLVMBuildCatchSwitch;
  ENV->TangoLLVMAddCase = &LLVMAddCase;
  ENV->TangoLLVMAddDestination = &LLVMAddDestination;
  ENV->TangoLLVMGetNumClauses = &LLVMGetNumClauses;
  ENV->TangoLLVMGetClause = &LLVMGetClause;
  ENV->TangoLLVMAddClause = &LLVMAddClause;
  ENV->TangoLLVMIsCleanup = &LLVMIsCleanup;
  ENV->TangoLLVMSetCleanup = &LLVMSetCleanup;
  ENV->TangoLLVMAddHandler = &LLVMAddHandler;
  ENV->TangoLLVMGetNumHandlers = &LLVMGetNumHandlers;
  ENV->TangoLLVMGetHandlers = &LLVMGetHandlers;
  ENV->TangoLLVMGetArgOperand = &LLVMGetArgOperand;
  ENV->TangoLLVMSetArgOperand = &LLVMSetArgOperand;
  ENV->TangoLLVMGetParentCatchSwitch = &LLVMGetParentCatchSwitch;
  ENV->TangoLLVMSetParentCatchSwitch = &LLVMSetParentCatchSwitch;
  ENV->TangoLLVMBuildAdd = &LLVMBuildAdd;
  ENV->TangoLLVMBuildNSWAdd = &LLVMBuildNSWAdd;
  ENV->TangoLLVMBuildNUWAdd = &LLVMBuildNUWAdd;
  ENV->TangoLLVMBuildFAdd = &LLVMBuildFAdd;
  ENV->TangoLLVMBuildSub = &LLVMBuildSub;
  ENV->TangoLLVMBuildNSWSub = &LLVMBuildNSWSub;
  ENV->TangoLLVMBuildNUWSub = &LLVMBuildNUWSub;
  ENV->TangoLLVMBuildFSub = &LLVMBuildFSub;
  ENV->TangoLLVMBuildMul = &LLVMBuildMul;
  ENV->TangoLLVMBuildNSWMul = &LLVMBuildNSWMul;
  ENV->TangoLLVMBuildNUWMul = &LLVMBuildNUWMul;
  ENV->TangoLLVMBuildFMul = &LLVMBuildFMul;
  ENV->TangoLLVMBuildUDiv = &LLVMBuildUDiv;
  ENV->TangoLLVMBuildExactUDiv = &LLVMBuildExactUDiv;
  ENV->TangoLLVMBuildSDiv = &LLVMBuildSDiv;
  ENV->TangoLLVMBuildExactSDiv = &LLVMBuildExactSDiv;
  ENV->TangoLLVMBuildFDiv = &LLVMBuildFDiv;
  ENV->TangoLLVMBuildURem = &LLVMBuildURem;
  ENV->TangoLLVMBuildSRem = &LLVMBuildSRem;
  ENV->TangoLLVMBuildFRem = &LLVMBuildFRem;
  ENV->TangoLLVMBuildShl = &LLVMBuildShl;
  ENV->TangoLLVMBuildLShr = &LLVMBuildLShr;
  ENV->TangoLLVMBuildAShr = &LLVMBuildAShr;
  ENV->TangoLLVMBuildAnd = &LLVMBuildAnd;
  ENV->TangoLLVMBuildOr = &LLVMBuildOr;
  ENV->TangoLLVMBuildXor = &LLVMBuildXor;
  ENV->TangoLLVMBuildBinOp = &LLVMBuildBinOp;
  ENV->TangoLLVMBuildNeg = &LLVMBuildNeg;
  ENV->TangoLLVMBuildNSWNeg = &LLVMBuildNSWNeg;
  ENV->TangoLLVMBuildNUWNeg = &LLVMBuildNUWNeg;
  ENV->TangoLLVMBuildFNeg = &LLVMBuildFNeg;
  ENV->TangoLLVMBuildNot = &LLVMBuildNot;
  ENV->TangoLLVMGetNUW = &LLVMGetNUW;
  ENV->TangoLLVMSetNUW = &LLVMSetNUW;
  ENV->TangoLLVMGetNSW = &LLVMGetNSW;
  ENV->TangoLLVMSetNSW = &LLVMSetNSW;
  ENV->TangoLLVMGetExact = &LLVMGetExact;
  ENV->TangoLLVMSetExact = &LLVMSetExact;
  ENV->TangoLLVMGetNNeg = &LLVMGetNNeg;
  ENV->TangoLLVMSetNNeg = &LLVMSetNNeg;
  ENV->TangoLLVMGetFastMathFlags = &LLVMGetFastMathFlags;
  ENV->TangoLLVMSetFastMathFlags = &LLVMSetFastMathFlags;
  ENV->TangoLLVMCanValueUseFastMathFlags = &LLVMCanValueUseFastMathFlags;
  ENV->TangoLLVMGetIsDisjoint = &LLVMGetIsDisjoint;
  ENV->TangoLLVMSetIsDisjoint = &LLVMSetIsDisjoint;
  ENV->TangoLLVMBuildMalloc = &LLVMBuildMalloc;
  ENV->TangoLLVMBuildArrayMalloc = &LLVMBuildArrayMalloc;
  ENV->TangoLLVMBuildMemSet = &LLVMBuildMemSet;
  ENV->TangoLLVMBuildMemCpy = &LLVMBuildMemCpy;
  ENV->TangoLLVMBuildMemMove = &LLVMBuildMemMove;
  ENV->TangoLLVMBuildAlloca = &LLVMBuildAlloca;
  ENV->TangoLLVMBuildArrayAlloca = &LLVMBuildArrayAlloca;
  ENV->TangoLLVMBuildFree = &LLVMBuildFree;
  ENV->TangoLLVMBuildLoad2 = &LLVMBuildLoad2;
  ENV->TangoLLVMBuildStore = &LLVMBuildStore;
  ENV->TangoLLVMBuildGEP2 = &LLVMBuildGEP2;
  ENV->TangoLLVMBuildInBoundsGEP2 = &LLVMBuildInBoundsGEP2;
  ENV->TangoLLVMBuildGEPWithNoWrapFlags = &LLVMBuildGEPWithNoWrapFlags;
  ENV->TangoLLVMBuildStructGEP2 = &LLVMBuildStructGEP2;
  ENV->TangoLLVMBuildGlobalString = &LLVMBuildGlobalString;
  ENV->TangoLLVMBuildGlobalStringPtr = &LLVMBuildGlobalStringPtr;
  ENV->TangoLLVMGetVolatile = &LLVMGetVolatile;
  ENV->TangoLLVMSetVolatile = &LLVMSetVolatile;
  ENV->TangoLLVMGetWeak = &LLVMGetWeak;
  ENV->TangoLLVMSetWeak = &LLVMSetWeak;
  ENV->TangoLLVMGetOrdering = &LLVMGetOrdering;
  ENV->TangoLLVMSetOrdering = &LLVMSetOrdering;
  ENV->TangoLLVMGetAtomicRMWBinOp = &LLVMGetAtomicRMWBinOp;
  ENV->TangoLLVMSetAtomicRMWBinOp = &LLVMSetAtomicRMWBinOp;
  ENV->TangoLLVMBuildTrunc = &LLVMBuildTrunc;
  ENV->TangoLLVMBuildZExt = &LLVMBuildZExt;
  ENV->TangoLLVMBuildSExt = &LLVMBuildSExt;
  ENV->TangoLLVMBuildFPToUI = &LLVMBuildFPToUI;
  ENV->TangoLLVMBuildFPToSI = &LLVMBuildFPToSI;
  ENV->TangoLLVMBuildUIToFP = &LLVMBuildUIToFP;
  ENV->TangoLLVMBuildSIToFP = &LLVMBuildSIToFP;
  ENV->TangoLLVMBuildFPTrunc = &LLVMBuildFPTrunc;
  ENV->TangoLLVMBuildFPExt = &LLVMBuildFPExt;
  ENV->TangoLLVMBuildPtrToInt = &LLVMBuildPtrToInt;
  ENV->TangoLLVMBuildIntToPtr = &LLVMBuildIntToPtr;
  ENV->TangoLLVMBuildBitCast = &LLVMBuildBitCast;
  ENV->TangoLLVMBuildAddrSpaceCast = &LLVMBuildAddrSpaceCast;
  ENV->TangoLLVMBuildZExtOrBitCast = &LLVMBuildZExtOrBitCast;
  ENV->TangoLLVMBuildSExtOrBitCast = &LLVMBuildSExtOrBitCast;
  ENV->TangoLLVMBuildTruncOrBitCast = &LLVMBuildTruncOrBitCast;
  ENV->TangoLLVMBuildCast = &LLVMBuildCast;
  ENV->TangoLLVMBuildPointerCast = &LLVMBuildPointerCast;
  ENV->TangoLLVMBuildIntCast2 = &LLVMBuildIntCast2;
  ENV->TangoLLVMBuildFPCast = &LLVMBuildFPCast;
  ENV->TangoLLVMBuildIntCast = &LLVMBuildIntCast;
  ENV->TangoLLVMGetCastOpcode = &LLVMGetCastOpcode;
  ENV->TangoLLVMBuildICmp = &LLVMBuildICmp;
  ENV->TangoLLVMBuildFCmp = &LLVMBuildFCmp;
  ENV->TangoLLVMBuildPhi = &LLVMBuildPhi;
  ENV->TangoLLVMBuildCall2 = &LLVMBuildCall2;
  ENV->TangoLLVMBuildCallWithOperandBundles = &LLVMBuildCallWithOperandBundles;
  ENV->TangoLLVMBuildSelect = &LLVMBuildSelect;
  ENV->TangoLLVMBuildVAArg = &LLVMBuildVAArg;
  ENV->TangoLLVMBuildExtractElement = &LLVMBuildExtractElement;
  ENV->TangoLLVMBuildInsertElement = &LLVMBuildInsertElement;
  ENV->TangoLLVMBuildShuffleVector = &LLVMBuildShuffleVector;
  ENV->TangoLLVMBuildExtractValue = &LLVMBuildExtractValue;
  ENV->TangoLLVMBuildInsertValue = &LLVMBuildInsertValue;
  ENV->TangoLLVMBuildFreeze = &LLVMBuildFreeze;
  ENV->TangoLLVMBuildIsNull = &LLVMBuildIsNull;
  ENV->TangoLLVMBuildIsNotNull = &LLVMBuildIsNotNull;
  ENV->TangoLLVMBuildPtrDiff2 = &LLVMBuildPtrDiff2;
  ENV->TangoLLVMBuildFence = &LLVMBuildFence;
  ENV->TangoLLVMBuildAtomicRMW = &LLVMBuildAtomicRMW;
  ENV->TangoLLVMBuildAtomicCmpXchg = &LLVMBuildAtomicCmpXchg;
  ENV->TangoLLVMGetNumMaskElements = &LLVMGetNumMaskElements;
  ENV->TangoLLVMGetUndefMaskElem = &LLVMGetUndefMaskElem;
  ENV->TangoLLVMGetMaskValue = &LLVMGetMaskValue;
  ENV->TangoLLVMIsAtomicSingleThread = &LLVMIsAtomicSingleThread;
  ENV->TangoLLVMSetAtomicSingleThread = &LLVMSetAtomicSingleThread;
  ENV->TangoLLVMGetCmpXchgSuccessOrdering = &LLVMGetCmpXchgSuccessOrdering;
  ENV->TangoLLVMSetCmpXchgSuccessOrdering = &LLVMSetCmpXchgSuccessOrdering;
  ENV->TangoLLVMGetCmpXchgFailureOrdering = &LLVMGetCmpXchgFailureOrdering;
  ENV->TangoLLVMSetCmpXchgFailureOrdering = &LLVMSetCmpXchgFailureOrdering;
  ENV->TangoLLVMCreateModuleProviderForExistingModule = &LLVMCreateModuleProviderForExistingModule;
  ENV->TangoLLVMDisposeModuleProvider = &LLVMDisposeModuleProvider;
  ENV->TangoLLVMCreateMemoryBufferWithContentsOfFile = &LLVMCreateMemoryBufferWithContentsOfFile;
  ENV->TangoLLVMCreateMemoryBufferWithSTDIN = &LLVMCreateMemoryBufferWithSTDIN;
  ENV->TangoLLVMCreateMemoryBufferWithMemoryRange = &LLVMCreateMemoryBufferWithMemoryRange;
  ENV->TangoLLVMCreateMemoryBufferWithMemoryRangeCopy = &LLVMCreateMemoryBufferWithMemoryRangeCopy;
  ENV->TangoLLVMGetBufferStart = &LLVMGetBufferStart;
  ENV->TangoLLVMGetBufferSize = &LLVMGetBufferSize;
  ENV->TangoLLVMDisposeMemoryBuffer = &LLVMDisposeMemoryBuffer;
  ENV->TangoLLVMCreatePassManager = &LLVMCreatePassManager;
  ENV->TangoLLVMCreateFunctionPassManagerForModule = &LLVMCreateFunctionPassManagerForModule;
  ENV->TangoLLVMCreateFunctionPassManager = &LLVMCreateFunctionPassManager;
  ENV->TangoLLVMRunPassManager = &LLVMRunPassManager;
  ENV->TangoLLVMInitializeFunctionPassManager = &LLVMInitializeFunctionPassManager;
  ENV->TangoLLVMRunFunctionPassManager = &LLVMRunFunctionPassManager;
  ENV->TangoLLVMFinalizeFunctionPassManager = &LLVMFinalizeFunctionPassManager;
  ENV->TangoLLVMDisposePassManager = &LLVMDisposePassManager;
  ENV->TangoLLVMStartMultithreaded = &LLVMStartMultithreaded;
  ENV->TangoLLVMStopMultithreaded = &LLVMStopMultithreaded;
  ENV->TangoLLVMIsMultithreaded = &LLVMIsMultithreaded;
  ENV->TangoLLVMWriteBitcodeToFile = &LLVMWriteBitcodeToFile;
  ENV->TangoLLVMWriteBitcodeToFD = &LLVMWriteBitcodeToFD;
  ENV->TangoLLVMWriteBitcodeToFileHandle = &LLVMWriteBitcodeToFileHandle;
  ENV->TangoLLVMWriteBitcodeToMemoryBuffer = &LLVMWriteBitcodeToMemoryBuffer;
  ENV->TangoLLVMParseBitcode = &LLVMParseBitcode;
  ENV->TangoLLVMParseBitcode2 = &LLVMParseBitcode2;
  ENV->TangoLLVMParseBitcodeInContext = &LLVMParseBitcodeInContext;
  ENV->TangoLLVMParseBitcodeInContext2 = &LLVMParseBitcodeInContext2;
  ENV->TangoLLVMGetBitcodeModuleInContext = &LLVMGetBitcodeModuleInContext;
  ENV->TangoLLVMGetBitcodeModuleInContext2 = &LLVMGetBitcodeModuleInContext2;
  ENV->TangoLLVMGetBitcodeModule = &LLVMGetBitcodeModule;
  ENV->TangoLLVMGetBitcodeModule2 = &LLVMGetBitcodeModule2;
  ENV->TangoLLVMVerifyModule = &LLVMVerifyModule;
  ENV->TangoLLVMVerifyFunction = &LLVMVerifyFunction;
  ENV->TangoLLVMViewFunctionCFG = &LLVMViewFunctionCFG;
  ENV->TangoLLVMViewFunctionCFGOnly = &LLVMViewFunctionCFGOnly;
  ENV->TangoLLVMDebugMetadataVersion = &LLVMDebugMetadataVersion;
  ENV->TangoLLVMGetModuleDebugMetadataVersion = &LLVMGetModuleDebugMetadataVersion;
  ENV->TangoLLVMStripModuleDebugInfo = &LLVMStripModuleDebugInfo;
  ENV->TangoLLVMCreateDIBuilderDisallowUnresolved = &LLVMCreateDIBuilderDisallowUnresolved;
  ENV->TangoLLVMCreateDIBuilder = &LLVMCreateDIBuilder;
  ENV->TangoLLVMDisposeDIBuilder = &LLVMDisposeDIBuilder;
  ENV->TangoLLVMDIBuilderFinalize = &LLVMDIBuilderFinalize;
  ENV->TangoLLVMDIBuilderFinalizeSubprogram = &LLVMDIBuilderFinalizeSubprogram;
  ENV->TangoLLVMDIBuilderCreateCompileUnit = &LLVMDIBuilderCreateCompileUnit;
  ENV->TangoLLVMDIBuilderCreateFile = &LLVMDIBuilderCreateFile;
  ENV->TangoLLVMDIBuilderCreateModule = &LLVMDIBuilderCreateModule;
  ENV->TangoLLVMDIBuilderCreateNameSpace = &LLVMDIBuilderCreateNameSpace;
  ENV->TangoLLVMDIBuilderCreateFunction = &LLVMDIBuilderCreateFunction;
  ENV->TangoLLVMDIBuilderCreateLexicalBlock = &LLVMDIBuilderCreateLexicalBlock;
  ENV->TangoLLVMDIBuilderCreateLexicalBlockFile = &LLVMDIBuilderCreateLexicalBlockFile;
  ENV->TangoLLVMDIBuilderCreateImportedModuleFromNamespace = &LLVMDIBuilderCreateImportedModuleFromNamespace;
  ENV->TangoLLVMDIBuilderCreateImportedModuleFromAlias = &LLVMDIBuilderCreateImportedModuleFromAlias;
  ENV->TangoLLVMDIBuilderCreateImportedModuleFromModule = &LLVMDIBuilderCreateImportedModuleFromModule;
  ENV->TangoLLVMDIBuilderCreateImportedDeclaration = &LLVMDIBuilderCreateImportedDeclaration;
  ENV->TangoLLVMDIBuilderCreateDebugLocation = &LLVMDIBuilderCreateDebugLocation;
  ENV->TangoLLVMDILocationGetLine = &LLVMDILocationGetLine;
  ENV->TangoLLVMDILocationGetColumn = &LLVMDILocationGetColumn;
  ENV->TangoLLVMDILocationGetScope = &LLVMDILocationGetScope;
  ENV->TangoLLVMDILocationGetInlinedAt = &LLVMDILocationGetInlinedAt;
  ENV->TangoLLVMDIScopeGetFile = &LLVMDIScopeGetFile;
  ENV->TangoLLVMDIFileGetDirectory = &LLVMDIFileGetDirectory;
  ENV->TangoLLVMDIFileGetFilename = &LLVMDIFileGetFilename;
  ENV->TangoLLVMDIFileGetSource = &LLVMDIFileGetSource;
  ENV->TangoLLVMDIBuilderGetOrCreateTypeArray = &LLVMDIBuilderGetOrCreateTypeArray;
  ENV->TangoLLVMDIBuilderCreateSubroutineType = &LLVMDIBuilderCreateSubroutineType;
  ENV->TangoLLVMDIBuilderCreateMacro = &LLVMDIBuilderCreateMacro;
  ENV->TangoLLVMDIBuilderCreateTempMacroFile = &LLVMDIBuilderCreateTempMacroFile;
  ENV->TangoLLVMDIBuilderCreateEnumerator = &LLVMDIBuilderCreateEnumerator;
  ENV->TangoLLVMDIBuilderCreateEnumerationType = &LLVMDIBuilderCreateEnumerationType;
  ENV->TangoLLVMDIBuilderCreateUnionType = &LLVMDIBuilderCreateUnionType;
  ENV->TangoLLVMDIBuilderCreateArrayType = &LLVMDIBuilderCreateArrayType;
  ENV->TangoLLVMDIBuilderCreateVectorType = &LLVMDIBuilderCreateVectorType;
  ENV->TangoLLVMDIBuilderCreateUnspecifiedType = &LLVMDIBuilderCreateUnspecifiedType;
  ENV->TangoLLVMDIBuilderCreateBasicType = &LLVMDIBuilderCreateBasicType;
  ENV->TangoLLVMDIBuilderCreatePointerType = &LLVMDIBuilderCreatePointerType;
  ENV->TangoLLVMDIBuilderCreateStructType = &LLVMDIBuilderCreateStructType;
  ENV->TangoLLVMDIBuilderCreateMemberType = &LLVMDIBuilderCreateMemberType;
  ENV->TangoLLVMDIBuilderCreateStaticMemberType = &LLVMDIBuilderCreateStaticMemberType;
  ENV->TangoLLVMDIBuilderCreateMemberPointerType = &LLVMDIBuilderCreateMemberPointerType;
  ENV->TangoLLVMDIBuilderCreateObjCIVar = &LLVMDIBuilderCreateObjCIVar;
  ENV->TangoLLVMDIBuilderCreateObjCProperty = &LLVMDIBuilderCreateObjCProperty;
  ENV->TangoLLVMDIBuilderCreateObjectPointerType = &LLVMDIBuilderCreateObjectPointerType;
  ENV->TangoLLVMDIBuilderCreateQualifiedType = &LLVMDIBuilderCreateQualifiedType;
  ENV->TangoLLVMDIBuilderCreateReferenceType = &LLVMDIBuilderCreateReferenceType;
  ENV->TangoLLVMDIBuilderCreateNullPtrType = &LLVMDIBuilderCreateNullPtrType;
  ENV->TangoLLVMDIBuilderCreateTypedef = &LLVMDIBuilderCreateTypedef;
  ENV->TangoLLVMDIBuilderCreateInheritance = &LLVMDIBuilderCreateInheritance;
  ENV->TangoLLVMDIBuilderCreateForwardDecl = &LLVMDIBuilderCreateForwardDecl;
  ENV->TangoLLVMDIBuilderCreateReplaceableCompositeType = &LLVMDIBuilderCreateReplaceableCompositeType;
  ENV->TangoLLVMDIBuilderCreateBitFieldMemberType = &LLVMDIBuilderCreateBitFieldMemberType;
  ENV->TangoLLVMDIBuilderCreateClassType = &LLVMDIBuilderCreateClassType;
  ENV->TangoLLVMDIBuilderCreateArtificialType = &LLVMDIBuilderCreateArtificialType;
  ENV->TangoLLVMDITypeGetName = &LLVMDITypeGetName;
  ENV->TangoLLVMDITypeGetSizeInBits = &LLVMDITypeGetSizeInBits;
  ENV->TangoLLVMDITypeGetOffsetInBits = &LLVMDITypeGetOffsetInBits;
  ENV->TangoLLVMDITypeGetAlignInBits = &LLVMDITypeGetAlignInBits;
  ENV->TangoLLVMDITypeGetLine = &LLVMDITypeGetLine;
  ENV->TangoLLVMDITypeGetFlags = &LLVMDITypeGetFlags;
  ENV->TangoLLVMDIBuilderGetOrCreateSubrange = &LLVMDIBuilderGetOrCreateSubrange;
  ENV->TangoLLVMDIBuilderGetOrCreateArray = &LLVMDIBuilderGetOrCreateArray;
  ENV->TangoLLVMDIBuilderCreateExpression = &LLVMDIBuilderCreateExpression;
  ENV->TangoLLVMDIBuilderCreateConstantValueExpression = &LLVMDIBuilderCreateConstantValueExpression;
  ENV->TangoLLVMDIBuilderCreateGlobalVariableExpression = &LLVMDIBuilderCreateGlobalVariableExpression;
  ENV->TangoLLVMGetDINodeTag = &LLVMGetDINodeTag;
  ENV->TangoLLVMDIGlobalVariableExpressionGetVariable = &LLVMDIGlobalVariableExpressionGetVariable;
  ENV->TangoLLVMDIGlobalVariableExpressionGetExpression = &LLVMDIGlobalVariableExpressionGetExpression;
  ENV->TangoLLVMDIVariableGetFile = &LLVMDIVariableGetFile;
  ENV->TangoLLVMDIVariableGetScope = &LLVMDIVariableGetScope;
  ENV->TangoLLVMDIVariableGetLine = &LLVMDIVariableGetLine;
  ENV->TangoLLVMTemporaryMDNode = &LLVMTemporaryMDNode;
  ENV->TangoLLVMDisposeTemporaryMDNode = &LLVMDisposeTemporaryMDNode;
  ENV->TangoLLVMMetadataReplaceAllUsesWith = &LLVMMetadataReplaceAllUsesWith;
  ENV->TangoLLVMDIBuilderCreateTempGlobalVariableFwdDecl = &LLVMDIBuilderCreateTempGlobalVariableFwdDecl;
  ENV->TangoLLVMDIBuilderInsertDeclareRecordBefore = &LLVMDIBuilderInsertDeclareRecordBefore;
  ENV->TangoLLVMDIBuilderInsertDeclareRecordAtEnd = &LLVMDIBuilderInsertDeclareRecordAtEnd;
  ENV->TangoLLVMDIBuilderInsertDbgValueRecordBefore = &LLVMDIBuilderInsertDbgValueRecordBefore;
  ENV->TangoLLVMDIBuilderInsertDbgValueRecordAtEnd = &LLVMDIBuilderInsertDbgValueRecordAtEnd;
  ENV->TangoLLVMDIBuilderCreateAutoVariable = &LLVMDIBuilderCreateAutoVariable;
  ENV->TangoLLVMDIBuilderCreateParameterVariable = &LLVMDIBuilderCreateParameterVariable;
  ENV->TangoLLVMGetSubprogram = &LLVMGetSubprogram;
  ENV->TangoLLVMSetSubprogram = &LLVMSetSubprogram;
  ENV->TangoLLVMDISubprogramGetLine = &LLVMDISubprogramGetLine;
  ENV->TangoLLVMInstructionGetDebugLoc = &LLVMInstructionGetDebugLoc;
  ENV->TangoLLVMInstructionSetDebugLoc = &LLVMInstructionSetDebugLoc;
  ENV->TangoLLVMGetMetadataKind = &LLVMGetMetadataKind;
  ENV->TangoLLVMLoadLibraryPermanently = &LLVMLoadLibraryPermanently;
  ENV->TangoLLVMParseCommandLineOptions = &LLVMParseCommandLineOptions;
  ENV->TangoLLVMSearchForAddressOfSymbol = &LLVMSearchForAddressOfSymbol;
  ENV->TangoLLVMAddSymbol = &LLVMAddSymbol;
  ENV->TangoLLVMGetModuleDataLayout = &LLVMGetModuleDataLayout;
  ENV->TangoLLVMSetModuleDataLayout = &LLVMSetModuleDataLayout;
  ENV->TangoLLVMCreateTargetData = &LLVMCreateTargetData;
  ENV->TangoLLVMDisposeTargetData = &LLVMDisposeTargetData;
  ENV->TangoLLVMAddTargetLibraryInfo = &LLVMAddTargetLibraryInfo;
  ENV->TangoLLVMCopyStringRepOfTargetData = &LLVMCopyStringRepOfTargetData;
  ENV->TangoLLVMByteOrder = &LLVMByteOrder;
  ENV->TangoLLVMPointerSize = &LLVMPointerSize;
  ENV->TangoLLVMPointerSizeForAS = &LLVMPointerSizeForAS;
  ENV->TangoLLVMIntPtrType = &LLVMIntPtrType;
  ENV->TangoLLVMIntPtrTypeForAS = &LLVMIntPtrTypeForAS;
  ENV->TangoLLVMIntPtrTypeInContext = &LLVMIntPtrTypeInContext;
  ENV->TangoLLVMIntPtrTypeForASInContext = &LLVMIntPtrTypeForASInContext;
  ENV->TangoLLVMSizeOfTypeInBits = &LLVMSizeOfTypeInBits;
  ENV->TangoLLVMStoreSizeOfType = &LLVMStoreSizeOfType;
  ENV->TangoLLVMABISizeOfType = &LLVMABISizeOfType;
  ENV->TangoLLVMABIAlignmentOfType = &LLVMABIAlignmentOfType;
  ENV->TangoLLVMCallFrameAlignmentOfType = &LLVMCallFrameAlignmentOfType;
  ENV->TangoLLVMPreferredAlignmentOfType = &LLVMPreferredAlignmentOfType;
  ENV->TangoLLVMPreferredAlignmentOfGlobal = &LLVMPreferredAlignmentOfGlobal;
  ENV->TangoLLVMElementAtOffset = &LLVMElementAtOffset;
  ENV->TangoLLVMOffsetOfElement = &LLVMOffsetOfElement;
  ENV->TangoLLVMGetFirstTarget = &LLVMGetFirstTarget;
  ENV->TangoLLVMGetNextTarget = &LLVMGetNextTarget;
  ENV->TangoLLVMGetTargetFromName = &LLVMGetTargetFromName;
  ENV->TangoLLVMGetTargetFromTriple = &LLVMGetTargetFromTriple;
  ENV->TangoLLVMGetTargetName = &LLVMGetTargetName;
  ENV->TangoLLVMGetTargetDescription = &LLVMGetTargetDescription;
  ENV->TangoLLVMTargetHasJIT = &LLVMTargetHasJIT;
  ENV->TangoLLVMTargetHasTargetMachine = &LLVMTargetHasTargetMachine;
  ENV->TangoLLVMTargetHasAsmBackend = &LLVMTargetHasAsmBackend;
  ENV->TangoLLVMCreateTargetMachineOptions = &LLVMCreateTargetMachineOptions;
  ENV->TangoLLVMDisposeTargetMachineOptions = &LLVMDisposeTargetMachineOptions;
  ENV->TangoLLVMTargetMachineOptionsSetCPU = &LLVMTargetMachineOptionsSetCPU;
  ENV->TangoLLVMTargetMachineOptionsSetFeatures = &LLVMTargetMachineOptionsSetFeatures;
  ENV->TangoLLVMTargetMachineOptionsSetABI = &LLVMTargetMachineOptionsSetABI;
  ENV->TangoLLVMTargetMachineOptionsSetCodeGenOptLevel = &LLVMTargetMachineOptionsSetCodeGenOptLevel;
  ENV->TangoLLVMTargetMachineOptionsSetRelocMode = &LLVMTargetMachineOptionsSetRelocMode;
  ENV->TangoLLVMTargetMachineOptionsSetCodeModel = &LLVMTargetMachineOptionsSetCodeModel;
  ENV->TangoLLVMCreateTargetMachineWithOptions = &LLVMCreateTargetMachineWithOptions;
  ENV->TangoLLVMCreateTargetMachine = &LLVMCreateTargetMachine;
  ENV->TangoLLVMDisposeTargetMachine = &LLVMDisposeTargetMachine;
  ENV->TangoLLVMGetTargetMachineTarget = &LLVMGetTargetMachineTarget;
  ENV->TangoLLVMGetTargetMachineTriple = &LLVMGetTargetMachineTriple;
  ENV->TangoLLVMGetTargetMachineCPU = &LLVMGetTargetMachineCPU;
  ENV->TangoLLVMGetTargetMachineFeatureString = &LLVMGetTargetMachineFeatureString;
  ENV->TangoLLVMCreateTargetDataLayout = &LLVMCreateTargetDataLayout;
  ENV->TangoLLVMSetTargetMachineAsmVerbosity = &LLVMSetTargetMachineAsmVerbosity;
  ENV->TangoLLVMSetTargetMachineFastISel = &LLVMSetTargetMachineFastISel;
  ENV->TangoLLVMSetTargetMachineGlobalISel = &LLVMSetTargetMachineGlobalISel;
  ENV->TangoLLVMSetTargetMachineGlobalISelAbort = &LLVMSetTargetMachineGlobalISelAbort;
  ENV->TangoLLVMSetTargetMachineMachineOutliner = &LLVMSetTargetMachineMachineOutliner;
  ENV->TangoLLVMTargetMachineEmitToFile = &LLVMTargetMachineEmitToFile;
  ENV->TangoLLVMTargetMachineEmitToMemoryBuffer = &LLVMTargetMachineEmitToMemoryBuffer;
  ENV->TangoLLVMGetDefaultTargetTriple = &LLVMGetDefaultTargetTriple;
  ENV->TangoLLVMNormalizeTargetTriple = &LLVMNormalizeTargetTriple;
  ENV->TangoLLVMGetHostCPUName = &LLVMGetHostCPUName;
  ENV->TangoLLVMGetHostCPUFeatures = &LLVMGetHostCPUFeatures;
  ENV->TangoLLVMRemoveFunctionBody = &LLVMRemoveFunctionBody;
  ENV->TangoLLVMBuildIntrinsic = &LLVMBuildIntrinsic;
  ENV->TangoLLVMAddAnalysisPasses = &LLVMAddAnalysisPasses;
  return ENV;
}
