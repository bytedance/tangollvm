/*===-- llvm-c/TangoLLVM.h - TangoLLVM C Interface ----------------*- C -*-===*\
|*                                                                            *|
|* Part of the LLVM Project, under the Apache License v2.0 with LLVM          *|
|* Exceptions.                                                                *|
|* See https://llvm.org/LICENSE.txt for license information.                  *|
|* SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception                    *|
|*                                                                            *|
|* Copyright (c) 2025 ByteDance Ltd. and/or its affiliates.                    *|
|* This file has been modified by ByteDance Ltd. and/or its affiliates on 2025-05-19. *|
|*                                                                            *|
|* Portions adapted from go-llvm IRBindings.h and backports.h, corresponding   *|
|* to IRBindings.cpp and backports.cpp. The modified file retains              *|
|* Apache-2.0 WITH LLVM-exception; the complete original license is in          *|
|* tango/overlay/src/cmd/internal/llvm/LICENSE.txt.                             *|
|*                                                                            *|
|* This header declares the C interface for TangoLLVM.                        *|
\*===----------------------------------------------------------------------===*/

#ifndef LLVM_C_TANGOLLVM_H
#define LLVM_C_TANGOLLVM_H

#include "llvm-c/Core.h"
#include "llvm-c/DebugInfo.h"
#include "llvm-c/Analysis.h"
#include "llvm-c/BitReader.h"
#include "llvm-c/BitWriter.h"
#include "llvm-c/Support.h"
#include "llvm-c/Target.h"
#include "llvm-c/TargetMachine.h"

LLVM_C_EXTERN_C_BEGIN

struct LLVMDebugLocMetadata{
    unsigned Line;
    unsigned Col;
    LLVMMetadataRef Scope;
    LLVMMetadataRef InlinedAt;
};

void LLVMInitializeAll(void);
LLVMMetadataRef LLVMConstantAsMetadata(LLVMValueRef Val);
LLVMValueRef LLVMConstAggregateZero(LLVMTypeRef t);
LLVMTypeRef LLVMGetFunctionType(LLVMValueRef function);

LLVMMetadataRef LLVMMDString2(LLVMContextRef C, const char *Str, unsigned SLen);
LLVMMetadataRef LLVMMDNode2(LLVMContextRef C, LLVMMetadataRef *MDs,
                            unsigned Count);

void LLVMGlobalObjectAddMetadata(LLVMValueRef objValue, unsigned KindID, LLVMMetadataRef md);

void LLVMAddNamedMetadataOperand2(LLVMModuleRef M, const char *name,
                                  LLVMMetadataRef Val);
void LLVMSetMetadata2(LLVMValueRef Inst, unsigned KindID, LLVMMetadataRef MD);

void LLVMGoSetCurrentDebugLocation(LLVMBuilderRef Bref, unsigned Line,
                                  unsigned Col, LLVMMetadataRef Scope,
                                  LLVMMetadataRef InlinedAt);

struct LLVMDebugLocMetadata LLVMGoGetCurrentDebugLocation(LLVMBuilderRef Bref);

LLVMValueRef LLVMGoGetInlineAsm(LLVMTypeRef Ty, char *AsmString,
                                size_t AsmStringSize, char *Constraints,
                                size_t ConstraintsSize, LLVMBool HasSideEffects,
                                LLVMBool IsAlignStack,
                                LLVMInlineAsmDialect Dialect, LLVMBool CanThrow);

LLVMValueRef LLVMBuildMemCpyInline(LLVMBuilderRef B,
                             LLVMValueRef Dst, unsigned DstAlign,
                             LLVMValueRef Src, unsigned SrcAlign,
                             LLVMValueRef Size);

LLVMValueRef LLVMBuildGEPByOffset(LLVMBuilderRef B, LLVMValueRef Base, LLVMTypeRef ElemType, uint64_t Offset);

void LLVMAddCallSiteFnAttribute(LLVMValueRef C, LLVMAttributeRef A);



typedef struct {

  // from this file
void (*TangoLLVMInitializeAll)(void);
LLVMMetadataRef (*TangoLLVMConstantAsMetadata)(LLVMValueRef Val);
LLVMValueRef (*TangoLLVMConstAggregateZero)(LLVMTypeRef t);
LLVMTypeRef (*TangoLLVMGetFunctionType)(LLVMValueRef function);

LLVMMetadataRef (*TangoLLVMMDString2)(LLVMContextRef C, const char *Str, unsigned SLen);
LLVMMetadataRef (*TangoLLVMMDNode2)(LLVMContextRef C, LLVMMetadataRef *MDs,
                            unsigned Count);

void (*TangoLLVMGlobalObjectAddMetadata)(LLVMValueRef objValue, unsigned KindID, LLVMMetadataRef md);

void (*TangoLLVMAddNamedMetadataOperand2)(LLVMModuleRef M, const char *name,
                                  LLVMMetadataRef Val);
void (*TangoLLVMSetMetadata2)(LLVMValueRef Inst, unsigned KindID, LLVMMetadataRef MD);

void (*TangoLLVMGoSetCurrentDebugLocation)(LLVMBuilderRef Bref, unsigned Line,
                                  unsigned Col, LLVMMetadataRef Scope,
                                  LLVMMetadataRef InlinedAt);
struct LLVMDebugLocMetadata (*TangoLLVMGoGetCurrentDebugLocation)(LLVMBuilderRef Bref);

LLVMValueRef (*TangoLLVMGoGetInlineAsm)(LLVMTypeRef Ty, char *AsmString,
                                size_t AsmStringSize, char *Constraints,
                                size_t ConstraintsSize, LLVMBool HasSideEffects,
                                LLVMBool IsAlignStack,
                                LLVMInlineAsmDialect Dialect, LLVMBool CanThrow);

LLVMValueRef (*TangoLLVMBuildMemCpyInline)(LLVMBuilderRef B,
                             LLVMValueRef Dst, unsigned DstAlign,
                             LLVMValueRef Src, unsigned SrcAlign,
                             LLVMValueRef Size);

LLVMValueRef (*TangoLLVMBuildGEPByOffset)(LLVMBuilderRef B, LLVMValueRef Base, LLVMTypeRef ElemType, uint64_t Offset);

void (*TangoLLVMAddCallSiteFnAttribute)(LLVMValueRef C, LLVMAttributeRef A);


  // from llvm-c/Core.h


  void (*TangoLLVMShutdown)(void);
  void (*TangoLLVMGetVersion)(unsigned *Major, unsigned *Minor, unsigned *Patch);
  char *(*TangoLLVMCreateMessage)(const char *Message);
  void (*TangoLLVMDisposeMessage)(char *Message);

  LLVMContextRef (*TangoLLVMContextCreate)(void);

  LLVMContextRef (*TangoLLVMGetGlobalContext)(void);

  void (*TangoLLVMContextSetDiagnosticHandler)(LLVMContextRef C,
      LLVMDiagnosticHandler Handler,
      void *DiagnosticContext);

  LLVMDiagnosticHandler (*TangoLLVMContextGetDiagnosticHandler)(LLVMContextRef C);

  void* (*TangoLLVMContextGetDiagnosticContext)(LLVMContextRef C);

  void (*TangoLLVMContextSetYieldCallback)(LLVMContextRef C, LLVMYieldCallback Callback,
      void *OpaqueHandle);

  LLVMBool (*TangoLLVMContextShouldDiscardValueNames)(LLVMContextRef C);

  void (*TangoLLVMContextSetDiscardValueNames)(LLVMContextRef C, LLVMBool Discard);

  void (*TangoLLVMContextDispose)(LLVMContextRef C);

  char* (*TangoLLVMGetDiagInfoDescription)(LLVMDiagnosticInfoRef DI);

  LLVMDiagnosticSeverity (*TangoLLVMGetDiagInfoSeverity)(LLVMDiagnosticInfoRef DI);

  unsigned (*TangoLLVMGetMDKindIDInContext)(LLVMContextRef C, const char *Name,
      unsigned SLen);
  unsigned (*TangoLLVMGetMDKindID)(const char *Name, unsigned SLen);

  unsigned (*TangoLLVMGetEnumAttributeKindForName)(const char *Name, size_t SLen);
  unsigned (*TangoLLVMGetLastEnumAttributeKind)(void);

  LLVMAttributeRef (*TangoLLVMCreateEnumAttribute)(LLVMContextRef C, unsigned KindID,
      uint64_t Val);

  unsigned (*TangoLLVMGetEnumAttributeKind)(LLVMAttributeRef A);

  uint64_t (*TangoLLVMGetEnumAttributeValue)(LLVMAttributeRef A);

  LLVMAttributeRef (*TangoLLVMCreateTypeAttribute)(LLVMContextRef C, unsigned KindID,
      LLVMTypeRef type_ref);

  LLVMTypeRef (*TangoLLVMGetTypeAttributeValue)(LLVMAttributeRef A);

  LLVMAttributeRef (*TangoLLVMCreateConstantRangeAttribute)(LLVMContextRef C,
      unsigned KindID,
      unsigned NumBits,
      const uint64_t LowerWords[],
      const uint64_t UpperWords[]);

  LLVMAttributeRef (*TangoLLVMCreateStringAttribute)(LLVMContextRef C,
      const char *K, unsigned KLength,
      const char *V, unsigned VLength);

  const char *(*TangoLLVMGetStringAttributeKind)(LLVMAttributeRef A, unsigned *Length);

  const char *(*TangoLLVMGetStringAttributeValue)(LLVMAttributeRef A, unsigned *Length);

  LLVMBool (*TangoLLVMIsEnumAttribute)(LLVMAttributeRef A);
  LLVMBool (*TangoLLVMIsStringAttribute)(LLVMAttributeRef A);
  LLVMBool (*TangoLLVMIsTypeAttribute)(LLVMAttributeRef A);

  LLVMTypeRef (*TangoLLVMGetTypeByName2)(LLVMContextRef C, const char *Name);

  LLVMModuleRef (*TangoLLVMModuleCreateWithName)(const char *ModuleID);

  LLVMModuleRef (*TangoLLVMModuleCreateWithNameInContext)(const char *ModuleID,
      LLVMContextRef C);

  LLVMModuleRef (*TangoLLVMCloneModule)(LLVMModuleRef M);

  void (*TangoLLVMDisposeModule)(LLVMModuleRef M);

  LLVMBool (*TangoLLVMIsNewDbgInfoFormat)(LLVMModuleRef M);

  void (*TangoLLVMSetIsNewDbgInfoFormat)(LLVMModuleRef M, LLVMBool UseNewFormat);

  const char *(*TangoLLVMGetModuleIdentifier)(LLVMModuleRef M, size_t *Len);

  void (*TangoLLVMSetModuleIdentifier)(LLVMModuleRef M, const char *Ident, size_t Len);

  const char *(*TangoLLVMGetSourceFileName)(LLVMModuleRef M, size_t *Len);

  void (*TangoLLVMSetSourceFileName)(LLVMModuleRef M, const char *Name, size_t Len);

  const char *(*TangoLLVMGetDataLayoutStr)(LLVMModuleRef M);
  const char *(*TangoLLVMGetDataLayout)(LLVMModuleRef M);

  void (*TangoLLVMSetDataLayout)(LLVMModuleRef M, const char *DataLayoutStr);

  const char *(*TangoLLVMGetTarget)(LLVMModuleRef M);

  void (*TangoLLVMSetTarget)(LLVMModuleRef M, const char *Triple);

  LLVMModuleFlagEntry *(*TangoLLVMCopyModuleFlagsMetadata)(LLVMModuleRef M, size_t *Len);

  void (*TangoLLVMDisposeModuleFlagsMetadata)(LLVMModuleFlagEntry *Entries);

  LLVMModuleFlagBehavior
    (*TangoLLVMModuleFlagEntriesGetFlagBehavior)(LLVMModuleFlagEntry *Entries,
        unsigned Index);

  const char *(*TangoLLVMModuleFlagEntriesGetKey)(LLVMModuleFlagEntry *Entries,
      unsigned Index, size_t *Len);

  LLVMMetadataRef (*TangoLLVMModuleFlagEntriesGetMetadata)(LLVMModuleFlagEntry *Entries,
      unsigned Index);

  LLVMMetadataRef (*TangoLLVMGetModuleFlag)(LLVMModuleRef M,
      const char *Key, size_t KeyLen);

  void (*TangoLLVMAddModuleFlag)(LLVMModuleRef M, LLVMModuleFlagBehavior Behavior,
      const char *Key, size_t KeyLen,
      LLVMMetadataRef Val);

  void (*TangoLLVMDumpModule)(LLVMModuleRef M);

  LLVMBool (*TangoLLVMPrintModuleToFile)(LLVMModuleRef M, const char *Filename,
      char **ErrorMessage);

  char *(*TangoLLVMPrintModuleToString)(LLVMModuleRef M);

  const char *(*TangoLLVMGetModuleInlineAsm)(LLVMModuleRef M, size_t *Len);

  void (*TangoLLVMSetModuleInlineAsm2)(LLVMModuleRef M, const char *Asm, size_t Len);

  void (*TangoLLVMAppendModuleInlineAsm)(LLVMModuleRef M, const char *Asm, size_t Len);

  LLVMValueRef (*TangoLLVMGetInlineAsm)(LLVMTypeRef Ty, const char *AsmString,
      size_t AsmStringSize, const char *Constraints,
      size_t ConstraintsSize, LLVMBool HasSideEffects,
      LLVMBool IsAlignStack,
      LLVMInlineAsmDialect Dialect, LLVMBool CanThrow);

  const char *(*TangoLLVMGetInlineAsmAsmString)(LLVMValueRef InlineAsmVal, size_t *Len);

  const char *(*TangoLLVMGetInlineAsmConstraintString)(LLVMValueRef InlineAsmVal,
      size_t *Len);

  LLVMInlineAsmDialect (*TangoLLVMGetInlineAsmDialect)(LLVMValueRef InlineAsmVal);

  LLVMTypeRef (*TangoLLVMGetInlineAsmFunctionType)(LLVMValueRef InlineAsmVal);

  LLVMBool (*TangoLLVMGetInlineAsmHasSideEffects)(LLVMValueRef InlineAsmVal);

  LLVMBool (*TangoLLVMGetInlineAsmNeedsAlignedStack)(LLVMValueRef InlineAsmVal);

  LLVMBool (*TangoLLVMGetInlineAsmCanUnwind)(LLVMValueRef InlineAsmVal);

  LLVMContextRef (*TangoLLVMGetModuleContext)(LLVMModuleRef M);

  LLVMTypeRef (*TangoLLVMGetTypeByName)(LLVMModuleRef M, const char *Name);

  LLVMNamedMDNodeRef (*TangoLLVMGetFirstNamedMetadata)(LLVMModuleRef M);

  LLVMNamedMDNodeRef (*TangoLLVMGetLastNamedMetadata)(LLVMModuleRef M);

  LLVMNamedMDNodeRef (*TangoLLVMGetNextNamedMetadata)(LLVMNamedMDNodeRef NamedMDNode);

  LLVMNamedMDNodeRef (*TangoLLVMGetPreviousNamedMetadata)(LLVMNamedMDNodeRef NamedMDNode);

  LLVMNamedMDNodeRef (*TangoLLVMGetNamedMetadata)(LLVMModuleRef M,
      const char *Name, size_t NameLen);

  LLVMNamedMDNodeRef (*TangoLLVMGetOrInsertNamedMetadata)(LLVMModuleRef M,
      const char *Name,
      size_t NameLen);

  const char *(*TangoLLVMGetNamedMetadataName)(LLVMNamedMDNodeRef NamedMD,
      size_t *NameLen);

  unsigned (*TangoLLVMGetNamedMetadataNumOperands)(LLVMModuleRef M, const char *Name);

  void (*TangoLLVMGetNamedMetadataOperands)(LLVMModuleRef M, const char *Name,
      LLVMValueRef *Dest);

  void (*TangoLLVMAddNamedMetadataOperand)(LLVMModuleRef M, const char *Name,
      LLVMValueRef Val);

  const char *(*TangoLLVMGetDebugLocDirectory)(LLVMValueRef Val, unsigned *Length);

  const char *(*TangoLLVMGetDebugLocFilename)(LLVMValueRef Val, unsigned *Length);

  unsigned (*TangoLLVMGetDebugLocLine)(LLVMValueRef Val);

  unsigned (*TangoLLVMGetDebugLocColumn)(LLVMValueRef Val);

  LLVMValueRef (*TangoLLVMAddFunction)(LLVMModuleRef M, const char *Name,
      LLVMTypeRef FunctionTy);

  LLVMValueRef (*TangoLLVMGetNamedFunction)(LLVMModuleRef M, const char *Name);

  LLVMValueRef (*TangoLLVMGetFirstFunction)(LLVMModuleRef M);

  LLVMValueRef (*TangoLLVMGetLastFunction)(LLVMModuleRef M);

  LLVMValueRef (*TangoLLVMGetNextFunction)(LLVMValueRef Fn);

  LLVMValueRef (*TangoLLVMGetPreviousFunction)(LLVMValueRef Fn);

  void (*TangoLLVMSetModuleInlineAsm)(LLVMModuleRef M, const char *Asm);

  LLVMTypeKind (*TangoLLVMGetTypeKind)(LLVMTypeRef Ty);

  LLVMBool (*TangoLLVMTypeIsSized)(LLVMTypeRef Ty);

  LLVMContextRef (*TangoLLVMGetTypeContext)(LLVMTypeRef Ty);

  void (*TangoLLVMDumpType)(LLVMTypeRef Val);

  char *(*TangoLLVMPrintTypeToString)(LLVMTypeRef Val);

  LLVMTypeRef (*TangoLLVMInt1TypeInContext)(LLVMContextRef C);
  LLVMTypeRef (*TangoLLVMInt8TypeInContext)(LLVMContextRef C);
  LLVMTypeRef (*TangoLLVMInt16TypeInContext)(LLVMContextRef C);
  LLVMTypeRef (*TangoLLVMInt32TypeInContext)(LLVMContextRef C);
  LLVMTypeRef (*TangoLLVMInt64TypeInContext)(LLVMContextRef C);
  LLVMTypeRef (*TangoLLVMInt128TypeInContext)(LLVMContextRef C);
  LLVMTypeRef (*TangoLLVMIntTypeInContext)(LLVMContextRef C, unsigned NumBits);

  LLVMTypeRef (*TangoLLVMInt1Type)(void);
  LLVMTypeRef (*TangoLLVMInt8Type)(void);
  LLVMTypeRef (*TangoLLVMInt16Type)(void);
  LLVMTypeRef (*TangoLLVMInt32Type)(void);
  LLVMTypeRef (*TangoLLVMInt64Type)(void);
  LLVMTypeRef (*TangoLLVMInt128Type)(void);
  LLVMTypeRef (*TangoLLVMIntType)(unsigned NumBits);
  unsigned (*TangoLLVMGetIntTypeWidth)(LLVMTypeRef IntegerTy);

  LLVMTypeRef (*TangoLLVMHalfTypeInContext)(LLVMContextRef C);

  LLVMTypeRef (*TangoLLVMBFloatTypeInContext)(LLVMContextRef C);

  LLVMTypeRef (*TangoLLVMFloatTypeInContext)(LLVMContextRef C);

  LLVMTypeRef (*TangoLLVMDoubleTypeInContext)(LLVMContextRef C);

  LLVMTypeRef (*TangoLLVMX86FP80TypeInContext)(LLVMContextRef C);

  LLVMTypeRef (*TangoLLVMFP128TypeInContext)(LLVMContextRef C);

  LLVMTypeRef (*TangoLLVMPPCFP128TypeInContext)(LLVMContextRef C);

  LLVMTypeRef (*TangoLLVMHalfType)(void);
  LLVMTypeRef (*TangoLLVMBFloatType)(void);
  LLVMTypeRef (*TangoLLVMFloatType)(void);
  LLVMTypeRef (*TangoLLVMDoubleType)(void);
  LLVMTypeRef (*TangoLLVMX86FP80Type)(void);
  LLVMTypeRef (*TangoLLVMFP128Type)(void);
  LLVMTypeRef (*TangoLLVMPPCFP128Type)(void);

  LLVMTypeRef (*TangoLLVMFunctionType)(LLVMTypeRef ReturnType,
      LLVMTypeRef *ParamTypes, unsigned ParamCount,
      LLVMBool IsVarArg);

  LLVMBool (*TangoLLVMIsFunctionVarArg)(LLVMTypeRef FunctionTy);

  LLVMTypeRef (*TangoLLVMGetReturnType)(LLVMTypeRef FunctionTy);

  unsigned (*TangoLLVMCountParamTypes)(LLVMTypeRef FunctionTy);

  void (*TangoLLVMGetParamTypes)(LLVMTypeRef FunctionTy, LLVMTypeRef *Dest);

  LLVMTypeRef (*TangoLLVMStructTypeInContext)(LLVMContextRef C, LLVMTypeRef *ElementTypes,
      unsigned ElementCount, LLVMBool Packed);

  LLVMTypeRef (*TangoLLVMStructType)(LLVMTypeRef *ElementTypes, unsigned ElementCount,
      LLVMBool Packed);

  LLVMTypeRef (*TangoLLVMStructCreateNamed)(LLVMContextRef C, const char *Name);

  const char *(*TangoLLVMGetStructName)(LLVMTypeRef Ty);

  void (*TangoLLVMStructSetBody)(LLVMTypeRef StructTy, LLVMTypeRef *ElementTypes,
      unsigned ElementCount, LLVMBool Packed);

  unsigned (*TangoLLVMCountStructElementTypes)(LLVMTypeRef StructTy);

  void (*TangoLLVMGetStructElementTypes)(LLVMTypeRef StructTy, LLVMTypeRef *Dest);

  LLVMTypeRef (*TangoLLVMStructGetTypeAtIndex)(LLVMTypeRef StructTy, unsigned i);

  LLVMBool (*TangoLLVMIsPackedStruct)(LLVMTypeRef StructTy);

  LLVMBool (*TangoLLVMIsOpaqueStruct)(LLVMTypeRef StructTy);

  LLVMBool (*TangoLLVMIsLiteralStruct)(LLVMTypeRef StructTy);

  LLVMTypeRef (*TangoLLVMGetElementType)(LLVMTypeRef Ty);

  void (*TangoLLVMGetSubtypes)(LLVMTypeRef Tp, LLVMTypeRef *Arr);

  unsigned (*TangoLLVMGetNumContainedTypes)(LLVMTypeRef Tp);

  LLVMTypeRef (*TangoLLVMArrayType)(LLVMTypeRef ElementType, unsigned ElementCount);

  LLVMTypeRef (*TangoLLVMArrayType2)(LLVMTypeRef ElementType, uint64_t ElementCount);

  unsigned (*TangoLLVMGetArrayLength)(LLVMTypeRef ArrayTy);

  uint64_t (*TangoLLVMGetArrayLength2)(LLVMTypeRef ArrayTy);

  LLVMTypeRef (*TangoLLVMPointerType)(LLVMTypeRef ElementType, unsigned AddressSpace);

  LLVMBool (*TangoLLVMPointerTypeIsOpaque)(LLVMTypeRef Ty);

  LLVMTypeRef (*TangoLLVMPointerTypeInContext)(LLVMContextRef C, unsigned AddressSpace);

  unsigned (*TangoLLVMGetPointerAddressSpace)(LLVMTypeRef PointerTy);

  LLVMTypeRef (*TangoLLVMVectorType)(LLVMTypeRef ElementType, unsigned ElementCount);

  LLVMTypeRef (*TangoLLVMScalableVectorType)(LLVMTypeRef ElementType,
      unsigned ElementCount);

  unsigned (*TangoLLVMGetVectorSize)(LLVMTypeRef VectorTy);

  LLVMValueRef (*TangoLLVMGetConstantPtrAuthPointer)(LLVMValueRef PtrAuth);

  LLVMValueRef (*TangoLLVMGetConstantPtrAuthKey)(LLVMValueRef PtrAuth);

  LLVMValueRef (*TangoLLVMGetConstantPtrAuthDiscriminator)(LLVMValueRef PtrAuth);

  LLVMValueRef (*TangoLLVMGetConstantPtrAuthAddrDiscriminator)(LLVMValueRef PtrAuth);

  LLVMTypeRef (*TangoLLVMVoidTypeInContext)(LLVMContextRef C);

  LLVMTypeRef (*TangoLLVMLabelTypeInContext)(LLVMContextRef C);

  LLVMTypeRef (*TangoLLVMX86MMXTypeInContext)(LLVMContextRef C);

  LLVMTypeRef (*TangoLLVMX86AMXTypeInContext)(LLVMContextRef C);

  LLVMTypeRef (*TangoLLVMTokenTypeInContext)(LLVMContextRef C);

  LLVMTypeRef (*TangoLLVMMetadataTypeInContext)(LLVMContextRef C);

  LLVMTypeRef (*TangoLLVMVoidType)(void);
  LLVMTypeRef (*TangoLLVMLabelType)(void);
  LLVMTypeRef (*TangoLLVMX86MMXType)(void);
  LLVMTypeRef (*TangoLLVMX86AMXType)(void);

  LLVMTypeRef (*TangoLLVMTargetExtTypeInContext)(LLVMContextRef C, const char *Name,
      LLVMTypeRef *TypeParams,
      unsigned TypeParamCount,
      unsigned *IntParams,
      unsigned IntParamCount);

  const char *(*TangoLLVMGetTargetExtTypeName)(LLVMTypeRef TargetExtTy);

  unsigned (*TangoLLVMGetTargetExtTypeNumTypeParams)(LLVMTypeRef TargetExtTy);

  LLVMTypeRef (*TangoLLVMGetTargetExtTypeTypeParam)(LLVMTypeRef TargetExtTy,
      unsigned Idx);

  unsigned (*TangoLLVMGetTargetExtTypeNumIntParams)(LLVMTypeRef TargetExtTy);

  unsigned (*TangoLLVMGetTargetExtTypeIntParam)(LLVMTypeRef TargetExtTy, unsigned Idx);

  LLVMTypeRef (*TangoLLVMTypeOf)(LLVMValueRef Val);

  LLVMValueKind (*TangoLLVMGetValueKind)(LLVMValueRef Val);

  const char *(*TangoLLVMGetValueName2)(LLVMValueRef Val, size_t *Length);

  void (*TangoLLVMSetValueName2)(LLVMValueRef Val, const char *Name, size_t NameLen);

  void (*TangoLLVMDumpValue)(LLVMValueRef Val);

  char *(*TangoLLVMPrintValueToString)(LLVMValueRef Val);

  char *(*TangoLLVMPrintDbgRecordToString)(LLVMDbgRecordRef Record);

  void (*TangoLLVMReplaceAllUsesWith)(LLVMValueRef OldVal, LLVMValueRef NewVal);

  LLVMBool (*TangoLLVMIsConstant)(LLVMValueRef Val);

  LLVMBool (*TangoLLVMIsUndef)(LLVMValueRef Val);

  LLVMBool (*TangoLLVMIsPoison)(LLVMValueRef Val);

#define TANGO_LLVM_DECLARE_VALUE_CAST(name) \
  LLVMValueRef (*TangoLLVMIsA##name)(LLVMValueRef Val);
  LLVM_FOR_EACH_VALUE_SUBCLASS(TANGO_LLVM_DECLARE_VALUE_CAST)

  LLVMValueRef (*TangoLLVMIsAMDNode)(LLVMValueRef Val);
  LLVMValueRef (*TangoLLVMIsAValueAsMetadata)(LLVMValueRef Val);
  LLVMValueRef (*TangoLLVMIsAMDString)(LLVMValueRef Val);

  const char *(*TangoLLVMGetValueName)(LLVMValueRef Val);

  void (*TangoLLVMSetValueName)(LLVMValueRef Val, const char *Name);

  LLVMUseRef (*TangoLLVMGetFirstUse)(LLVMValueRef Val);

  LLVMUseRef (*TangoLLVMGetNextUse)(LLVMUseRef U);

  LLVMValueRef (*TangoLLVMGetUser)(LLVMUseRef U);

  LLVMValueRef (*TangoLLVMGetUsedValue)(LLVMUseRef U);

  LLVMValueRef (*TangoLLVMGetOperand)(LLVMValueRef Val, unsigned Index);

  LLVMUseRef (*TangoLLVMGetOperandUse)(LLVMValueRef Val, unsigned Index);

  void (*TangoLLVMSetOperand)(LLVMValueRef User, unsigned Index, LLVMValueRef Val);

  int (*TangoLLVMGetNumOperands)(LLVMValueRef Val);

  LLVMValueRef (*TangoLLVMConstNull)(LLVMTypeRef Ty);

  LLVMValueRef (*TangoLLVMConstAllOnes)(LLVMTypeRef Ty);

  LLVMValueRef (*TangoLLVMGetUndef)(LLVMTypeRef Ty);

  LLVMValueRef (*TangoLLVMGetPoison)(LLVMTypeRef Ty);

  LLVMBool (*TangoLLVMIsNull)(LLVMValueRef Val);

  LLVMValueRef (*TangoLLVMConstPointerNull)(LLVMTypeRef Ty);

  LLVMValueRef (*TangoLLVMConstInt)(LLVMTypeRef IntTy, unsigned long long N,
      LLVMBool SignExtend);

  LLVMValueRef (*TangoLLVMConstIntOfArbitraryPrecision)(LLVMTypeRef IntTy,
      unsigned NumWords,
      const uint64_t Words[]);

  LLVMValueRef (*TangoLLVMConstIntOfString)(LLVMTypeRef IntTy, const char *Text,
      uint8_t Radix);

  LLVMValueRef (*TangoLLVMConstIntOfStringAndSize)(LLVMTypeRef IntTy, const char *Text,
      unsigned SLen, uint8_t Radix);

  LLVMValueRef (*TangoLLVMConstReal)(LLVMTypeRef RealTy, double N);

  LLVMValueRef (*TangoLLVMConstRealOfString)(LLVMTypeRef RealTy, const char *Text);

  LLVMValueRef (*TangoLLVMConstRealOfStringAndSize)(LLVMTypeRef RealTy, const char *Text,
      unsigned SLen);

  unsigned long long (*TangoLLVMConstIntGetZExtValue)(LLVMValueRef ConstantVal);

  long long (*TangoLLVMConstIntGetSExtValue)(LLVMValueRef ConstantVal);

  double (*TangoLLVMConstRealGetDouble)(LLVMValueRef ConstantVal, LLVMBool *losesInfo);

  LLVMValueRef (*TangoLLVMConstStringInContext)(LLVMContextRef C, const char *Str,
      unsigned Length, LLVMBool DontNullTerminate);

  LLVMValueRef (*TangoLLVMConstStringInContext2)(LLVMContextRef C, const char *Str,
      size_t Length,
      LLVMBool DontNullTerminate);

  LLVMValueRef (*TangoLLVMConstString)(const char *Str, unsigned Length,
      LLVMBool DontNullTerminate);

  LLVMBool (*TangoLLVMIsConstantString)(LLVMValueRef c);

  const char *(*TangoLLVMGetAsString)(LLVMValueRef c, size_t *Length);

  LLVMValueRef (*TangoLLVMConstStructInContext)(LLVMContextRef C,
      LLVMValueRef *ConstantVals,
      unsigned Count, LLVMBool Packed);

  LLVMValueRef (*TangoLLVMConstStruct)(LLVMValueRef *ConstantVals, unsigned Count,
      LLVMBool Packed);

  LLVMValueRef (*TangoLLVMConstArray)(LLVMTypeRef ElementTy,
      LLVMValueRef *ConstantVals, unsigned Length);

  LLVMValueRef (*TangoLLVMConstArray2)(LLVMTypeRef ElementTy, LLVMValueRef *ConstantVals,
      uint64_t Length);

  LLVMValueRef (*TangoLLVMConstNamedStruct)(LLVMTypeRef StructTy,
      LLVMValueRef *ConstantVals,
      unsigned Count);

  LLVMValueRef (*TangoLLVMGetAggregateElement)(LLVMValueRef C, unsigned Idx);

  LLVMValueRef (*TangoLLVMGetElementAsConstant)(LLVMValueRef C, unsigned idx);

  LLVMValueRef (*TangoLLVMConstVector)(LLVMValueRef *ScalarConstantVals, unsigned Size);

  LLVMValueRef (*TangoLLVMConstantPtrAuth)(LLVMValueRef Ptr, LLVMValueRef Key,
      LLVMValueRef Disc, LLVMValueRef AddrDisc);

  LLVMOpcode (*TangoLLVMGetConstOpcode)(LLVMValueRef ConstantVal);
  LLVMValueRef (*TangoLLVMAlignOf)(LLVMTypeRef Ty);
  LLVMValueRef (*TangoLLVMSizeOf)(LLVMTypeRef Ty);
  LLVMValueRef (*TangoLLVMConstNeg)(LLVMValueRef ConstantVal);
  LLVMValueRef (*TangoLLVMConstNSWNeg)(LLVMValueRef ConstantVal);
  LLVMValueRef (*TangoLLVMConstNUWNeg)(LLVMValueRef ConstantVal);

  LLVMValueRef (*TangoLLVMConstNot)(LLVMValueRef ConstantVal);
  LLVMValueRef (*TangoLLVMConstAdd)(LLVMValueRef LHSConstant, LLVMValueRef RHSConstant);
  LLVMValueRef (*TangoLLVMConstNSWAdd)(LLVMValueRef LHSConstant, LLVMValueRef RHSConstant);
  LLVMValueRef (*TangoLLVMConstNUWAdd)(LLVMValueRef LHSConstant, LLVMValueRef RHSConstant);
  LLVMValueRef (*TangoLLVMConstSub)(LLVMValueRef LHSConstant, LLVMValueRef RHSConstant);
  LLVMValueRef (*TangoLLVMConstNSWSub)(LLVMValueRef LHSConstant, LLVMValueRef RHSConstant);
  LLVMValueRef (*TangoLLVMConstNUWSub)(LLVMValueRef LHSConstant, LLVMValueRef RHSConstant);
  LLVMValueRef (*TangoLLVMConstMul)(LLVMValueRef LHSConstant, LLVMValueRef RHSConstant);
  LLVMValueRef (*TangoLLVMConstNSWMul)(LLVMValueRef LHSConstant, LLVMValueRef RHSConstant);
  LLVMValueRef (*TangoLLVMConstNUWMul)(LLVMValueRef LHSConstant, LLVMValueRef RHSConstant);
  LLVMValueRef (*TangoLLVMConstXor)(LLVMValueRef LHSConstant, LLVMValueRef RHSConstant);
  LLVMValueRef (*TangoLLVMConstGEP2)(LLVMTypeRef Ty, LLVMValueRef ConstantVal,
      LLVMValueRef *ConstantIndices, unsigned NumIndices);
  LLVMValueRef (*TangoLLVMConstInBoundsGEP2)(LLVMTypeRef Ty, LLVMValueRef ConstantVal,
      LLVMValueRef *ConstantIndices,
      unsigned NumIndices);

  LLVMValueRef (*TangoLLVMConstGEPWithNoWrapFlags)(LLVMTypeRef Ty,
      LLVMValueRef ConstantVal,
      LLVMValueRef *ConstantIndices,
      unsigned NumIndices,
      LLVMGEPNoWrapFlags NoWrapFlags);
  LLVMValueRef (*TangoLLVMConstTrunc)(LLVMValueRef ConstantVal, LLVMTypeRef ToType);
  LLVMValueRef (*TangoLLVMConstPtrToInt)(LLVMValueRef ConstantVal, LLVMTypeRef ToType);
  LLVMValueRef (*TangoLLVMConstIntToPtr)(LLVMValueRef ConstantVal, LLVMTypeRef ToType);
  LLVMValueRef (*TangoLLVMConstBitCast)(LLVMValueRef ConstantVal, LLVMTypeRef ToType);
  LLVMValueRef (*TangoLLVMConstAddrSpaceCast)(LLVMValueRef ConstantVal, LLVMTypeRef ToType);
  LLVMValueRef (*TangoLLVMConstTruncOrBitCast)(LLVMValueRef ConstantVal,
      LLVMTypeRef ToType);
  LLVMValueRef (*TangoLLVMConstPointerCast)(LLVMValueRef ConstantVal,
      LLVMTypeRef ToType);
  LLVMValueRef (*TangoLLVMConstExtractElement)(LLVMValueRef VectorConstant,
      LLVMValueRef IndexConstant);
  LLVMValueRef (*TangoLLVMConstInsertElement)(LLVMValueRef VectorConstant,
      LLVMValueRef ElementValueConstant,
      LLVMValueRef IndexConstant);
  LLVMValueRef (*TangoLLVMConstShuffleVector)(LLVMValueRef VectorAConstant,
      LLVMValueRef VectorBConstant,
      LLVMValueRef MaskConstant);
  LLVMValueRef (*TangoLLVMBlockAddress)(LLVMValueRef F, LLVMBasicBlockRef BB);

  LLVMValueRef (*TangoLLVMGetBlockAddressFunction)(LLVMValueRef BlockAddr);

  LLVMBasicBlockRef (*TangoLLVMGetBlockAddressBasicBlock)(LLVMValueRef BlockAddr);

  LLVMValueRef (*TangoLLVMConstInlineAsm)(LLVMTypeRef Ty,
      const char *AsmString, const char *Constraints,
      LLVMBool HasSideEffects, LLVMBool IsAlignStack);

  LLVMModuleRef (*TangoLLVMGetGlobalParent)(LLVMValueRef Global);
  LLVMBool (*TangoLLVMIsDeclaration)(LLVMValueRef Global);
  LLVMLinkage (*TangoLLVMGetLinkage)(LLVMValueRef Global);
  void (*TangoLLVMSetLinkage)(LLVMValueRef Global, LLVMLinkage Linkage);
  const char *(*TangoLLVMGetSection)(LLVMValueRef Global);
  void (*TangoLLVMSetSection)(LLVMValueRef Global, const char *Section);
  LLVMVisibility (*TangoLLVMGetVisibility)(LLVMValueRef Global);
  void (*TangoLLVMSetVisibility)(LLVMValueRef Global, LLVMVisibility Viz);
  LLVMDLLStorageClass (*TangoLLVMGetDLLStorageClass)(LLVMValueRef Global);
  void (*TangoLLVMSetDLLStorageClass)(LLVMValueRef Global, LLVMDLLStorageClass Class);
  LLVMUnnamedAddr (*TangoLLVMGetUnnamedAddress)(LLVMValueRef Global);
  void (*TangoLLVMSetUnnamedAddress)(LLVMValueRef Global, LLVMUnnamedAddr UnnamedAddr);

  LLVMTypeRef (*TangoLLVMGlobalGetValueType)(LLVMValueRef Global);

  LLVMBool (*TangoLLVMHasUnnamedAddr)(LLVMValueRef Global);

  void (*TangoLLVMSetUnnamedAddr)(LLVMValueRef Global, LLVMBool HasUnnamedAddr);

  unsigned (*TangoLLVMGetAlignment)(LLVMValueRef V);

  void (*TangoLLVMSetAlignment)(LLVMValueRef V, unsigned Bytes);

  void (*TangoLLVMGlobalSetMetadata)(LLVMValueRef Global, unsigned Kind,
      LLVMMetadataRef MD);

  void (*TangoLLVMGlobalEraseMetadata)(LLVMValueRef Global, unsigned Kind);

  void (*TangoLLVMGlobalClearMetadata)(LLVMValueRef Global);

  LLVMValueMetadataEntry *(*TangoLLVMGlobalCopyAllMetadata)(LLVMValueRef Value,
      size_t *NumEntries);

  void (*TangoLLVMDisposeValueMetadataEntries)(LLVMValueMetadataEntry *Entries);

  unsigned (*TangoLLVMValueMetadataEntriesGetKind)(LLVMValueMetadataEntry *Entries,
      unsigned Index);

  LLVMMetadataRef (*TangoLLVMValueMetadataEntriesGetMetadata)(LLVMValueMetadataEntry *Entries, unsigned Index);

  LLVMValueRef (*TangoLLVMAddGlobal)(LLVMModuleRef M, LLVMTypeRef Ty, const char *Name);
  LLVMValueRef (*TangoLLVMAddGlobalInAddressSpace)(LLVMModuleRef M, LLVMTypeRef Ty,
      const char *Name,
      unsigned AddressSpace);
  LLVMValueRef (*TangoLLVMGetNamedGlobal)(LLVMModuleRef M, const char *Name);
  LLVMValueRef (*TangoLLVMGetFirstGlobal)(LLVMModuleRef M);
  LLVMValueRef (*TangoLLVMGetLastGlobal)(LLVMModuleRef M);
  LLVMValueRef (*TangoLLVMGetNextGlobal)(LLVMValueRef GlobalVar);
  LLVMValueRef (*TangoLLVMGetPreviousGlobal)(LLVMValueRef GlobalVar);
  void (*TangoLLVMDeleteGlobal)(LLVMValueRef GlobalVar);
  LLVMValueRef (*TangoLLVMGetInitializer)(LLVMValueRef GlobalVar);
  void (*TangoLLVMSetInitializer)(LLVMValueRef GlobalVar, LLVMValueRef ConstantVal);
  LLVMBool (*TangoLLVMIsThreadLocal)(LLVMValueRef GlobalVar);
  void (*TangoLLVMSetThreadLocal)(LLVMValueRef GlobalVar, LLVMBool IsThreadLocal);
  LLVMBool (*TangoLLVMIsGlobalConstant)(LLVMValueRef GlobalVar);
  void (*TangoLLVMSetGlobalConstant)(LLVMValueRef GlobalVar, LLVMBool IsConstant);
  LLVMThreadLocalMode (*TangoLLVMGetThreadLocalMode)(LLVMValueRef GlobalVar);
  void (*TangoLLVMSetThreadLocalMode)(LLVMValueRef GlobalVar, LLVMThreadLocalMode Mode);
  LLVMBool (*TangoLLVMIsExternallyInitialized)(LLVMValueRef GlobalVar);
  void (*TangoLLVMSetExternallyInitialized)(LLVMValueRef GlobalVar, LLVMBool IsExtInit);

  LLVMValueRef (*TangoLLVMAddAlias2)(LLVMModuleRef M, LLVMTypeRef ValueTy,
      unsigned AddrSpace, LLVMValueRef Aliasee,
      const char *Name);

  LLVMValueRef (*TangoLLVMGetNamedGlobalAlias)(LLVMModuleRef M,
      const char *Name, size_t NameLen);

  LLVMValueRef (*TangoLLVMGetFirstGlobalAlias)(LLVMModuleRef M);

  LLVMValueRef (*TangoLLVMGetLastGlobalAlias)(LLVMModuleRef M);

  LLVMValueRef (*TangoLLVMGetNextGlobalAlias)(LLVMValueRef GA);

  LLVMValueRef (*TangoLLVMGetPreviousGlobalAlias)(LLVMValueRef GA);

  LLVMValueRef (*TangoLLVMAliasGetAliasee)(LLVMValueRef Alias);

  void (*TangoLLVMAliasSetAliasee)(LLVMValueRef Alias, LLVMValueRef Aliasee);

  void (*TangoLLVMDeleteFunction)(LLVMValueRef Fn);

  LLVMBool (*TangoLLVMHasPersonalityFn)(LLVMValueRef Fn);

  LLVMValueRef (*TangoLLVMGetPersonalityFn)(LLVMValueRef Fn);

  void (*TangoLLVMSetPersonalityFn)(LLVMValueRef Fn, LLVMValueRef PersonalityFn);

  unsigned (*TangoLLVMLookupIntrinsicID)(const char *Name, size_t NameLen);

  unsigned (*TangoLLVMGetIntrinsicID)(LLVMValueRef Fn);

  LLVMValueRef (*TangoLLVMGetIntrinsicDeclaration)(LLVMModuleRef Mod,
      unsigned ID,
      LLVMTypeRef *ParamTypes,
      size_t ParamCount);

  LLVMTypeRef (*TangoLLVMIntrinsicGetType)(LLVMContextRef Ctx, unsigned ID,
      LLVMTypeRef *ParamTypes, size_t ParamCount);

  const char *(*TangoLLVMIntrinsicGetName)(unsigned ID, size_t *NameLength);

  const char *(*TangoLLVMIntrinsicCopyOverloadedName)(unsigned ID,
      LLVMTypeRef *ParamTypes,
      size_t ParamCount,
      size_t *NameLength);

  const char *(*TangoLLVMIntrinsicCopyOverloadedName2)(LLVMModuleRef Mod, unsigned ID,
      LLVMTypeRef *ParamTypes,
      size_t ParamCount,
      size_t *NameLength);

  LLVMBool (*TangoLLVMIntrinsicIsOverloaded)(unsigned ID);

  unsigned (*TangoLLVMGetFunctionCallConv)(LLVMValueRef Fn);

  void (*TangoLLVMSetFunctionCallConv)(LLVMValueRef Fn, unsigned CC);

  const char *(*TangoLLVMGetGC)(LLVMValueRef Fn);

  void (*TangoLLVMSetGC)(LLVMValueRef Fn, const char *Name);

  LLVMValueRef (*TangoLLVMGetPrefixData)(LLVMValueRef Fn);

  LLVMBool (*TangoLLVMHasPrefixData)(LLVMValueRef Fn);

  void (*TangoLLVMSetPrefixData)(LLVMValueRef Fn, LLVMValueRef prefixData);

  LLVMValueRef (*TangoLLVMGetPrologueData)(LLVMValueRef Fn);

  LLVMBool (*TangoLLVMHasPrologueData)(LLVMValueRef Fn);

  void (*TangoLLVMSetPrologueData)(LLVMValueRef Fn, LLVMValueRef prologueData);

  void (*TangoLLVMAddAttributeAtIndex)(LLVMValueRef F, LLVMAttributeIndex Idx,
      LLVMAttributeRef A);
  unsigned (*TangoLLVMGetAttributeCountAtIndex)(LLVMValueRef F, LLVMAttributeIndex Idx);
  void (*TangoLLVMGetAttributesAtIndex)(LLVMValueRef F, LLVMAttributeIndex Idx,
      LLVMAttributeRef *Attrs);
  LLVMAttributeRef (*TangoLLVMGetEnumAttributeAtIndex)(LLVMValueRef F,
      LLVMAttributeIndex Idx,
      unsigned KindID);
  LLVMAttributeRef (*TangoLLVMGetStringAttributeAtIndex)(LLVMValueRef F,
      LLVMAttributeIndex Idx,
      const char *K, unsigned KLen);
  void (*TangoLLVMRemoveEnumAttributeAtIndex)(LLVMValueRef F, LLVMAttributeIndex Idx,
      unsigned KindID);
  void (*TangoLLVMRemoveStringAttributeAtIndex)(LLVMValueRef F, LLVMAttributeIndex Idx,
      const char *K, unsigned KLen);

  void (*TangoLLVMAddTargetDependentFunctionAttr)(LLVMValueRef Fn, const char *A,
      const char *V);

  unsigned (*TangoLLVMCountParams)(LLVMValueRef Fn);

  void (*TangoLLVMGetParams)(LLVMValueRef Fn, LLVMValueRef *Params);

  LLVMValueRef (*TangoLLVMGetParam)(LLVMValueRef Fn, unsigned Index);

  LLVMValueRef (*TangoLLVMGetParamParent)(LLVMValueRef Inst);

  LLVMValueRef (*TangoLLVMGetFirstParam)(LLVMValueRef Fn);

  LLVMValueRef (*TangoLLVMGetLastParam)(LLVMValueRef Fn);

  LLVMValueRef (*TangoLLVMGetNextParam)(LLVMValueRef Arg);

  LLVMValueRef (*TangoLLVMGetPreviousParam)(LLVMValueRef Arg);

  void (*TangoLLVMSetParamAlignment)(LLVMValueRef Arg, unsigned Align);

  LLVMValueRef (*TangoLLVMAddGlobalIFunc)(LLVMModuleRef M,
      const char *Name, size_t NameLen,
      LLVMTypeRef Ty, unsigned AddrSpace,
      LLVMValueRef Resolver);

  LLVMValueRef (*TangoLLVMGetNamedGlobalIFunc)(LLVMModuleRef M,
      const char *Name, size_t NameLen);

  LLVMValueRef (*TangoLLVMGetFirstGlobalIFunc)(LLVMModuleRef M);

  LLVMValueRef (*TangoLLVMGetLastGlobalIFunc)(LLVMModuleRef M);

  LLVMValueRef (*TangoLLVMGetNextGlobalIFunc)(LLVMValueRef IFunc);

  LLVMValueRef (*TangoLLVMGetPreviousGlobalIFunc)(LLVMValueRef IFunc);

  LLVMValueRef (*TangoLLVMGetGlobalIFuncResolver)(LLVMValueRef IFunc);

  void (*TangoLLVMSetGlobalIFuncResolver)(LLVMValueRef IFunc, LLVMValueRef Resolver);

  void (*TangoLLVMEraseGlobalIFunc)(LLVMValueRef IFunc);

  void (*TangoLLVMRemoveGlobalIFunc)(LLVMValueRef IFunc);

  LLVMMetadataRef (*TangoLLVMMDStringInContext2)(LLVMContextRef C, const char *Str,
      size_t SLen);

  LLVMMetadataRef (*TangoLLVMMDNodeInContext2)(LLVMContextRef C, LLVMMetadataRef *MDs,
      size_t Count);

  LLVMValueRef (*TangoLLVMMetadataAsValue)(LLVMContextRef C, LLVMMetadataRef MD);

  LLVMMetadataRef (*TangoLLVMValueAsMetadata)(LLVMValueRef Val);

  const char *(*TangoLLVMGetMDString)(LLVMValueRef V, unsigned *Length);

  unsigned (*TangoLLVMGetMDNodeNumOperands)(LLVMValueRef V);

  void (*TangoLLVMGetMDNodeOperands)(LLVMValueRef V, LLVMValueRef *Dest);

  void (*TangoLLVMReplaceMDNodeOperandWith)(LLVMValueRef V, unsigned Index,
      LLVMMetadataRef Replacement);

  LLVMValueRef (*TangoLLVMMDStringInContext)(LLVMContextRef C, const char *Str,
      unsigned SLen);

  LLVMValueRef (*TangoLLVMMDString)(const char *Str, unsigned SLen);

  LLVMValueRef (*TangoLLVMMDNodeInContext)(LLVMContextRef C, LLVMValueRef *Vals,
      unsigned Count);

  LLVMValueRef (*TangoLLVMMDNode)(LLVMValueRef *Vals, unsigned Count);

  LLVMOperandBundleRef (*TangoLLVMCreateOperandBundle)(const char *Tag, size_t TagLen,
      LLVMValueRef *Args,
      unsigned NumArgs);

  void (*TangoLLVMDisposeOperandBundle)(LLVMOperandBundleRef Bundle);

  const char *(*TangoLLVMGetOperandBundleTag)(LLVMOperandBundleRef Bundle, size_t *Len);

  unsigned (*TangoLLVMGetNumOperandBundleArgs)(LLVMOperandBundleRef Bundle);

  LLVMValueRef (*TangoLLVMGetOperandBundleArgAtIndex)(LLVMOperandBundleRef Bundle,
      unsigned Index);

  LLVMValueRef (*TangoLLVMBasicBlockAsValue)(LLVMBasicBlockRef BB);

  LLVMBool (*TangoLLVMValueIsBasicBlock)(LLVMValueRef Val);

  LLVMBasicBlockRef (*TangoLLVMValueAsBasicBlock)(LLVMValueRef Val);

  const char *(*TangoLLVMGetBasicBlockName)(LLVMBasicBlockRef BB);

  LLVMValueRef (*TangoLLVMGetBasicBlockParent)(LLVMBasicBlockRef BB);

  LLVMValueRef (*TangoLLVMGetBasicBlockTerminator)(LLVMBasicBlockRef BB);

  unsigned (*TangoLLVMCountBasicBlocks)(LLVMValueRef Fn);

  void (*TangoLLVMGetBasicBlocks)(LLVMValueRef Fn, LLVMBasicBlockRef *BasicBlocks);

  LLVMBasicBlockRef (*TangoLLVMGetFirstBasicBlock)(LLVMValueRef Fn);

  LLVMBasicBlockRef (*TangoLLVMGetLastBasicBlock)(LLVMValueRef Fn);

  LLVMBasicBlockRef (*TangoLLVMGetNextBasicBlock)(LLVMBasicBlockRef BB);

  LLVMBasicBlockRef (*TangoLLVMGetPreviousBasicBlock)(LLVMBasicBlockRef BB);

  LLVMBasicBlockRef (*TangoLLVMGetEntryBasicBlock)(LLVMValueRef Fn);

  void (*TangoLLVMInsertExistingBasicBlockAfterInsertBlock)(LLVMBuilderRef Builder,
      LLVMBasicBlockRef BB);

  void (*TangoLLVMAppendExistingBasicBlock)(LLVMValueRef Fn,
      LLVMBasicBlockRef BB);

  LLVMBasicBlockRef (*TangoLLVMCreateBasicBlockInContext)(LLVMContextRef C,
      const char *Name);

  LLVMBasicBlockRef (*TangoLLVMAppendBasicBlockInContext)(LLVMContextRef C,
      LLVMValueRef Fn,
      const char *Name);

  LLVMBasicBlockRef (*TangoLLVMAppendBasicBlock)(LLVMValueRef Fn, const char *Name);

  LLVMBasicBlockRef (*TangoLLVMInsertBasicBlockInContext)(LLVMContextRef C,
      LLVMBasicBlockRef BB,
      const char *Name);

  LLVMBasicBlockRef (*TangoLLVMInsertBasicBlock)(LLVMBasicBlockRef InsertBeforeBB,
      const char *Name);

  void (*TangoLLVMDeleteBasicBlock)(LLVMBasicBlockRef BB);

  void (*TangoLLVMRemoveBasicBlockFromParent)(LLVMBasicBlockRef BB);

  void (*TangoLLVMMoveBasicBlockBefore)(LLVMBasicBlockRef BB, LLVMBasicBlockRef MovePos);

  void (*TangoLLVMMoveBasicBlockAfter)(LLVMBasicBlockRef BB, LLVMBasicBlockRef MovePos);

  LLVMValueRef (*TangoLLVMGetFirstInstruction)(LLVMBasicBlockRef BB);

  LLVMValueRef (*TangoLLVMGetLastInstruction)(LLVMBasicBlockRef BB);

  int (*TangoLLVMHasMetadata)(LLVMValueRef Val);

  LLVMValueRef (*TangoLLVMGetMetadata)(LLVMValueRef Val, unsigned KindID);

  void (*TangoLLVMSetMetadata)(LLVMValueRef Val, unsigned KindID, LLVMValueRef Node);

  LLVMValueMetadataEntry* (*TangoLLVMInstructionGetAllMetadataOtherThanDebugLoc)(LLVMValueRef Instr,
        size_t *NumEntries);

  LLVMBasicBlockRef (*TangoLLVMGetInstructionParent)(LLVMValueRef Inst);

  LLVMValueRef (*TangoLLVMGetNextInstruction)(LLVMValueRef Inst);

  LLVMValueRef (*TangoLLVMGetPreviousInstruction)(LLVMValueRef Inst);

  void (*TangoLLVMInstructionRemoveFromParent)(LLVMValueRef Inst);

  void (*TangoLLVMInstructionEraseFromParent)(LLVMValueRef Inst);

  void (*TangoLLVMDeleteInstruction)(LLVMValueRef Inst);

  LLVMOpcode (*TangoLLVMGetInstructionOpcode)(LLVMValueRef Inst);

  LLVMIntPredicate (*TangoLLVMGetICmpPredicate)(LLVMValueRef Inst);

  LLVMRealPredicate (*TangoLLVMGetFCmpPredicate)(LLVMValueRef Inst);

  LLVMValueRef (*TangoLLVMInstructionClone)(LLVMValueRef Inst);

  LLVMValueRef (*TangoLLVMIsATerminatorInst)(LLVMValueRef Inst);

  unsigned (*TangoLLVMGetNumArgOperands)(LLVMValueRef Instr);

  void (*TangoLLVMSetInstructionCallConv)(LLVMValueRef Instr, unsigned CC);

  unsigned (*TangoLLVMGetInstructionCallConv)(LLVMValueRef Instr);

  void (*TangoLLVMSetInstrParamAlignment)(LLVMValueRef Instr, LLVMAttributeIndex Idx,
      unsigned Align);

  void (*TangoLLVMAddCallSiteAttribute)(LLVMValueRef C, LLVMAttributeIndex Idx,
      LLVMAttributeRef A);
  unsigned (*TangoLLVMGetCallSiteAttributeCount)(LLVMValueRef C, LLVMAttributeIndex Idx);
  void (*TangoLLVMGetCallSiteAttributes)(LLVMValueRef C, LLVMAttributeIndex Idx,
      LLVMAttributeRef *Attrs);
  LLVMAttributeRef (*TangoLLVMGetCallSiteEnumAttribute)(LLVMValueRef C,
      LLVMAttributeIndex Idx,
      unsigned KindID);
  LLVMAttributeRef (*TangoLLVMGetCallSiteStringAttribute)(LLVMValueRef C,
      LLVMAttributeIndex Idx,
      const char *K, unsigned KLen);
  void (*TangoLLVMRemoveCallSiteEnumAttribute)(LLVMValueRef C, LLVMAttributeIndex Idx,
      unsigned KindID);
  void (*TangoLLVMRemoveCallSiteStringAttribute)(LLVMValueRef C, LLVMAttributeIndex Idx,
      const char *K, unsigned KLen);

  LLVMTypeRef (*TangoLLVMGetCalledFunctionType)(LLVMValueRef C);

  LLVMValueRef (*TangoLLVMGetCalledValue)(LLVMValueRef Instr);

  unsigned (*TangoLLVMGetNumOperandBundles)(LLVMValueRef C);

  LLVMOperandBundleRef (*TangoLLVMGetOperandBundleAtIndex)(LLVMValueRef C,
      unsigned Index);

  LLVMBool (*TangoLLVMIsTailCall)(LLVMValueRef CallInst);

  void (*TangoLLVMSetTailCall)(LLVMValueRef CallInst, LLVMBool IsTailCall);

  LLVMTailCallKind (*TangoLLVMGetTailCallKind)(LLVMValueRef CallInst);

  void (*TangoLLVMSetTailCallKind)(LLVMValueRef CallInst, LLVMTailCallKind kind);

  LLVMBasicBlockRef (*TangoLLVMGetNormalDest)(LLVMValueRef InvokeInst);

  LLVMBasicBlockRef (*TangoLLVMGetUnwindDest)(LLVMValueRef InvokeInst);

  void (*TangoLLVMSetNormalDest)(LLVMValueRef InvokeInst, LLVMBasicBlockRef B);

  void (*TangoLLVMSetUnwindDest)(LLVMValueRef InvokeInst, LLVMBasicBlockRef B);

  LLVMBasicBlockRef (*TangoLLVMGetCallBrDefaultDest)(LLVMValueRef CallBr);

  unsigned (*TangoLLVMGetCallBrNumIndirectDests)(LLVMValueRef CallBr);

  LLVMBasicBlockRef (*TangoLLVMGetCallBrIndirectDest)(LLVMValueRef CallBr, unsigned Idx);

  unsigned (*TangoLLVMGetNumSuccessors)(LLVMValueRef Term);

  LLVMBasicBlockRef (*TangoLLVMGetSuccessor)(LLVMValueRef Term, unsigned i);

  void (*TangoLLVMSetSuccessor)(LLVMValueRef Term, unsigned i, LLVMBasicBlockRef block);

  LLVMBool (*TangoLLVMIsConditional)(LLVMValueRef Branch);

  LLVMValueRef (*TangoLLVMGetCondition)(LLVMValueRef Branch);

  void (*TangoLLVMSetCondition)(LLVMValueRef Branch, LLVMValueRef Cond);

  LLVMBasicBlockRef (*TangoLLVMGetSwitchDefaultDest)(LLVMValueRef SwitchInstr);

  LLVMTypeRef (*TangoLLVMGetAllocatedType)(LLVMValueRef Alloca);

  LLVMBool (*TangoLLVMIsInBounds)(LLVMValueRef GEP);

  void (*TangoLLVMSetIsInBounds)(LLVMValueRef GEP, LLVMBool InBounds);

  LLVMTypeRef (*TangoLLVMGetGEPSourceElementType)(LLVMValueRef GEP);

  LLVMGEPNoWrapFlags (*TangoLLVMGEPGetNoWrapFlags)(LLVMValueRef GEP);

  void (*TangoLLVMGEPSetNoWrapFlags)(LLVMValueRef GEP, LLVMGEPNoWrapFlags NoWrapFlags);

  void (*TangoLLVMAddIncoming)(LLVMValueRef PhiNode, LLVMValueRef *IncomingValues,
      LLVMBasicBlockRef *IncomingBlocks, unsigned Count);

  unsigned (*TangoLLVMCountIncoming)(LLVMValueRef PhiNode);

  LLVMValueRef (*TangoLLVMGetIncomingValue)(LLVMValueRef PhiNode, unsigned Index);

  LLVMBasicBlockRef (*TangoLLVMGetIncomingBlock)(LLVMValueRef PhiNode, unsigned Index);

  unsigned (*TangoLLVMGetNumIndices)(LLVMValueRef Inst);

  const unsigned *(*TangoLLVMGetIndices)(LLVMValueRef Inst);

  LLVMBuilderRef (*TangoLLVMCreateBuilderInContext)(LLVMContextRef C);
  LLVMBuilderRef (*TangoLLVMCreateBuilder)(void);

  void (*TangoLLVMPositionBuilder)(LLVMBuilderRef Builder, LLVMBasicBlockRef Block,
      LLVMValueRef Instr);

  void (*TangoLLVMPositionBuilderBeforeDbgRecords)(LLVMBuilderRef Builder,
      LLVMBasicBlockRef Block,
      LLVMValueRef Inst);

  void (*TangoLLVMPositionBuilderBefore)(LLVMBuilderRef Builder, LLVMValueRef Instr);

  void (*TangoLLVMPositionBuilderBeforeInstrAndDbgRecords)(LLVMBuilderRef Builder,
      LLVMValueRef Instr);
  void (*TangoLLVMPositionBuilderAtEnd)(LLVMBuilderRef Builder, LLVMBasicBlockRef Block);
  LLVMBasicBlockRef (*TangoLLVMGetInsertBlock)(LLVMBuilderRef Builder);
  void (*TangoLLVMClearInsertionPosition)(LLVMBuilderRef Builder);
  void (*TangoLLVMInsertIntoBuilder)(LLVMBuilderRef Builder, LLVMValueRef Instr);
  void (*TangoLLVMInsertIntoBuilderWithName)(LLVMBuilderRef Builder, LLVMValueRef Instr,
      const char *Name);
  void (*TangoLLVMDisposeBuilder)(LLVMBuilderRef Builder);

  LLVMMetadataRef (*TangoLLVMGetCurrentDebugLocation2)(LLVMBuilderRef Builder);

  void (*TangoLLVMSetCurrentDebugLocation2)(LLVMBuilderRef Builder, LLVMMetadataRef Loc);

  void (*TangoLLVMSetInstDebugLocation)(LLVMBuilderRef Builder, LLVMValueRef Inst);

  void (*TangoLLVMAddMetadataToInst)(LLVMBuilderRef Builder, LLVMValueRef Inst);

  LLVMMetadataRef (*TangoLLVMBuilderGetDefaultFPMathTag)(LLVMBuilderRef Builder);

  void (*TangoLLVMBuilderSetDefaultFPMathTag)(LLVMBuilderRef Builder,
      LLVMMetadataRef FPMathTag);

  void (*TangoLLVMSetCurrentDebugLocation)(LLVMBuilderRef Builder, LLVMValueRef L);

  LLVMValueRef (*TangoLLVMGetCurrentDebugLocation)(LLVMBuilderRef Builder);

  LLVMValueRef (*TangoLLVMBuildRetVoid)(LLVMBuilderRef);
  LLVMValueRef (*TangoLLVMBuildRet)(LLVMBuilderRef, LLVMValueRef V);
  LLVMValueRef (*TangoLLVMBuildAggregateRet)(LLVMBuilderRef, LLVMValueRef *RetVals,
      unsigned N);
  LLVMValueRef (*TangoLLVMBuildBr)(LLVMBuilderRef, LLVMBasicBlockRef Dest);
  LLVMValueRef (*TangoLLVMBuildCondBr)(LLVMBuilderRef, LLVMValueRef If,
      LLVMBasicBlockRef Then, LLVMBasicBlockRef Else);
  LLVMValueRef (*TangoLLVMBuildSwitch)(LLVMBuilderRef, LLVMValueRef V,
      LLVMBasicBlockRef Else, unsigned NumCases);
  LLVMValueRef (*TangoLLVMBuildIndirectBr)(LLVMBuilderRef B, LLVMValueRef Addr,
      unsigned NumDests);
  LLVMValueRef (*TangoLLVMBuildCallBr)(LLVMBuilderRef B, LLVMTypeRef Ty, LLVMValueRef Fn,
      LLVMBasicBlockRef DefaultDest,
      LLVMBasicBlockRef *IndirectDests,
      unsigned NumIndirectDests, LLVMValueRef *Args,
      unsigned NumArgs, LLVMOperandBundleRef *Bundles,
      unsigned NumBundles, const char *Name);
  LLVMValueRef (*TangoLLVMBuildInvoke2)(LLVMBuilderRef, LLVMTypeRef Ty, LLVMValueRef Fn,
      LLVMValueRef *Args, unsigned NumArgs,
      LLVMBasicBlockRef Then, LLVMBasicBlockRef Catch,
      const char *Name);
  LLVMValueRef (*TangoLLVMBuildInvokeWithOperandBundles)(
      LLVMBuilderRef, LLVMTypeRef Ty, LLVMValueRef Fn, LLVMValueRef *Args,
      unsigned NumArgs, LLVMBasicBlockRef Then, LLVMBasicBlockRef Catch,
      LLVMOperandBundleRef *Bundles, unsigned NumBundles, const char *Name);
  LLVMValueRef (*TangoLLVMBuildUnreachable)(LLVMBuilderRef);

  LLVMValueRef (*TangoLLVMBuildResume)(LLVMBuilderRef B, LLVMValueRef Exn);
  LLVMValueRef (*TangoLLVMBuildLandingPad)(LLVMBuilderRef B, LLVMTypeRef Ty,
      LLVMValueRef PersFn, unsigned NumClauses,
      const char *Name);
  LLVMValueRef (*TangoLLVMBuildCleanupRet)(LLVMBuilderRef B, LLVMValueRef CatchPad,
      LLVMBasicBlockRef BB);
  LLVMValueRef (*TangoLLVMBuildCatchRet)(LLVMBuilderRef B, LLVMValueRef CatchPad,
      LLVMBasicBlockRef BB);
  LLVMValueRef (*TangoLLVMBuildCatchPad)(LLVMBuilderRef B, LLVMValueRef ParentPad,
      LLVMValueRef *Args, unsigned NumArgs,
      const char *Name);
  LLVMValueRef (*TangoLLVMBuildCleanupPad)(LLVMBuilderRef B, LLVMValueRef ParentPad,
      LLVMValueRef *Args, unsigned NumArgs,
      const char *Name);
  LLVMValueRef (*TangoLLVMBuildCatchSwitch)(LLVMBuilderRef B, LLVMValueRef ParentPad,
      LLVMBasicBlockRef UnwindBB,
      unsigned NumHandlers, const char *Name);

  void (*TangoLLVMAddCase)(LLVMValueRef Switch, LLVMValueRef OnVal,
      LLVMBasicBlockRef Dest);

  void (*TangoLLVMAddDestination)(LLVMValueRef IndirectBr, LLVMBasicBlockRef Dest);

  unsigned (*TangoLLVMGetNumClauses)(LLVMValueRef LandingPad);

  LLVMValueRef (*TangoLLVMGetClause)(LLVMValueRef LandingPad, unsigned Idx);

  void (*TangoLLVMAddClause)(LLVMValueRef LandingPad, LLVMValueRef ClauseVal);

  LLVMBool (*TangoLLVMIsCleanup)(LLVMValueRef LandingPad);

  void (*TangoLLVMSetCleanup)(LLVMValueRef LandingPad, LLVMBool Val);

  void (*TangoLLVMAddHandler)(LLVMValueRef CatchSwitch, LLVMBasicBlockRef Dest);

  unsigned (*TangoLLVMGetNumHandlers)(LLVMValueRef CatchSwitch);

  void (*TangoLLVMGetHandlers)(LLVMValueRef CatchSwitch, LLVMBasicBlockRef *Handlers);

  LLVMValueRef (*TangoLLVMGetArgOperand)(LLVMValueRef Funclet, unsigned i);

  void (*TangoLLVMSetArgOperand)(LLVMValueRef Funclet, unsigned i, LLVMValueRef value);

  LLVMValueRef (*TangoLLVMGetParentCatchSwitch)(LLVMValueRef CatchPad);

  void (*TangoLLVMSetParentCatchSwitch)(LLVMValueRef CatchPad, LLVMValueRef CatchSwitch);

  LLVMValueRef (*TangoLLVMBuildAdd)(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS,
      const char *Name);
  LLVMValueRef (*TangoLLVMBuildNSWAdd)(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS,
      const char *Name);
  LLVMValueRef (*TangoLLVMBuildNUWAdd)(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS,
      const char *Name);
  LLVMValueRef (*TangoLLVMBuildFAdd)(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS,
      const char *Name);
  LLVMValueRef (*TangoLLVMBuildSub)(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS,
      const char *Name);
  LLVMValueRef (*TangoLLVMBuildNSWSub)(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS,
      const char *Name);
  LLVMValueRef (*TangoLLVMBuildNUWSub)(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS,
      const char *Name);
  LLVMValueRef (*TangoLLVMBuildFSub)(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS,
      const char *Name);
  LLVMValueRef (*TangoLLVMBuildMul)(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS,
      const char *Name);
  LLVMValueRef (*TangoLLVMBuildNSWMul)(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS,
      const char *Name);
  LLVMValueRef (*TangoLLVMBuildNUWMul)(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS,
      const char *Name);
  LLVMValueRef (*TangoLLVMBuildFMul)(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS,
      const char *Name);
  LLVMValueRef (*TangoLLVMBuildUDiv)(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS,
      const char *Name);
  LLVMValueRef (*TangoLLVMBuildExactUDiv)(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS,
      const char *Name);
  LLVMValueRef (*TangoLLVMBuildSDiv)(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS,
      const char *Name);
  LLVMValueRef (*TangoLLVMBuildExactSDiv)(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS,
      const char *Name);
  LLVMValueRef (*TangoLLVMBuildFDiv)(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS,
      const char *Name);
  LLVMValueRef (*TangoLLVMBuildURem)(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS,
      const char *Name);
  LLVMValueRef (*TangoLLVMBuildSRem)(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS,
      const char *Name);
  LLVMValueRef (*TangoLLVMBuildFRem)(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS,
      const char *Name);
  LLVMValueRef (*TangoLLVMBuildShl)(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS,
      const char *Name);
  LLVMValueRef (*TangoLLVMBuildLShr)(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS,
      const char *Name);
  LLVMValueRef (*TangoLLVMBuildAShr)(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS,
      const char *Name);
  LLVMValueRef (*TangoLLVMBuildAnd)(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS,
      const char *Name);
  LLVMValueRef (*TangoLLVMBuildOr)(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS,
      const char *Name);
  LLVMValueRef (*TangoLLVMBuildXor)(LLVMBuilderRef, LLVMValueRef LHS, LLVMValueRef RHS,
      const char *Name);
  LLVMValueRef (*TangoLLVMBuildBinOp)(LLVMBuilderRef B, LLVMOpcode Op,
      LLVMValueRef LHS, LLVMValueRef RHS,
      const char *Name);
  LLVMValueRef (*TangoLLVMBuildNeg)(LLVMBuilderRef, LLVMValueRef V, const char *Name);
  LLVMValueRef (*TangoLLVMBuildNSWNeg)(LLVMBuilderRef B, LLVMValueRef V,
      const char *Name);
  LLVMValueRef (*TangoLLVMBuildNUWNeg)(LLVMBuilderRef B,
        LLVMValueRef V, const char *Name);
  LLVMValueRef (*TangoLLVMBuildFNeg)(LLVMBuilderRef, LLVMValueRef V, const char *Name);
  LLVMValueRef (*TangoLLVMBuildNot)(LLVMBuilderRef, LLVMValueRef V, const char *Name);

  LLVMBool (*TangoLLVMGetNUW)(LLVMValueRef ArithInst);
  void (*TangoLLVMSetNUW)(LLVMValueRef ArithInst, LLVMBool HasNUW);
  LLVMBool (*TangoLLVMGetNSW)(LLVMValueRef ArithInst);
  void (*TangoLLVMSetNSW)(LLVMValueRef ArithInst, LLVMBool HasNSW);
  LLVMBool (*TangoLLVMGetExact)(LLVMValueRef DivOrShrInst);
  void (*TangoLLVMSetExact)(LLVMValueRef DivOrShrInst, LLVMBool IsExact);

  LLVMBool (*TangoLLVMGetNNeg)(LLVMValueRef NonNegInst);

  void (*TangoLLVMSetNNeg)(LLVMValueRef NonNegInst, LLVMBool IsNonNeg);

  LLVMFastMathFlags (*TangoLLVMGetFastMathFlags)(LLVMValueRef FPMathInst);

  void (*TangoLLVMSetFastMathFlags)(LLVMValueRef FPMathInst, LLVMFastMathFlags FMF);

  LLVMBool (*TangoLLVMCanValueUseFastMathFlags)(LLVMValueRef Inst);

  LLVMBool (*TangoLLVMGetIsDisjoint)(LLVMValueRef Inst);

  void (*TangoLLVMSetIsDisjoint)(LLVMValueRef Inst, LLVMBool IsDisjoint);

  LLVMValueRef (*TangoLLVMBuildMalloc)(LLVMBuilderRef, LLVMTypeRef Ty, const char *Name);
  LLVMValueRef (*TangoLLVMBuildArrayMalloc)(LLVMBuilderRef, LLVMTypeRef Ty,
      LLVMValueRef Val, const char *Name);

  LLVMValueRef (*TangoLLVMBuildMemSet)(LLVMBuilderRef B, LLVMValueRef Ptr,
      LLVMValueRef Val, LLVMValueRef Len,
      unsigned Align);

  LLVMValueRef (*TangoLLVMBuildMemCpy)(LLVMBuilderRef B,
      LLVMValueRef Dst, unsigned DstAlign,
      LLVMValueRef Src, unsigned SrcAlign,
      LLVMValueRef Size);

  LLVMValueRef (*TangoLLVMBuildMemMove)(LLVMBuilderRef B,
      LLVMValueRef Dst, unsigned DstAlign,
      LLVMValueRef Src, unsigned SrcAlign,
      LLVMValueRef Size);

  LLVMValueRef (*TangoLLVMBuildAlloca)(LLVMBuilderRef, LLVMTypeRef Ty, const char *Name);
  LLVMValueRef (*TangoLLVMBuildArrayAlloca)(LLVMBuilderRef, LLVMTypeRef Ty,
      LLVMValueRef Val, const char *Name);
  LLVMValueRef (*TangoLLVMBuildFree)(LLVMBuilderRef, LLVMValueRef PointerVal);
  LLVMValueRef (*TangoLLVMBuildLoad2)(LLVMBuilderRef, LLVMTypeRef Ty,
      LLVMValueRef PointerVal, const char *Name);
  LLVMValueRef (*TangoLLVMBuildStore)(LLVMBuilderRef, LLVMValueRef Val, LLVMValueRef Ptr);
  LLVMValueRef (*TangoLLVMBuildGEP2)(LLVMBuilderRef B, LLVMTypeRef Ty,
      LLVMValueRef Pointer, LLVMValueRef *Indices,
      unsigned NumIndices, const char *Name);
  LLVMValueRef (*TangoLLVMBuildInBoundsGEP2)(LLVMBuilderRef B, LLVMTypeRef Ty,
      LLVMValueRef Pointer, LLVMValueRef *Indices,
      unsigned NumIndices, const char *Name);

  LLVMValueRef (*TangoLLVMBuildGEPWithNoWrapFlags)(LLVMBuilderRef B, LLVMTypeRef Ty,
      LLVMValueRef Pointer,
      LLVMValueRef *Indices,
      unsigned NumIndices, const char *Name,
      LLVMGEPNoWrapFlags NoWrapFlags);
  LLVMValueRef (*TangoLLVMBuildStructGEP2)(LLVMBuilderRef B, LLVMTypeRef Ty,
      LLVMValueRef Pointer, unsigned Idx,
      const char *Name);
  LLVMValueRef (*TangoLLVMBuildGlobalString)(LLVMBuilderRef B, const char *Str,
      const char *Name);
  LLVMValueRef (*TangoLLVMBuildGlobalStringPtr)(LLVMBuilderRef B, const char *Str,
      const char *Name);
  LLVMBool (*TangoLLVMGetVolatile)(LLVMValueRef MemoryAccessInst);
  void (*TangoLLVMSetVolatile)(LLVMValueRef MemoryAccessInst, LLVMBool IsVolatile);
  LLVMBool (*TangoLLVMGetWeak)(LLVMValueRef CmpXchgInst);
  void (*TangoLLVMSetWeak)(LLVMValueRef CmpXchgInst, LLVMBool IsWeak);
  LLVMAtomicOrdering (*TangoLLVMGetOrdering)(LLVMValueRef MemoryAccessInst);
  void (*TangoLLVMSetOrdering)(LLVMValueRef MemoryAccessInst, LLVMAtomicOrdering Ordering);
  LLVMAtomicRMWBinOp (*TangoLLVMGetAtomicRMWBinOp)(LLVMValueRef AtomicRMWInst);
  void (*TangoLLVMSetAtomicRMWBinOp)(LLVMValueRef AtomicRMWInst, LLVMAtomicRMWBinOp BinOp);

  LLVMValueRef (*TangoLLVMBuildTrunc)(LLVMBuilderRef, LLVMValueRef Val,
      LLVMTypeRef DestTy, const char *Name);
  LLVMValueRef (*TangoLLVMBuildZExt)(LLVMBuilderRef, LLVMValueRef Val,
      LLVMTypeRef DestTy, const char *Name);
  LLVMValueRef (*TangoLLVMBuildSExt)(LLVMBuilderRef, LLVMValueRef Val,
      LLVMTypeRef DestTy, const char *Name);
  LLVMValueRef (*TangoLLVMBuildFPToUI)(LLVMBuilderRef, LLVMValueRef Val,
      LLVMTypeRef DestTy, const char *Name);
  LLVMValueRef (*TangoLLVMBuildFPToSI)(LLVMBuilderRef, LLVMValueRef Val,
      LLVMTypeRef DestTy, const char *Name);
  LLVMValueRef (*TangoLLVMBuildUIToFP)(LLVMBuilderRef, LLVMValueRef Val,
      LLVMTypeRef DestTy, const char *Name);
  LLVMValueRef (*TangoLLVMBuildSIToFP)(LLVMBuilderRef, LLVMValueRef Val,
      LLVMTypeRef DestTy, const char *Name);
  LLVMValueRef (*TangoLLVMBuildFPTrunc)(LLVMBuilderRef, LLVMValueRef Val,
      LLVMTypeRef DestTy, const char *Name);
  LLVMValueRef (*TangoLLVMBuildFPExt)(LLVMBuilderRef, LLVMValueRef Val,
      LLVMTypeRef DestTy, const char *Name);
  LLVMValueRef (*TangoLLVMBuildPtrToInt)(LLVMBuilderRef, LLVMValueRef Val,
      LLVMTypeRef DestTy, const char *Name);
  LLVMValueRef (*TangoLLVMBuildIntToPtr)(LLVMBuilderRef, LLVMValueRef Val,
      LLVMTypeRef DestTy, const char *Name);
  LLVMValueRef (*TangoLLVMBuildBitCast)(LLVMBuilderRef, LLVMValueRef Val,
      LLVMTypeRef DestTy, const char *Name);
  LLVMValueRef (*TangoLLVMBuildAddrSpaceCast)(LLVMBuilderRef, LLVMValueRef Val,
      LLVMTypeRef DestTy, const char *Name);
  LLVMValueRef (*TangoLLVMBuildZExtOrBitCast)(LLVMBuilderRef, LLVMValueRef Val,
      LLVMTypeRef DestTy, const char *Name);
  LLVMValueRef (*TangoLLVMBuildSExtOrBitCast)(LLVMBuilderRef, LLVMValueRef Val,
      LLVMTypeRef DestTy, const char *Name);
  LLVMValueRef (*TangoLLVMBuildTruncOrBitCast)(LLVMBuilderRef, LLVMValueRef Val,
      LLVMTypeRef DestTy, const char *Name);
  LLVMValueRef (*TangoLLVMBuildCast)(LLVMBuilderRef B, LLVMOpcode Op, LLVMValueRef Val,
      LLVMTypeRef DestTy, const char *Name);
  LLVMValueRef (*TangoLLVMBuildPointerCast)(LLVMBuilderRef, LLVMValueRef Val,
      LLVMTypeRef DestTy, const char *Name);
  LLVMValueRef (*TangoLLVMBuildIntCast2)(LLVMBuilderRef, LLVMValueRef Val,
      LLVMTypeRef DestTy, LLVMBool IsSigned,
      const char *Name);
  LLVMValueRef (*TangoLLVMBuildFPCast)(LLVMBuilderRef, LLVMValueRef Val,
      LLVMTypeRef DestTy, const char *Name);

  LLVMValueRef (*TangoLLVMBuildIntCast)(LLVMBuilderRef, LLVMValueRef Val,
      LLVMTypeRef DestTy, const char *Name);

  LLVMOpcode (*TangoLLVMGetCastOpcode)(LLVMValueRef Src, LLVMBool SrcIsSigned,
      LLVMTypeRef DestTy, LLVMBool DestIsSigned);

  LLVMValueRef (*TangoLLVMBuildICmp)(LLVMBuilderRef, LLVMIntPredicate Op,
      LLVMValueRef LHS, LLVMValueRef RHS,
      const char *Name);
  LLVMValueRef (*TangoLLVMBuildFCmp)(LLVMBuilderRef, LLVMRealPredicate Op,
      LLVMValueRef LHS, LLVMValueRef RHS,
      const char *Name);

  LLVMValueRef (*TangoLLVMBuildPhi)(LLVMBuilderRef, LLVMTypeRef Ty, const char *Name);
  LLVMValueRef (*TangoLLVMBuildCall2)(LLVMBuilderRef, LLVMTypeRef, LLVMValueRef Fn,
      LLVMValueRef *Args, unsigned NumArgs,
      const char *Name);
  LLVMValueRef (*TangoLLVMBuildCallWithOperandBundles)(LLVMBuilderRef, LLVMTypeRef, LLVMValueRef Fn,
        LLVMValueRef *Args, unsigned NumArgs,
        LLVMOperandBundleRef *Bundles,
        unsigned NumBundles, const char *Name);
  LLVMValueRef (*TangoLLVMBuildSelect)(LLVMBuilderRef, LLVMValueRef If,
      LLVMValueRef Then, LLVMValueRef Else,
      const char *Name);
  LLVMValueRef (*TangoLLVMBuildVAArg)(LLVMBuilderRef, LLVMValueRef List, LLVMTypeRef Ty,
      const char *Name);
  LLVMValueRef (*TangoLLVMBuildExtractElement)(LLVMBuilderRef, LLVMValueRef VecVal,
      LLVMValueRef Index, const char *Name);
  LLVMValueRef (*TangoLLVMBuildInsertElement)(LLVMBuilderRef, LLVMValueRef VecVal,
      LLVMValueRef EltVal, LLVMValueRef Index,
      const char *Name);
  LLVMValueRef (*TangoLLVMBuildShuffleVector)(LLVMBuilderRef, LLVMValueRef V1,
      LLVMValueRef V2, LLVMValueRef Mask,
      const char *Name);
  LLVMValueRef (*TangoLLVMBuildExtractValue)(LLVMBuilderRef, LLVMValueRef AggVal,
      unsigned Index, const char *Name);
  LLVMValueRef (*TangoLLVMBuildInsertValue)(LLVMBuilderRef, LLVMValueRef AggVal,
      LLVMValueRef EltVal, unsigned Index,
      const char *Name);
  LLVMValueRef (*TangoLLVMBuildFreeze)(LLVMBuilderRef, LLVMValueRef Val,
      const char *Name);

  LLVMValueRef (*TangoLLVMBuildIsNull)(LLVMBuilderRef, LLVMValueRef Val,
      const char *Name);
  LLVMValueRef (*TangoLLVMBuildIsNotNull)(LLVMBuilderRef, LLVMValueRef Val,
      const char *Name);
  LLVMValueRef (*TangoLLVMBuildPtrDiff2)(LLVMBuilderRef, LLVMTypeRef ElemTy,
      LLVMValueRef LHS, LLVMValueRef RHS,
      const char *Name);
  LLVMValueRef (*TangoLLVMBuildFence)(LLVMBuilderRef B, LLVMAtomicOrdering ordering,
      LLVMBool singleThread, const char *Name);
  LLVMValueRef (*TangoLLVMBuildAtomicRMW)(LLVMBuilderRef B, LLVMAtomicRMWBinOp op,
      LLVMValueRef PTR, LLVMValueRef Val,
      LLVMAtomicOrdering ordering,
      LLVMBool singleThread);
  LLVMValueRef (*TangoLLVMBuildAtomicCmpXchg)(LLVMBuilderRef B, LLVMValueRef Ptr,
      LLVMValueRef Cmp, LLVMValueRef New,
      LLVMAtomicOrdering SuccessOrdering,
      LLVMAtomicOrdering FailureOrdering,
      LLVMBool SingleThread);

  unsigned (*TangoLLVMGetNumMaskElements)(LLVMValueRef ShuffleVectorInst);

  int (*TangoLLVMGetUndefMaskElem)(void);

  int (*TangoLLVMGetMaskValue)(LLVMValueRef ShuffleVectorInst, unsigned Elt);

  LLVMBool (*TangoLLVMIsAtomicSingleThread)(LLVMValueRef AtomicInst);
  void (*TangoLLVMSetAtomicSingleThread)(LLVMValueRef AtomicInst, LLVMBool SingleThread);

  LLVMAtomicOrdering (*TangoLLVMGetCmpXchgSuccessOrdering)(LLVMValueRef CmpXchgInst);
  void (*TangoLLVMSetCmpXchgSuccessOrdering)(LLVMValueRef CmpXchgInst,
      LLVMAtomicOrdering Ordering);
  LLVMAtomicOrdering (*TangoLLVMGetCmpXchgFailureOrdering)(LLVMValueRef CmpXchgInst);
  void (*TangoLLVMSetCmpXchgFailureOrdering)(LLVMValueRef CmpXchgInst,
      LLVMAtomicOrdering Ordering);

  LLVMModuleProviderRef (*TangoLLVMCreateModuleProviderForExistingModule)(LLVMModuleRef M);

  void (*TangoLLVMDisposeModuleProvider)(LLVMModuleProviderRef M);

  LLVMBool (*TangoLLVMCreateMemoryBufferWithContentsOfFile)(const char *Path,
      LLVMMemoryBufferRef *OutMemBuf,
      char **OutMessage);
  LLVMBool (*TangoLLVMCreateMemoryBufferWithSTDIN)(LLVMMemoryBufferRef *OutMemBuf,
      char **OutMessage);
  LLVMMemoryBufferRef (*TangoLLVMCreateMemoryBufferWithMemoryRange)(const char *InputData,
      size_t InputDataLength,
      const char *BufferName,
      LLVMBool RequiresNullTerminator);
  LLVMMemoryBufferRef (*TangoLLVMCreateMemoryBufferWithMemoryRangeCopy)(const char *InputData,
      size_t InputDataLength,
      const char *BufferName);
  const char *(*TangoLLVMGetBufferStart)(LLVMMemoryBufferRef MemBuf);
  size_t (*TangoLLVMGetBufferSize)(LLVMMemoryBufferRef MemBuf);
  void (*TangoLLVMDisposeMemoryBuffer)(LLVMMemoryBufferRef MemBuf);

  LLVMPassManagerRef (*TangoLLVMCreatePassManager)(void);

  LLVMPassManagerRef (*TangoLLVMCreateFunctionPassManagerForModule)(LLVMModuleRef M);

  LLVMPassManagerRef (*TangoLLVMCreateFunctionPassManager)(LLVMModuleProviderRef MP);

  LLVMBool (*TangoLLVMRunPassManager)(LLVMPassManagerRef PM, LLVMModuleRef M);

  LLVMBool (*TangoLLVMInitializeFunctionPassManager)(LLVMPassManagerRef FPM);

  LLVMBool (*TangoLLVMRunFunctionPassManager)(LLVMPassManagerRef FPM, LLVMValueRef F);

  LLVMBool (*TangoLLVMFinalizeFunctionPassManager)(LLVMPassManagerRef FPM);

  void (*TangoLLVMDisposePassManager)(LLVMPassManagerRef PM);

  LLVMBool (*TangoLLVMStartMultithreaded)(void);

  void (*TangoLLVMStopMultithreaded)(void);

  LLVMBool (*TangoLLVMIsMultithreaded)(void);

  // from llvm-c/BitWriter.h
  int (*TangoLLVMWriteBitcodeToFile)(LLVMModuleRef M, const char *Path);

  int (*TangoLLVMWriteBitcodeToFD)(LLVMModuleRef M, int FD, int ShouldClose,
      int Unbuffered);

  int (*TangoLLVMWriteBitcodeToFileHandle)(LLVMModuleRef M, int Handle);

  LLVMMemoryBufferRef (*TangoLLVMWriteBitcodeToMemoryBuffer)(LLVMModuleRef M);

  // from llvm-c/BitReader.h
  LLVMBool (*TangoLLVMParseBitcode)(LLVMMemoryBufferRef MemBuf, LLVMModuleRef *OutModule,
      char **OutMessage);

  LLVMBool (*TangoLLVMParseBitcode2)(LLVMMemoryBufferRef MemBuf,
      LLVMModuleRef *OutModule);

  LLVMBool (*TangoLLVMParseBitcodeInContext)(LLVMContextRef ContextRef,
      LLVMMemoryBufferRef MemBuf,
      LLVMModuleRef *OutModule, char **OutMessage);

  LLVMBool (*TangoLLVMParseBitcodeInContext2)(LLVMContextRef ContextRef,
      LLVMMemoryBufferRef MemBuf,
      LLVMModuleRef *OutModule);

  LLVMBool (*TangoLLVMGetBitcodeModuleInContext)(LLVMContextRef ContextRef,
      LLVMMemoryBufferRef MemBuf,
      LLVMModuleRef *OutM, char **OutMessage);

  LLVMBool (*TangoLLVMGetBitcodeModuleInContext2)(LLVMContextRef ContextRef,
      LLVMMemoryBufferRef MemBuf,
      LLVMModuleRef *OutM);

  LLVMBool (*TangoLLVMGetBitcodeModule)(LLVMMemoryBufferRef MemBuf, LLVMModuleRef *OutM,
      char **OutMessage);

  LLVMBool (*TangoLLVMGetBitcodeModule2)(LLVMMemoryBufferRef MemBuf, LLVMModuleRef *OutM);

  // from llvm-c/Analysis.h
  LLVMBool (*TangoLLVMVerifyModule)(LLVMModuleRef M, LLVMVerifierFailureAction Action,
      char **OutMessage);

  LLVMBool (*TangoLLVMVerifyFunction)(LLVMValueRef Fn, LLVMVerifierFailureAction Action);

  void (*TangoLLVMViewFunctionCFG)(LLVMValueRef Fn);
  void (*TangoLLVMViewFunctionCFGOnly)(LLVMValueRef Fn);

  // from llvm-c/DebugInfo.h
  unsigned (*TangoLLVMDebugMetadataVersion)(void);

  unsigned (*TangoLLVMGetModuleDebugMetadataVersion)(LLVMModuleRef Module);

  LLVMBool (*TangoLLVMStripModuleDebugInfo)(LLVMModuleRef Module);

  LLVMDIBuilderRef (*TangoLLVMCreateDIBuilderDisallowUnresolved)(LLVMModuleRef M);

  LLVMDIBuilderRef (*TangoLLVMCreateDIBuilder)(LLVMModuleRef M);

  void (*TangoLLVMDisposeDIBuilder)(LLVMDIBuilderRef Builder);

  void (*TangoLLVMDIBuilderFinalize)(LLVMDIBuilderRef Builder);

  void (*TangoLLVMDIBuilderFinalizeSubprogram)(LLVMDIBuilderRef Builder,
      LLVMMetadataRef Subprogram);

  LLVMMetadataRef (*TangoLLVMDIBuilderCreateCompileUnit)(
      LLVMDIBuilderRef Builder, LLVMDWARFSourceLanguage Lang,
      LLVMMetadataRef FileRef, const char *Producer, size_t ProducerLen,
      LLVMBool isOptimized, const char *Flags, size_t FlagsLen,
      unsigned RuntimeVer, const char *SplitName, size_t SplitNameLen,
      LLVMDWARFEmissionKind Kind, unsigned DWOId, LLVMBool SplitDebugInlining,
      LLVMBool DebugInfoForProfiling, const char *SysRoot, size_t SysRootLen,
      const char *SDK, size_t SDKLen);

  LLVMMetadataRef (*TangoLLVMDIBuilderCreateFile)(LLVMDIBuilderRef Builder, const char *Filename,
        size_t FilenameLen, const char *Directory,
        size_t DirectoryLen);

  LLVMMetadataRef (*TangoLLVMDIBuilderCreateModule)(LLVMDIBuilderRef Builder, LLVMMetadataRef ParentScope,
        const char *Name, size_t NameLen,
        const char *ConfigMacros, size_t ConfigMacrosLen,
        const char *IncludePath, size_t IncludePathLen,
        const char *APINotesFile, size_t APINotesFileLen);

  LLVMMetadataRef (*TangoLLVMDIBuilderCreateNameSpace)(LLVMDIBuilderRef Builder,
        LLVMMetadataRef ParentScope,
        const char *Name, size_t NameLen,
        LLVMBool ExportSymbols);

  LLVMMetadataRef (*TangoLLVMDIBuilderCreateFunction)(
      LLVMDIBuilderRef Builder, LLVMMetadataRef Scope, const char *Name,
      size_t NameLen, const char *LinkageName, size_t LinkageNameLen,
      LLVMMetadataRef File, unsigned LineNo, LLVMMetadataRef Ty,
      LLVMBool IsLocalToUnit, LLVMBool IsDefinition,
      unsigned ScopeLine, LLVMDIFlags Flags, LLVMBool IsOptimized);

  LLVMMetadataRef (*TangoLLVMDIBuilderCreateLexicalBlock)(
      LLVMDIBuilderRef Builder, LLVMMetadataRef Scope,
      LLVMMetadataRef File, unsigned Line, unsigned Column);

  LLVMMetadataRef (*TangoLLVMDIBuilderCreateLexicalBlockFile)(LLVMDIBuilderRef Builder,
        LLVMMetadataRef Scope,
        LLVMMetadataRef File,
        unsigned Discriminator);

  LLVMMetadataRef (*TangoLLVMDIBuilderCreateImportedModuleFromNamespace)(LLVMDIBuilderRef Builder,
        LLVMMetadataRef Scope,
        LLVMMetadataRef NS,
        LLVMMetadataRef File,
        unsigned Line);

  LLVMMetadataRef (*TangoLLVMDIBuilderCreateImportedModuleFromAlias)(
      LLVMDIBuilderRef Builder, LLVMMetadataRef Scope,
      LLVMMetadataRef ImportedEntity, LLVMMetadataRef File, unsigned Line,
      LLVMMetadataRef *Elements, unsigned NumElements);

  LLVMMetadataRef (*TangoLLVMDIBuilderCreateImportedModuleFromModule)(
      LLVMDIBuilderRef Builder, LLVMMetadataRef Scope, LLVMMetadataRef M,
      LLVMMetadataRef File, unsigned Line, LLVMMetadataRef *Elements,
      unsigned NumElements);

  LLVMMetadataRef (*TangoLLVMDIBuilderCreateImportedDeclaration)(
      LLVMDIBuilderRef Builder, LLVMMetadataRef Scope, LLVMMetadataRef Decl,
      LLVMMetadataRef File, unsigned Line, const char *Name, size_t NameLen,
      LLVMMetadataRef *Elements, unsigned NumElements);

  LLVMMetadataRef (*TangoLLVMDIBuilderCreateDebugLocation)(LLVMContextRef Ctx, unsigned Line,
        unsigned Column, LLVMMetadataRef Scope,
        LLVMMetadataRef InlinedAt);

  unsigned (*TangoLLVMDILocationGetLine)(LLVMMetadataRef Location);

  unsigned (*TangoLLVMDILocationGetColumn)(LLVMMetadataRef Location);

  LLVMMetadataRef (*TangoLLVMDILocationGetScope)(LLVMMetadataRef Location);

  LLVMMetadataRef (*TangoLLVMDILocationGetInlinedAt)(LLVMMetadataRef Location);

  LLVMMetadataRef (*TangoLLVMDIScopeGetFile)(LLVMMetadataRef Scope);

  const char *(*TangoLLVMDIFileGetDirectory)(LLVMMetadataRef File, unsigned *Len);

  const char *(*TangoLLVMDIFileGetFilename)(LLVMMetadataRef File, unsigned *Len);

  const char *(*TangoLLVMDIFileGetSource)(LLVMMetadataRef File, unsigned *Len);

  LLVMMetadataRef (*TangoLLVMDIBuilderGetOrCreateTypeArray)(LLVMDIBuilderRef Builder,
      LLVMMetadataRef *Data,
      size_t NumElements);

  LLVMMetadataRef (*TangoLLVMDIBuilderCreateSubroutineType)(LLVMDIBuilderRef Builder,
        LLVMMetadataRef File,
        LLVMMetadataRef *ParameterTypes,
        unsigned NumParameterTypes,
        LLVMDIFlags Flags);

  LLVMMetadataRef (*TangoLLVMDIBuilderCreateMacro)(LLVMDIBuilderRef Builder,
      LLVMMetadataRef ParentMacroFile,
      unsigned Line,
      LLVMDWARFMacinfoRecordType RecordType,
      const char *Name, size_t NameLen,
      const char *Value, size_t ValueLen);

  LLVMMetadataRef (*TangoLLVMDIBuilderCreateTempMacroFile)(LLVMDIBuilderRef Builder,
        LLVMMetadataRef ParentMacroFile, unsigned Line,
        LLVMMetadataRef File);

  LLVMMetadataRef (*TangoLLVMDIBuilderCreateEnumerator)(LLVMDIBuilderRef Builder,
      const char *Name, size_t NameLen,
      int64_t Value,
      LLVMBool IsUnsigned);

  LLVMMetadataRef (*TangoLLVMDIBuilderCreateEnumerationType)(
      LLVMDIBuilderRef Builder, LLVMMetadataRef Scope, const char *Name,
      size_t NameLen, LLVMMetadataRef File, unsigned LineNumber,
      uint64_t SizeInBits, uint32_t AlignInBits, LLVMMetadataRef *Elements,
      unsigned NumElements, LLVMMetadataRef ClassTy);

  LLVMMetadataRef (*TangoLLVMDIBuilderCreateUnionType)(
      LLVMDIBuilderRef Builder, LLVMMetadataRef Scope, const char *Name,
      size_t NameLen, LLVMMetadataRef File, unsigned LineNumber,
      uint64_t SizeInBits, uint32_t AlignInBits, LLVMDIFlags Flags,
      LLVMMetadataRef *Elements, unsigned NumElements, unsigned RunTimeLang,
      const char *UniqueId, size_t UniqueIdLen);

  LLVMMetadataRef (*TangoLLVMDIBuilderCreateArrayType)(LLVMDIBuilderRef Builder, uint64_t Size,
        uint32_t AlignInBits, LLVMMetadataRef Ty,
        LLVMMetadataRef *Subscripts,
        unsigned NumSubscripts);

  LLVMMetadataRef (*TangoLLVMDIBuilderCreateVectorType)(LLVMDIBuilderRef Builder, uint64_t Size,
        uint32_t AlignInBits, LLVMMetadataRef Ty,
        LLVMMetadataRef *Subscripts,
        unsigned NumSubscripts);

  LLVMMetadataRef (*TangoLLVMDIBuilderCreateUnspecifiedType)(LLVMDIBuilderRef Builder, const char *Name,
        size_t NameLen);

  LLVMMetadataRef (*TangoLLVMDIBuilderCreateBasicType)(LLVMDIBuilderRef Builder, const char *Name,
        size_t NameLen, uint64_t SizeInBits,
        LLVMDWARFTypeEncoding Encoding,
        LLVMDIFlags Flags);

  LLVMMetadataRef (*TangoLLVMDIBuilderCreatePointerType)(
      LLVMDIBuilderRef Builder, LLVMMetadataRef PointeeTy,
      uint64_t SizeInBits, uint32_t AlignInBits, unsigned AddressSpace,
      const char *Name, size_t NameLen);

  LLVMMetadataRef (*TangoLLVMDIBuilderCreateStructType)(
      LLVMDIBuilderRef Builder, LLVMMetadataRef Scope, const char *Name,
      size_t NameLen, LLVMMetadataRef File, unsigned LineNumber,
      uint64_t SizeInBits, uint32_t AlignInBits, LLVMDIFlags Flags,
      LLVMMetadataRef DerivedFrom, LLVMMetadataRef *Elements,
      unsigned NumElements, unsigned RunTimeLang, LLVMMetadataRef VTableHolder,
      const char *UniqueId, size_t UniqueIdLen);

  LLVMMetadataRef (*TangoLLVMDIBuilderCreateMemberType)(
      LLVMDIBuilderRef Builder, LLVMMetadataRef Scope, const char *Name,
      size_t NameLen, LLVMMetadataRef File, unsigned LineNo,
      uint64_t SizeInBits, uint32_t AlignInBits, uint64_t OffsetInBits,
      LLVMDIFlags Flags, LLVMMetadataRef Ty);

  LLVMMetadataRef (*TangoLLVMDIBuilderCreateStaticMemberType)(
        LLVMDIBuilderRef Builder, LLVMMetadataRef Scope, const char *Name,
        size_t NameLen, LLVMMetadataRef File, unsigned LineNumber,
        LLVMMetadataRef Type, LLVMDIFlags Flags, LLVMValueRef ConstantVal,
        uint32_t AlignInBits);

  LLVMMetadataRef (*TangoLLVMDIBuilderCreateMemberPointerType)(LLVMDIBuilderRef Builder,
        LLVMMetadataRef PointeeType,
        LLVMMetadataRef ClassType,
        uint64_t SizeInBits,
        uint32_t AlignInBits,
        LLVMDIFlags Flags);

  LLVMMetadataRef (*TangoLLVMDIBuilderCreateObjCIVar)(LLVMDIBuilderRef Builder,
        const char *Name, size_t NameLen,
        LLVMMetadataRef File, unsigned LineNo,
        uint64_t SizeInBits, uint32_t AlignInBits,
        uint64_t OffsetInBits, LLVMDIFlags Flags,
        LLVMMetadataRef Ty, LLVMMetadataRef PropertyNode);

  LLVMMetadataRef (*TangoLLVMDIBuilderCreateObjCProperty)(LLVMDIBuilderRef Builder,
        const char *Name, size_t NameLen,
        LLVMMetadataRef File, unsigned LineNo,
        const char *GetterName, size_t GetterNameLen,
        const char *SetterName, size_t SetterNameLen,
        unsigned PropertyAttributes,
        LLVMMetadataRef Ty);

  LLVMMetadataRef (*TangoLLVMDIBuilderCreateObjectPointerType)(LLVMDIBuilderRef Builder,
        LLVMMetadataRef Type);

  LLVMMetadataRef (*TangoLLVMDIBuilderCreateQualifiedType)(LLVMDIBuilderRef Builder, unsigned Tag,
        LLVMMetadataRef Type);

  LLVMMetadataRef (*TangoLLVMDIBuilderCreateReferenceType)(LLVMDIBuilderRef Builder, unsigned Tag,
        LLVMMetadataRef Type);

  LLVMMetadataRef (*TangoLLVMDIBuilderCreateNullPtrType)(LLVMDIBuilderRef Builder);

  LLVMMetadataRef (*TangoLLVMDIBuilderCreateTypedef)(LLVMDIBuilderRef Builder, LLVMMetadataRef Type,
        const char *Name, size_t NameLen,
        LLVMMetadataRef File, unsigned LineNo,
        LLVMMetadataRef Scope, uint32_t AlignInBits);

  LLVMMetadataRef (*TangoLLVMDIBuilderCreateInheritance)(LLVMDIBuilderRef Builder,
        LLVMMetadataRef Ty, LLVMMetadataRef BaseTy,
        uint64_t BaseOffset, uint32_t VBPtrOffset,
        LLVMDIFlags Flags);

  LLVMMetadataRef (*TangoLLVMDIBuilderCreateForwardDecl)(
      LLVMDIBuilderRef Builder, unsigned Tag, const char *Name,
      size_t NameLen, LLVMMetadataRef Scope, LLVMMetadataRef File, unsigned Line,
      unsigned RuntimeLang, uint64_t SizeInBits, uint32_t AlignInBits,
      const char *UniqueIdentifier, size_t UniqueIdentifierLen);

  LLVMMetadataRef (*TangoLLVMDIBuilderCreateReplaceableCompositeType)(
        LLVMDIBuilderRef Builder, unsigned Tag, const char *Name,
        size_t NameLen, LLVMMetadataRef Scope, LLVMMetadataRef File, unsigned Line,
        unsigned RuntimeLang, uint64_t SizeInBits, uint32_t AlignInBits,
        LLVMDIFlags Flags, const char *UniqueIdentifier,
        size_t UniqueIdentifierLen);

  LLVMMetadataRef (*TangoLLVMDIBuilderCreateBitFieldMemberType)(LLVMDIBuilderRef Builder,
        LLVMMetadataRef Scope,
        const char *Name, size_t NameLen,
        LLVMMetadataRef File, unsigned LineNumber,
        uint64_t SizeInBits,
        uint64_t OffsetInBits,
        uint64_t StorageOffsetInBits,
        LLVMDIFlags Flags, LLVMMetadataRef Type);

  LLVMMetadataRef (*TangoLLVMDIBuilderCreateClassType)(LLVMDIBuilderRef Builder,
      LLVMMetadataRef Scope, const char *Name, size_t NameLen,
      LLVMMetadataRef File, unsigned LineNumber, uint64_t SizeInBits,
      uint32_t AlignInBits, uint64_t OffsetInBits, LLVMDIFlags Flags,
      LLVMMetadataRef DerivedFrom,
      LLVMMetadataRef *Elements, unsigned NumElements,
      LLVMMetadataRef VTableHolder, LLVMMetadataRef TemplateParamsNode,
      const char *UniqueIdentifier, size_t UniqueIdentifierLen);

  LLVMMetadataRef (*TangoLLVMDIBuilderCreateArtificialType)(LLVMDIBuilderRef Builder,
        LLVMMetadataRef Type);

  const char *(*TangoLLVMDITypeGetName)(LLVMMetadataRef DType, size_t *Length);

  uint64_t (*TangoLLVMDITypeGetSizeInBits)(LLVMMetadataRef DType);

  uint64_t (*TangoLLVMDITypeGetOffsetInBits)(LLVMMetadataRef DType);

  uint32_t (*TangoLLVMDITypeGetAlignInBits)(LLVMMetadataRef DType);

  unsigned (*TangoLLVMDITypeGetLine)(LLVMMetadataRef DType);

  LLVMDIFlags (*TangoLLVMDITypeGetFlags)(LLVMMetadataRef DType);

  LLVMMetadataRef (*TangoLLVMDIBuilderGetOrCreateSubrange)(LLVMDIBuilderRef Builder,
      int64_t LowerBound,
      int64_t Count);

  LLVMMetadataRef (*TangoLLVMDIBuilderGetOrCreateArray)(LLVMDIBuilderRef Builder,
      LLVMMetadataRef *Data,
      size_t NumElements);

  LLVMMetadataRef (*TangoLLVMDIBuilderCreateExpression)(LLVMDIBuilderRef Builder,
      uint64_t *Addr, size_t Length);

  LLVMMetadataRef (*TangoLLVMDIBuilderCreateConstantValueExpression)(LLVMDIBuilderRef Builder,
        uint64_t Value);

  LLVMMetadataRef (*TangoLLVMDIBuilderCreateGlobalVariableExpression)(
      LLVMDIBuilderRef Builder, LLVMMetadataRef Scope, const char *Name,
      size_t NameLen, const char *Linkage, size_t LinkLen, LLVMMetadataRef File,
      unsigned LineNo, LLVMMetadataRef Ty, LLVMBool LocalToUnit,
      LLVMMetadataRef Expr, LLVMMetadataRef Decl, uint32_t AlignInBits);

  uint16_t (*TangoLLVMGetDINodeTag)(LLVMMetadataRef MD);

  LLVMMetadataRef (*TangoLLVMDIGlobalVariableExpressionGetVariable)(LLVMMetadataRef GVE);

  LLVMMetadataRef (*TangoLLVMDIGlobalVariableExpressionGetExpression)(
      LLVMMetadataRef GVE);

  LLVMMetadataRef (*TangoLLVMDIVariableGetFile)(LLVMMetadataRef Var);

  LLVMMetadataRef (*TangoLLVMDIVariableGetScope)(LLVMMetadataRef Var);

  unsigned (*TangoLLVMDIVariableGetLine)(LLVMMetadataRef Var);

  LLVMMetadataRef (*TangoLLVMTemporaryMDNode)(LLVMContextRef Ctx, LLVMMetadataRef *Data,
      size_t NumElements);

  void (*TangoLLVMDisposeTemporaryMDNode)(LLVMMetadataRef TempNode);

  void (*TangoLLVMMetadataReplaceAllUsesWith)(LLVMMetadataRef TempTargetMetadata,
      LLVMMetadataRef Replacement);

  LLVMMetadataRef (*TangoLLVMDIBuilderCreateTempGlobalVariableFwdDecl)(
      LLVMDIBuilderRef Builder, LLVMMetadataRef Scope, const char *Name,
      size_t NameLen, const char *Linkage, size_t LnkLen, LLVMMetadataRef File,
      unsigned LineNo, LLVMMetadataRef Ty, LLVMBool LocalToUnit,
      LLVMMetadataRef Decl, uint32_t AlignInBits);

  LLVMDbgRecordRef (*TangoLLVMDIBuilderInsertDeclareRecordBefore)(
      LLVMDIBuilderRef Builder, LLVMValueRef Storage, LLVMMetadataRef VarInfo,
      LLVMMetadataRef Expr, LLVMMetadataRef DebugLoc, LLVMValueRef Instr);

  LLVMDbgRecordRef (*TangoLLVMDIBuilderInsertDeclareRecordAtEnd)(
      LLVMDIBuilderRef Builder, LLVMValueRef Storage, LLVMMetadataRef VarInfo,
      LLVMMetadataRef Expr, LLVMMetadataRef DebugLoc, LLVMBasicBlockRef Block);

  LLVMDbgRecordRef (*TangoLLVMDIBuilderInsertDbgValueRecordBefore)(
      LLVMDIBuilderRef Builder, LLVMValueRef Val, LLVMMetadataRef VarInfo,
      LLVMMetadataRef Expr, LLVMMetadataRef DebugLoc, LLVMValueRef Instr);

  LLVMDbgRecordRef (*TangoLLVMDIBuilderInsertDbgValueRecordAtEnd)(
      LLVMDIBuilderRef Builder, LLVMValueRef Val, LLVMMetadataRef VarInfo,
      LLVMMetadataRef Expr, LLVMMetadataRef DebugLoc, LLVMBasicBlockRef Block);

  LLVMMetadataRef (*TangoLLVMDIBuilderCreateAutoVariable)(
      LLVMDIBuilderRef Builder, LLVMMetadataRef Scope, const char *Name,
      size_t NameLen, LLVMMetadataRef File, unsigned LineNo, LLVMMetadataRef Ty,
      LLVMBool AlwaysPreserve, LLVMDIFlags Flags, uint32_t AlignInBits);

  LLVMMetadataRef (*TangoLLVMDIBuilderCreateParameterVariable)(
      LLVMDIBuilderRef Builder, LLVMMetadataRef Scope, const char *Name,
      size_t NameLen, unsigned ArgNo, LLVMMetadataRef File, unsigned LineNo,
      LLVMMetadataRef Ty, LLVMBool AlwaysPreserve, LLVMDIFlags Flags);

  LLVMMetadataRef (*TangoLLVMGetSubprogram)(LLVMValueRef Func);

  void (*TangoLLVMSetSubprogram)(LLVMValueRef Func, LLVMMetadataRef SP);

  unsigned (*TangoLLVMDISubprogramGetLine)(LLVMMetadataRef Subprogram);

  LLVMMetadataRef (*TangoLLVMInstructionGetDebugLoc)(LLVMValueRef Inst);

  void (*TangoLLVMInstructionSetDebugLoc)(LLVMValueRef Inst, LLVMMetadataRef Loc);

  LLVMMetadataKind (*TangoLLVMGetMetadataKind)(LLVMMetadataRef Metadata);

  // from llvm-c/Support.h
  LLVMBool (*TangoLLVMLoadLibraryPermanently)(const char* Filename);

  void (*TangoLLVMParseCommandLineOptions)(int argc, const char *const *argv,
      const char *Overview);

  void *(*TangoLLVMSearchForAddressOfSymbol)(const char *symbolName);

  void (*TangoLLVMAddSymbol)(const char *symbolName, void *symbolValue);

  // from llvm-c/Targets.h
  LLVMTargetDataRef (*TangoLLVMGetModuleDataLayout)(LLVMModuleRef M);

  void (*TangoLLVMSetModuleDataLayout)(LLVMModuleRef M, LLVMTargetDataRef DL);

  LLVMTargetDataRef (*TangoLLVMCreateTargetData)(const char *StringRep);

  void (*TangoLLVMDisposeTargetData)(LLVMTargetDataRef TD);

  void (*TangoLLVMAddTargetLibraryInfo)(LLVMTargetLibraryInfoRef TLI,
      LLVMPassManagerRef PM);

  char *(*TangoLLVMCopyStringRepOfTargetData)(LLVMTargetDataRef TD);

  enum LLVMByteOrdering (*TangoLLVMByteOrder)(LLVMTargetDataRef TD);

  unsigned (*TangoLLVMPointerSize)(LLVMTargetDataRef TD);

  unsigned (*TangoLLVMPointerSizeForAS)(LLVMTargetDataRef TD, unsigned AS);

  LLVMTypeRef (*TangoLLVMIntPtrType)(LLVMTargetDataRef TD);

  LLVMTypeRef (*TangoLLVMIntPtrTypeForAS)(LLVMTargetDataRef TD, unsigned AS);

  LLVMTypeRef (*TangoLLVMIntPtrTypeInContext)(LLVMContextRef C, LLVMTargetDataRef TD);

  LLVMTypeRef (*TangoLLVMIntPtrTypeForASInContext)(LLVMContextRef C, LLVMTargetDataRef TD,
      unsigned AS);

  unsigned long long (*TangoLLVMSizeOfTypeInBits)(LLVMTargetDataRef TD, LLVMTypeRef Ty);

  unsigned long long (*TangoLLVMStoreSizeOfType)(LLVMTargetDataRef TD, LLVMTypeRef Ty);

  unsigned long long (*TangoLLVMABISizeOfType)(LLVMTargetDataRef TD, LLVMTypeRef Ty);

  unsigned (*TangoLLVMABIAlignmentOfType)(LLVMTargetDataRef TD, LLVMTypeRef Ty);

  unsigned (*TangoLLVMCallFrameAlignmentOfType)(LLVMTargetDataRef TD, LLVMTypeRef Ty);

  unsigned (*TangoLLVMPreferredAlignmentOfType)(LLVMTargetDataRef TD, LLVMTypeRef Ty);

  unsigned (*TangoLLVMPreferredAlignmentOfGlobal)(LLVMTargetDataRef TD,
      LLVMValueRef GlobalVar);

  unsigned (*TangoLLVMElementAtOffset)(LLVMTargetDataRef TD, LLVMTypeRef StructTy,
      unsigned long long Offset);

  unsigned long long (*TangoLLVMOffsetOfElement)(LLVMTargetDataRef TD,
      LLVMTypeRef StructTy, unsigned Element);

  // from llvm-c/TargetMachine.h
  LLVMTargetRef (*TangoLLVMGetFirstTarget)(void);

  LLVMTargetRef (*TangoLLVMGetNextTarget)(LLVMTargetRef T);

  LLVMTargetRef (*TangoLLVMGetTargetFromName)(const char *Name);

  LLVMBool (*TangoLLVMGetTargetFromTriple)(const char* Triple, LLVMTargetRef *T,
      char **ErrorMessage);

  const char *(*TangoLLVMGetTargetName)(LLVMTargetRef T);

  const char *(*TangoLLVMGetTargetDescription)(LLVMTargetRef T);

  LLVMBool (*TangoLLVMTargetHasJIT)(LLVMTargetRef T);

  LLVMBool (*TangoLLVMTargetHasTargetMachine)(LLVMTargetRef T);

  LLVMBool (*TangoLLVMTargetHasAsmBackend)(LLVMTargetRef T);

  LLVMTargetMachineOptionsRef (*TangoLLVMCreateTargetMachineOptions)(void);

  void (*TangoLLVMDisposeTargetMachineOptions)(LLVMTargetMachineOptionsRef Options);

  void (*TangoLLVMTargetMachineOptionsSetCPU)(LLVMTargetMachineOptionsRef Options,
      const char *CPU);

  void (*TangoLLVMTargetMachineOptionsSetFeatures)(LLVMTargetMachineOptionsRef Options,
      const char *Features);

  void (*TangoLLVMTargetMachineOptionsSetABI)(LLVMTargetMachineOptionsRef Options,
      const char *ABI);

  void (*TangoLLVMTargetMachineOptionsSetCodeGenOptLevel)(LLVMTargetMachineOptionsRef Options, LLVMCodeGenOptLevel Level);

  void (*TangoLLVMTargetMachineOptionsSetRelocMode)(LLVMTargetMachineOptionsRef Options,
      LLVMRelocMode Reloc);

  void (*TangoLLVMTargetMachineOptionsSetCodeModel)(LLVMTargetMachineOptionsRef Options,
      LLVMCodeModel CodeModel);

  LLVMTargetMachineRef (*TangoLLVMCreateTargetMachineWithOptions)(LLVMTargetRef T, const char *Triple,
        LLVMTargetMachineOptionsRef Options);

  LLVMTargetMachineRef (*TangoLLVMCreateTargetMachine)(LLVMTargetRef T,
      const char *Triple, const char *CPU, const char *Features,
      LLVMCodeGenOptLevel Level, LLVMRelocMode Reloc, LLVMCodeModel CodeModel);

  void (*TangoLLVMDisposeTargetMachine)(LLVMTargetMachineRef T);

  LLVMTargetRef (*TangoLLVMGetTargetMachineTarget)(LLVMTargetMachineRef T);

  char *(*TangoLLVMGetTargetMachineTriple)(LLVMTargetMachineRef T);

  char *(*TangoLLVMGetTargetMachineCPU)(LLVMTargetMachineRef T);

  char *(*TangoLLVMGetTargetMachineFeatureString)(LLVMTargetMachineRef T);

  LLVMTargetDataRef (*TangoLLVMCreateTargetDataLayout)(LLVMTargetMachineRef T);

  void (*TangoLLVMSetTargetMachineAsmVerbosity)(LLVMTargetMachineRef T,
      LLVMBool VerboseAsm);

  void (*TangoLLVMSetTargetMachineFastISel)(LLVMTargetMachineRef T, LLVMBool Enable);

  void (*TangoLLVMSetTargetMachineGlobalISel)(LLVMTargetMachineRef T, LLVMBool Enable);

  void (*TangoLLVMSetTargetMachineGlobalISelAbort)(LLVMTargetMachineRef T,
      LLVMGlobalISelAbortMode Mode);

  void (*TangoLLVMSetTargetMachineMachineOutliner)(LLVMTargetMachineRef T,
      LLVMBool Enable);

  LLVMBool (*TangoLLVMTargetMachineEmitToFile)(LLVMTargetMachineRef T, LLVMModuleRef M,
      const char *Filename,
      LLVMCodeGenFileType codegen,
      char **ErrorMessage);

  LLVMBool (*TangoLLVMTargetMachineEmitToMemoryBuffer)(LLVMTargetMachineRef T, LLVMModuleRef M,
      LLVMCodeGenFileType codegen, char** ErrorMessage, LLVMMemoryBufferRef *OutMemBuf);

  char* (*TangoLLVMGetDefaultTargetTriple)(void);

  char* (*TangoLLVMNormalizeTargetTriple)(const char* triple);

  char* (*TangoLLVMGetHostCPUName)(void);

  char* (*TangoLLVMGetHostCPUFeatures)(void);

  void (*TangoLLVMAddAnalysisPasses)(LLVMTargetMachineRef T, LLVMPassManagerRef PM);

  void (*TangoLLVMRemoveFunctionBody)(LLVMValueRef C);

  LLVMValueRef (*TangoLLVMBuildIntrinsic)(LLVMBuilderRef B, unsigned IntrinsicID,
      LLVMTypeRef *Types,
      unsigned NumTypes,
      LLVMValueRef *Args,
      unsigned NumArgs, const char *Name);
} TangoLLVMEnv;

TangoLLVMEnv* TangoLLVMCreateTangoLLVMEnvironment(void);

LLVM_C_EXTERN_C_END

#endif /* LLVM_C_TANGOLLVM_H */
