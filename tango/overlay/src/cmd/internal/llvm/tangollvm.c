// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 ByteDance Ltd. and/or its affiliates.

//go:build goexperiment.tangollvm && linux && amd64 && cgo && !compiler_bootstrap

#include "tangollvm.h"
#include <stdio.h>

TangoLLVMEnv *_TheTangoLLVMEnv = NULL;

int InitTangoLLVMEnv(const char *libraryname) {
  void *lib_handle;
  TangoLLVMEnv *(*Fn)(void);
  lib_handle = dlopen(libraryname, RTLD_NOW);
  if (lib_handle == NULL) {
    return 1;
  }

  Fn = dlsym(lib_handle, "TangoLLVMCreateTangoLLVMEnvironment");
  if (Fn == NULL) {
    return 2;
  }

  _TheTangoLLVMEnv = Fn();
  if (_TheTangoLLVMEnv == NULL) {
    return -3;
  }
  return 0;
}

LLVMMetadataRef TangoLLVMConstantAsMetadata(LLVMValueRef Val) {
  return _TheTangoLLVMEnv->TangoLLVMConstantAsMetadata(Val);
}

LLVMValueRef TangoLLVMConstAggregateZero(LLVMTypeRef t) {
  return _TheTangoLLVMEnv->TangoLLVMConstAggregateZero(t);
}

LLVMTypeRef TangoLLVMGetFunctionType(LLVMValueRef function) {
  return _TheTangoLLVMEnv->TangoLLVMGetFunctionType(function);
}

LLVMMetadataRef TangoLLVMMDString2(LLVMContextRef C, const char *Str, unsigned SLen) {
  return _TheTangoLLVMEnv->TangoLLVMMDString2(C, Str, SLen);
}

LLVMMetadataRef TangoLLVMMDNode2(LLVMContextRef C, LLVMMetadataRef *MDs, unsigned Count) {
  return _TheTangoLLVMEnv->TangoLLVMMDNode2(C, MDs, Count);
}

void TangoLLVMGlobalObjectAddMetadata(LLVMValueRef objValue, unsigned KindID, LLVMMetadataRef md) {
  _TheTangoLLVMEnv->TangoLLVMGlobalObjectAddMetadata(objValue, KindID, md);
}

void TangoLLVMAddNamedMetadataOperand2(LLVMModuleRef M, const char *name, LLVMMetadataRef Val) {
  _TheTangoLLVMEnv->TangoLLVMAddNamedMetadataOperand2(M, name, Val);
}

void TangoLLVMSetMetadata2(LLVMValueRef Inst, unsigned KindID, LLVMMetadataRef MD) {
  _TheTangoLLVMEnv->TangoLLVMSetMetadata2(Inst, KindID, MD);
}

void TangoLLVMGoSetCurrentDebugLocation(LLVMBuilderRef Bref, unsigned Line, unsigned Col, LLVMMetadataRef Scope, LLVMMetadataRef InlinedAt) {
  _TheTangoLLVMEnv->TangoLLVMGoSetCurrentDebugLocation(Bref, Line, Col, Scope, InlinedAt);
}

struct LLVMDebugLocMetadata TangoLLVMGoGetCurrentDebugLocation(LLVMBuilderRef Bref) {
  return _TheTangoLLVMEnv->TangoLLVMGoGetCurrentDebugLocation(Bref);
}

LLVMValueRef TangoLLVMGoGetInlineAsm(LLVMTypeRef Ty, char *AsmString, size_t AsmStringSize, char *Constraints, size_t ConstraintsSize, LLVMBool HasSideEffects, LLVMBool IsAlignStack, LLVMInlineAsmDialect Dialect, LLVMBool CanThrow) {
  return _TheTangoLLVMEnv->TangoLLVMGoGetInlineAsm(Ty, AsmString, AsmStringSize, Constraints, ConstraintsSize, HasSideEffects, IsAlignStack, Dialect, CanThrow);
}

LLVMValueRef TangoLLVMBuildMemCpyInline(LLVMBuilderRef B, LLVMValueRef Dst, unsigned DstAlign, LLVMValueRef Src, unsigned SrcAlign, LLVMValueRef Size) {
  return _TheTangoLLVMEnv->TangoLLVMBuildMemCpyInline(B, Dst, DstAlign, Src, SrcAlign, Size);
}

LLVMValueRef TangoLLVMBuildGEPByOffset(LLVMBuilderRef B, LLVMValueRef Base, LLVMTypeRef ElemType, uint64_t Offset) {
  return _TheTangoLLVMEnv->TangoLLVMBuildGEPByOffset(B, Base, ElemType, Offset);
}

void TangoLLVMAddCallSiteFnAttribute(LLVMValueRef C, LLVMAttributeRef A) {
  _TheTangoLLVMEnv->TangoLLVMAddCallSiteFnAttribute(C, A);
}

void TangoLLVMShutdown(void) {
  _TheTangoLLVMEnv->TangoLLVMShutdown();
}

void TangoLLVMGetVersion(unsigned *Major, unsigned *Minor, unsigned *Patch) {
  _TheTangoLLVMEnv->TangoLLVMGetVersion(Major, Minor, Patch);
}

char *TangoLLVMCreateMessage(const char *Message) {
  return _TheTangoLLVMEnv->TangoLLVMCreateMessage(Message);
}

void TangoLLVMDisposeMessage(char *Message) {
  _TheTangoLLVMEnv->TangoLLVMDisposeMessage(Message);
}

LLVMContextRef TangoLLVMContextCreate(void) {
  return _TheTangoLLVMEnv->TangoLLVMContextCreate();
}

LLVMContextRef TangoLLVMGetGlobalContext(void) {
  return _TheTangoLLVMEnv->TangoLLVMGetGlobalContext();
}

void TangoLLVMContextSetDiagnosticHandler(LLVMContextRef C, LLVMDiagnosticHandler Handler, void *DiagnosticContext) {
  _TheTangoLLVMEnv->TangoLLVMContextSetDiagnosticHandler(C, Handler, DiagnosticContext);
}

LLVMDiagnosticHandler TangoLLVMContextGetDiagnosticHandler(LLVMContextRef C) {
  return _TheTangoLLVMEnv->TangoLLVMContextGetDiagnosticHandler(C);
}

void* TangoLLVMContextGetDiagnosticContext(LLVMContextRef C) {
  return _TheTangoLLVMEnv->TangoLLVMContextGetDiagnosticContext(C);
}

void TangoLLVMContextSetYieldCallback(LLVMContextRef C, LLVMYieldCallback Callback, void *OpaqueHandle) {
  _TheTangoLLVMEnv->TangoLLVMContextSetYieldCallback(C, Callback, OpaqueHandle);
}

LLVMBool TangoLLVMContextShouldDiscardValueNames(LLVMContextRef C) {
  return _TheTangoLLVMEnv->TangoLLVMContextShouldDiscardValueNames(C);
}

void TangoLLVMContextSetDiscardValueNames(LLVMContextRef C, LLVMBool Discard) {
  _TheTangoLLVMEnv->TangoLLVMContextSetDiscardValueNames(C, Discard);
}

void TangoLLVMContextDispose(LLVMContextRef C) {
  _TheTangoLLVMEnv->TangoLLVMContextDispose(C);
}

char* TangoLLVMGetDiagInfoDescription(LLVMDiagnosticInfoRef DI) {
  return _TheTangoLLVMEnv->TangoLLVMGetDiagInfoDescription(DI);
}

LLVMDiagnosticSeverity TangoLLVMGetDiagInfoSeverity(LLVMDiagnosticInfoRef DI) {
  return _TheTangoLLVMEnv->TangoLLVMGetDiagInfoSeverity(DI);
}

unsigned TangoLLVMGetMDKindIDInContext(LLVMContextRef C, const char *Name, unsigned SLen) {
   return _TheTangoLLVMEnv->TangoLLVMGetMDKindIDInContext(C, Name, SLen);
}

unsigned TangoLLVMGetMDKindID(const char *Name, unsigned SLen) {
  return _TheTangoLLVMEnv->TangoLLVMGetMDKindID(Name, SLen);
}

unsigned TangoLLVMGetEnumAttributeKindForName(const char *Name, size_t SLen) {
  return _TheTangoLLVMEnv->TangoLLVMGetEnumAttributeKindForName(Name, SLen);
}

unsigned TangoLLVMGetLastEnumAttributeKind(void) {
  return _TheTangoLLVMEnv->TangoLLVMGetLastEnumAttributeKind();
}

LLVMAttributeRef TangoLLVMCreateEnumAttribute(LLVMContextRef C, unsigned KindID, uint64_t Val) {
  return _TheTangoLLVMEnv->TangoLLVMCreateEnumAttribute(C, KindID, Val);
}

unsigned TangoLLVMGetEnumAttributeKind(LLVMAttributeRef A) {
  return _TheTangoLLVMEnv->TangoLLVMGetEnumAttributeKind(A);
}

uint64_t TangoLLVMGetEnumAttributeValue(LLVMAttributeRef A) {
  return _TheTangoLLVMEnv->TangoLLVMGetEnumAttributeValue(A);
}

LLVMAttributeRef TangoLLVMCreateTypeAttribute(LLVMContextRef C, unsigned KindID, LLVMTypeRef type_ref) {
  return _TheTangoLLVMEnv->TangoLLVMCreateTypeAttribute(C, KindID, type_ref);
}

LLVMTypeRef TangoLLVMGetTypeAttributeValue(LLVMAttributeRef A) {
  return _TheTangoLLVMEnv->TangoLLVMGetTypeAttributeValue(A);
}

LLVMAttributeRef TangoLLVMCreateConstantRangeAttribute(LLVMContextRef C, unsigned KindID, unsigned NumBits, const uint64_t LowerWords[], const uint64_t UpperWords[]) {
  return _TheTangoLLVMEnv->TangoLLVMCreateConstantRangeAttribute(C, KindID, NumBits, LowerWords, UpperWords);
}

LLVMAttributeRef TangoLLVMCreateStringAttribute(LLVMContextRef C, const char *K, unsigned KLength, const char *V, unsigned VLength) {
  return _TheTangoLLVMEnv->TangoLLVMCreateStringAttribute(C, K, KLength, V, VLength);
}

const char *TangoLLVMGetStringAttributeKind(LLVMAttributeRef A, unsigned *Length) {
  return _TheTangoLLVMEnv->TangoLLVMGetStringAttributeKind(A, Length);
}

const char *TangoLLVMGetStringAttributeValue(LLVMAttributeRef A, unsigned *Length) {
  return _TheTangoLLVMEnv->TangoLLVMGetStringAttributeValue(A, Length);
}

LLVMBool TangoLLVMIsEnumAttribute(LLVMAttributeRef A) {
  return _TheTangoLLVMEnv->TangoLLVMIsEnumAttribute(A);
}

LLVMBool TangoLLVMIsStringAttribute(LLVMAttributeRef A) {
  return _TheTangoLLVMEnv->TangoLLVMIsStringAttribute(A);
}

LLVMBool TangoLLVMIsTypeAttribute(LLVMAttributeRef A) {
  return _TheTangoLLVMEnv->TangoLLVMIsTypeAttribute(A);
}

LLVMTypeRef TangoLLVMGetTypeByName2(LLVMContextRef C, const char *Name) {
  return _TheTangoLLVMEnv->TangoLLVMGetTypeByName2(C, Name);
}

LLVMModuleRef TangoLLVMModuleCreateWithName(const char *ModuleID) {
  return _TheTangoLLVMEnv->TangoLLVMModuleCreateWithName(ModuleID);
}

LLVMModuleRef TangoLLVMModuleCreateWithNameInContext(const char *ModuleID, LLVMContextRef C) {
  return _TheTangoLLVMEnv->TangoLLVMModuleCreateWithNameInContext(ModuleID, C);
}

LLVMModuleRef TangoLLVMCloneModule(LLVMModuleRef M) {
  return _TheTangoLLVMEnv->TangoLLVMCloneModule(M);
}

void TangoLLVMDisposeModule(LLVMModuleRef M) {
  _TheTangoLLVMEnv->TangoLLVMDisposeModule(M);
}

LLVMBool TangoLLVMIsNewDbgInfoFormat(LLVMModuleRef M) {
  return _TheTangoLLVMEnv->TangoLLVMIsNewDbgInfoFormat(M);
}

void TangoLLVMSetIsNewDbgInfoFormat(LLVMModuleRef M, LLVMBool UseNewFormat) {
  _TheTangoLLVMEnv->TangoLLVMSetIsNewDbgInfoFormat(M, UseNewFormat);
}

const char *TangoLLVMGetModuleIdentifier(LLVMModuleRef M, size_t *Len) {
  return _TheTangoLLVMEnv->TangoLLVMGetModuleIdentifier(M, Len);
}

void TangoLLVMSetModuleIdentifier(LLVMModuleRef M, const char *Ident, size_t Len) {
  _TheTangoLLVMEnv->TangoLLVMSetModuleIdentifier(M, Ident, Len);
}

const char *TangoLLVMGetSourceFileName(LLVMModuleRef M, size_t *Len) {
  return _TheTangoLLVMEnv->TangoLLVMGetSourceFileName(M, Len);
}

void TangoLLVMSetSourceFileName(LLVMModuleRef M, const char *Name, size_t Len) {
  _TheTangoLLVMEnv->TangoLLVMSetSourceFileName(M, Name, Len);
}

const char *TangoLLVMGetDataLayoutStr(LLVMModuleRef M) {
  return _TheTangoLLVMEnv->TangoLLVMGetDataLayoutStr(M);
}

const char *TangoLLVMGetDataLayout(LLVMModuleRef M) {
  return _TheTangoLLVMEnv->TangoLLVMGetDataLayout(M);
}

void TangoLLVMSetDataLayout(LLVMModuleRef M, const char *DataLayoutStr) {
  _TheTangoLLVMEnv->TangoLLVMSetDataLayout(M, DataLayoutStr);
}

const char *TangoLLVMGetTarget(LLVMModuleRef M) {
  return _TheTangoLLVMEnv->TangoLLVMGetTarget(M);
}

void TangoLLVMSetTarget(LLVMModuleRef M, const char *Triple) {
  _TheTangoLLVMEnv->TangoLLVMSetTarget(M, Triple);
}

LLVMModuleFlagEntry *TangoLLVMCopyModuleFlagsMetadata(LLVMModuleRef M, size_t *Len) {
  return _TheTangoLLVMEnv->TangoLLVMCopyModuleFlagsMetadata(M, Len);
}

void TangoLLVMDisposeModuleFlagsMetadata(LLVMModuleFlagEntry *Entries) {
  _TheTangoLLVMEnv->TangoLLVMDisposeModuleFlagsMetadata(Entries);
}

LLVMModuleFlagBehavior TangoLLVMModuleFlagEntriesGetFlagBehavior(LLVMModuleFlagEntry *Entries, unsigned Index) {
  return _TheTangoLLVMEnv->TangoLLVMModuleFlagEntriesGetFlagBehavior(Entries, Index);
}

const char *TangoLLVMModuleFlagEntriesGetKey(LLVMModuleFlagEntry *Entries, unsigned Index, size_t *Len) {
  return _TheTangoLLVMEnv->TangoLLVMModuleFlagEntriesGetKey(Entries, Index, Len);
}

LLVMMetadataRef TangoLLVMModuleFlagEntriesGetMetadata(LLVMModuleFlagEntry *Entries, unsigned Index) {
  return _TheTangoLLVMEnv->TangoLLVMModuleFlagEntriesGetMetadata(Entries, Index);
}

LLVMMetadataRef TangoLLVMGetModuleFlag(LLVMModuleRef M, const char *Key, size_t KeyLen) {
  return _TheTangoLLVMEnv->TangoLLVMGetModuleFlag(M, Key, KeyLen);
}

void TangoLLVMAddModuleFlag(LLVMModuleRef M, LLVMModuleFlagBehavior Behavior, const char *Key, size_t KeyLen, LLVMMetadataRef Val) {
  _TheTangoLLVMEnv->TangoLLVMAddModuleFlag(M, Behavior, Key, KeyLen, Val);
}

void TangoLLVMDumpModule(LLVMModuleRef M) {
  _TheTangoLLVMEnv->TangoLLVMDumpModule(M);
}

LLVMBool TangoLLVMPrintModuleToFile(LLVMModuleRef M, const char *Filename, char **ErrorMessage) {
  return _TheTangoLLVMEnv->TangoLLVMPrintModuleToFile(M, Filename, ErrorMessage);
}

char *TangoLLVMPrintModuleToString(LLVMModuleRef M) {
  return _TheTangoLLVMEnv->TangoLLVMPrintModuleToString(M);
}

const char *TangoLLVMGetModuleInlineAsm(LLVMModuleRef M, size_t *Len) {
  return _TheTangoLLVMEnv->TangoLLVMGetModuleInlineAsm(M, Len);
}

void TangoLLVMSetModuleInlineAsm2(LLVMModuleRef M, const char *Asm, size_t Len) {
  _TheTangoLLVMEnv->TangoLLVMSetModuleInlineAsm2(M, Asm, Len);
}

void TangoLLVMAppendModuleInlineAsm(LLVMModuleRef M, const char *Asm, size_t Len) {
  _TheTangoLLVMEnv->TangoLLVMAppendModuleInlineAsm(M, Asm, Len);
}

LLVMValueRef TangoLLVMGetInlineAsm(LLVMTypeRef Ty, const char *AsmString, size_t AsmStringSize, const char *Constraints, size_t ConstraintsSize, LLVMBool HasSideEffects, LLVMBool IsAlignStack, LLVMInlineAsmDialect Dialect, LLVMBool CanThrow) {
  return _TheTangoLLVMEnv->TangoLLVMGetInlineAsm(Ty, AsmString, AsmStringSize, Constraints, ConstraintsSize, HasSideEffects, IsAlignStack, Dialect, CanThrow);
}

const char *TangoLLVMGetInlineAsmAsmString(LLVMValueRef InlineAsmVal, size_t *Len) {
  return _TheTangoLLVMEnv->TangoLLVMGetInlineAsmAsmString(InlineAsmVal, Len);
}

const char *TangoLLVMGetInlineAsmConstraintString(LLVMValueRef InlineAsmVal, size_t *Len) {
  return _TheTangoLLVMEnv->TangoLLVMGetInlineAsmConstraintString(InlineAsmVal, Len);
}

LLVMInlineAsmDialect TangoLLVMGetInlineAsmDialect(LLVMValueRef InlineAsmVal) {
  return _TheTangoLLVMEnv->TangoLLVMGetInlineAsmDialect(InlineAsmVal);
}

LLVMTypeRef TangoLLVMGetInlineAsmFunctionType(LLVMValueRef InlineAsmVal) {
  return _TheTangoLLVMEnv->TangoLLVMGetInlineAsmFunctionType(InlineAsmVal);
}

LLVMBool TangoLLVMGetInlineAsmHasSideEffects(LLVMValueRef InlineAsmVal) {
  return _TheTangoLLVMEnv->TangoLLVMGetInlineAsmHasSideEffects(InlineAsmVal);
}

LLVMBool TangoLLVMGetInlineAsmNeedsAlignedStack(LLVMValueRef InlineAsmVal) {
  return _TheTangoLLVMEnv->TangoLLVMGetInlineAsmNeedsAlignedStack(InlineAsmVal);
}

LLVMBool TangoLLVMGetInlineAsmCanUnwind(LLVMValueRef InlineAsmVal) {
  return _TheTangoLLVMEnv->TangoLLVMGetInlineAsmCanUnwind(InlineAsmVal);
}

LLVMContextRef TangoLLVMGetModuleContext(LLVMModuleRef M) {
  return _TheTangoLLVMEnv->TangoLLVMGetModuleContext(M);
}

LLVMTypeRef TangoLLVMGetTypeByName(LLVMModuleRef M, const char *Name) {
  return _TheTangoLLVMEnv->TangoLLVMGetTypeByName(M, Name);
}

LLVMNamedMDNodeRef TangoLLVMGetFirstNamedMetadata(LLVMModuleRef M) {
  return _TheTangoLLVMEnv->TangoLLVMGetFirstNamedMetadata(M);
}

LLVMNamedMDNodeRef TangoLLVMGetLastNamedMetadata(LLVMModuleRef M) {
  return _TheTangoLLVMEnv->TangoLLVMGetLastNamedMetadata(M);
}

LLVMNamedMDNodeRef TangoLLVMGetNextNamedMetadata(LLVMNamedMDNodeRef NamedMDNode) {
  return _TheTangoLLVMEnv->TangoLLVMGetNextNamedMetadata(NamedMDNode);
}

LLVMNamedMDNodeRef TangoLLVMGetPreviousNamedMetadata(LLVMNamedMDNodeRef NamedMDNode) {
  return _TheTangoLLVMEnv->TangoLLVMGetPreviousNamedMetadata(NamedMDNode);
}

LLVMNamedMDNodeRef TangoLLVMGetNamedMetadata(LLVMModuleRef M, const char *Name, size_t NameLen) {
  return _TheTangoLLVMEnv->TangoLLVMGetNamedMetadata(M, Name, NameLen);
}

LLVMNamedMDNodeRef TangoLLVMGetOrInsertNamedMetadata(LLVMModuleRef M, const char *Name, size_t NameLen) {
  return _TheTangoLLVMEnv->TangoLLVMGetOrInsertNamedMetadata(M, Name, NameLen);
}

const char *TangoLLVMGetNamedMetadataName(LLVMNamedMDNodeRef NamedMD, size_t *NameLen) {
  return _TheTangoLLVMEnv->TangoLLVMGetNamedMetadataName(NamedMD, NameLen);
}

unsigned TangoLLVMGetNamedMetadataNumOperands(LLVMModuleRef M, const char *Name) {
  return _TheTangoLLVMEnv->TangoLLVMGetNamedMetadataNumOperands(M, Name);
}

void TangoLLVMGetNamedMetadataOperands(LLVMModuleRef M, const char *Name, LLVMValueRef *Dest) {
  _TheTangoLLVMEnv->TangoLLVMGetNamedMetadataOperands(M, Name, Dest);
}

void TangoLLVMAddNamedMetadataOperand(LLVMModuleRef M, const char *Name, LLVMValueRef Val) {
  _TheTangoLLVMEnv->TangoLLVMAddNamedMetadataOperand(M, Name, Val);
}

const char *TangoLLVMGetDebugLocDirectory(LLVMValueRef Val, unsigned *Length) {
  return _TheTangoLLVMEnv->TangoLLVMGetDebugLocDirectory(Val, Length);
}

const char *TangoLLVMGetDebugLocFilename(LLVMValueRef Val, unsigned *Length) {
  return _TheTangoLLVMEnv->TangoLLVMGetDebugLocFilename(Val, Length);
}

unsigned TangoLLVMGetDebugLocLine(LLVMValueRef Val) {
  return _TheTangoLLVMEnv->TangoLLVMGetDebugLocLine(Val);
}

unsigned TangoLLVMGetDebugLocColumn(LLVMValueRef Val) {
  return _TheTangoLLVMEnv->TangoLLVMGetDebugLocColumn(Val);
}

LLVMValueRef TangoLLVMAddFunction(LLVMModuleRef M, const char *Name, LLVMTypeRef FunctionTy) {
  return _TheTangoLLVMEnv->TangoLLVMAddFunction(M, Name, FunctionTy);
}

LLVMValueRef TangoLLVMGetNamedFunction(LLVMModuleRef M, const char *Name) {
  return _TheTangoLLVMEnv->TangoLLVMGetNamedFunction(M, Name);
}

LLVMValueRef TangoLLVMGetFirstFunction(LLVMModuleRef M) {
  return _TheTangoLLVMEnv->TangoLLVMGetFirstFunction(M);
}

LLVMValueRef TangoLLVMGetLastFunction(LLVMModuleRef M) {
  return _TheTangoLLVMEnv->TangoLLVMGetLastFunction(M);
}

LLVMValueRef TangoLLVMGetNextFunction(LLVMValueRef Fn) {
  return _TheTangoLLVMEnv->TangoLLVMGetNextFunction(Fn);
}

LLVMValueRef TangoLLVMGetPreviousFunction(LLVMValueRef Fn) {
  return _TheTangoLLVMEnv->TangoLLVMGetPreviousFunction(Fn);
}

void TangoLLVMSetModuleInlineAsm(LLVMModuleRef M, const char *Asm) {
  _TheTangoLLVMEnv->TangoLLVMSetModuleInlineAsm(M, Asm);
}

LLVMTypeKind TangoLLVMGetTypeKind(LLVMTypeRef Ty) {
  return _TheTangoLLVMEnv->TangoLLVMGetTypeKind(Ty);
}

LLVMBool TangoLLVMTypeIsSized(LLVMTypeRef Ty) {
  return _TheTangoLLVMEnv->TangoLLVMTypeIsSized(Ty);
}

LLVMContextRef TangoLLVMGetTypeContext(LLVMTypeRef Ty) {
  return _TheTangoLLVMEnv->TangoLLVMGetTypeContext(Ty);
}

void TangoLLVMDumpType(LLVMTypeRef Val) {
  _TheTangoLLVMEnv->TangoLLVMDumpType(Val);
}

char *TangoLLVMPrintTypeToString(LLVMTypeRef Val) {
  return _TheTangoLLVMEnv->TangoLLVMPrintTypeToString(Val);
}

LLVMTypeRef TangoLLVMInt1TypeInContext(LLVMContextRef C) {
  return _TheTangoLLVMEnv->TangoLLVMInt1TypeInContext(C);
}

LLVMTypeRef TangoLLVMInt8TypeInContext(LLVMContextRef C) {
  return _TheTangoLLVMEnv->TangoLLVMInt8TypeInContext(C);
}

LLVMTypeRef TangoLLVMInt16TypeInContext(LLVMContextRef C) {
  return _TheTangoLLVMEnv->TangoLLVMInt16TypeInContext(C);
}

LLVMTypeRef TangoLLVMInt32TypeInContext(LLVMContextRef C) {
  return _TheTangoLLVMEnv->TangoLLVMInt32TypeInContext(C);
}

LLVMTypeRef TangoLLVMInt64TypeInContext(LLVMContextRef C) {
  return _TheTangoLLVMEnv->TangoLLVMInt64TypeInContext(C);
}

LLVMTypeRef TangoLLVMInt128TypeInContext(LLVMContextRef C) {
  return _TheTangoLLVMEnv->TangoLLVMInt128TypeInContext(C);
}

LLVMTypeRef TangoLLVMIntTypeInContext(LLVMContextRef C, unsigned NumBits) {
  return _TheTangoLLVMEnv->TangoLLVMIntTypeInContext(C, NumBits);
}

LLVMTypeRef TangoLLVMInt1Type(void) {
  return _TheTangoLLVMEnv->TangoLLVMInt1Type();
}

LLVMTypeRef TangoLLVMInt8Type(void) {
  return _TheTangoLLVMEnv->TangoLLVMInt8Type();
}

LLVMTypeRef TangoLLVMInt16Type(void) {
  return _TheTangoLLVMEnv->TangoLLVMInt16Type();
}

LLVMTypeRef TangoLLVMInt32Type(void) {
  return _TheTangoLLVMEnv->TangoLLVMInt32Type();
}

LLVMTypeRef TangoLLVMInt64Type(void) {
  return _TheTangoLLVMEnv->TangoLLVMInt64Type();
}

LLVMTypeRef TangoLLVMInt128Type(void) {
  return _TheTangoLLVMEnv->TangoLLVMInt128Type();
}

LLVMTypeRef TangoLLVMIntType(unsigned NumBits) {
  return _TheTangoLLVMEnv->TangoLLVMIntType(NumBits);
}

unsigned TangoLLVMGetIntTypeWidth(LLVMTypeRef IntegerTy) {
  return _TheTangoLLVMEnv->TangoLLVMGetIntTypeWidth(IntegerTy);
}

LLVMTypeRef TangoLLVMHalfTypeInContext(LLVMContextRef C) {
  return _TheTangoLLVMEnv->TangoLLVMHalfTypeInContext(C);
}

LLVMTypeRef TangoLLVMBFloatTypeInContext(LLVMContextRef C) {
  return _TheTangoLLVMEnv->TangoLLVMBFloatTypeInContext(C);
}

LLVMTypeRef TangoLLVMFloatTypeInContext(LLVMContextRef C) {
  return _TheTangoLLVMEnv->TangoLLVMFloatTypeInContext(C);
}

LLVMTypeRef TangoLLVMDoubleTypeInContext(LLVMContextRef C) {
  return _TheTangoLLVMEnv->TangoLLVMDoubleTypeInContext(C);
}

LLVMTypeRef TangoLLVMX86FP80TypeInContext(LLVMContextRef C) {
  return _TheTangoLLVMEnv->TangoLLVMX86FP80TypeInContext(C);
}

LLVMTypeRef TangoLLVMFP128TypeInContext(LLVMContextRef C) {
  return _TheTangoLLVMEnv->TangoLLVMFP128TypeInContext(C);
}

LLVMTypeRef TangoLLVMPPCFP128TypeInContext(LLVMContextRef C) {
  return _TheTangoLLVMEnv->TangoLLVMPPCFP128TypeInContext(C);
}

LLVMTypeRef TangoLLVMHalfType(void) {
  return _TheTangoLLVMEnv->TangoLLVMHalfType();
}

LLVMTypeRef TangoLLVMBFloatType(void) {
  return _TheTangoLLVMEnv->TangoLLVMBFloatType();
}

LLVMTypeRef TangoLLVMFloatType(void) {
  return _TheTangoLLVMEnv->TangoLLVMFloatType();
}

LLVMTypeRef TangoLLVMDoubleType(void) {
  return _TheTangoLLVMEnv->TangoLLVMDoubleType();
}

LLVMTypeRef TangoLLVMX86FP80Type(void) {
  return _TheTangoLLVMEnv->TangoLLVMX86FP80Type();
}

LLVMTypeRef TangoLLVMFP128Type(void) {
  return _TheTangoLLVMEnv->TangoLLVMFP128Type();
}

LLVMTypeRef TangoLLVMPPCFP128Type(void) {
  return _TheTangoLLVMEnv->TangoLLVMPPCFP128Type();
}

LLVMTypeRef TangoLLVMFunctionType(LLVMTypeRef ReturnType, LLVMTypeRef *ParamTypes, unsigned ParamCount, LLVMBool IsVarArg) {
  return _TheTangoLLVMEnv->TangoLLVMFunctionType(ReturnType, ParamTypes, ParamCount, IsVarArg);
}

LLVMBool TangoLLVMIsFunctionVarArg(LLVMTypeRef FunctionTy) {
  return _TheTangoLLVMEnv->TangoLLVMIsFunctionVarArg(FunctionTy);
}

LLVMTypeRef TangoLLVMGetReturnType(LLVMTypeRef FunctionTy) {
  return _TheTangoLLVMEnv->TangoLLVMGetReturnType(FunctionTy);
}

unsigned TangoLLVMCountParamTypes(LLVMTypeRef FunctionTy) {
  return _TheTangoLLVMEnv->TangoLLVMCountParamTypes(FunctionTy);
}

void TangoLLVMGetParamTypes(LLVMTypeRef FunctionTy, LLVMTypeRef *Dest) {
  _TheTangoLLVMEnv->TangoLLVMGetParamTypes(FunctionTy, Dest);
}

LLVMTypeRef TangoLLVMStructTypeInContext(LLVMContextRef C, LLVMTypeRef *ElementTypes, unsigned ElementCount, LLVMBool Packed) {
  return _TheTangoLLVMEnv->TangoLLVMStructTypeInContext(C, ElementTypes, ElementCount, Packed);
}

LLVMTypeRef TangoLLVMStructType(LLVMTypeRef *ElementTypes, unsigned ElementCount, LLVMBool Packed) {
  return _TheTangoLLVMEnv->TangoLLVMStructType(ElementTypes, ElementCount, Packed);
}

LLVMTypeRef TangoLLVMStructCreateNamed(LLVMContextRef C, const char *Name) {
  return _TheTangoLLVMEnv->TangoLLVMStructCreateNamed(C, Name);
}

const char *TangoLLVMGetStructName(LLVMTypeRef Ty) {
  return _TheTangoLLVMEnv->TangoLLVMGetStructName(Ty);
}

void TangoLLVMStructSetBody(LLVMTypeRef StructTy, LLVMTypeRef *ElementTypes, unsigned ElementCount, LLVMBool Packed) {
  return _TheTangoLLVMEnv->TangoLLVMStructSetBody(StructTy, ElementTypes, ElementCount, Packed);
}

unsigned TangoLLVMCountStructElementTypes(LLVMTypeRef StructTy) {
  return _TheTangoLLVMEnv->TangoLLVMCountStructElementTypes(StructTy);
}

void TangoLLVMGetStructElementTypes(LLVMTypeRef StructTy, LLVMTypeRef *Dest) {
  _TheTangoLLVMEnv->TangoLLVMGetStructElementTypes(StructTy, Dest);
}

LLVMTypeRef TangoLLVMStructGetTypeAtIndex(LLVMTypeRef StructTy, unsigned i) {
  return _TheTangoLLVMEnv->TangoLLVMStructGetTypeAtIndex(StructTy, i);
}

LLVMBool TangoLLVMIsPackedStruct(LLVMTypeRef StructTy) {
  return _TheTangoLLVMEnv->TangoLLVMIsPackedStruct(StructTy);
}

LLVMBool TangoLLVMIsOpaqueStruct(LLVMTypeRef StructTy) {
  return _TheTangoLLVMEnv->TangoLLVMIsOpaqueStruct(StructTy);
}

LLVMBool TangoLLVMIsLiteralStruct(LLVMTypeRef StructTy) {
  return _TheTangoLLVMEnv->TangoLLVMIsLiteralStruct(StructTy);
}

LLVMTypeRef TangoLLVMGetElementType(LLVMTypeRef Ty) {
  return _TheTangoLLVMEnv->TangoLLVMGetElementType(Ty);
}

void TangoLLVMGetSubtypes(LLVMTypeRef Tp, LLVMTypeRef *Arr) {
  _TheTangoLLVMEnv->TangoLLVMGetSubtypes(Tp, Arr);
}

unsigned TangoLLVMGetNumContainedTypes(LLVMTypeRef Tp) {
  return _TheTangoLLVMEnv->TangoLLVMGetNumContainedTypes(Tp);
}

LLVMTypeRef TangoLLVMArrayType(LLVMTypeRef ElementType, unsigned ElementCount) {
   return _TheTangoLLVMEnv->TangoLLVMArrayType(ElementType, ElementCount);
}

LLVMTypeRef TangoLLVMArrayType2(LLVMTypeRef ElementType, uint64_t ElementCount) {
   return _TheTangoLLVMEnv->TangoLLVMArrayType2(ElementType, ElementCount);
}

unsigned TangoLLVMGetArrayLength(LLVMTypeRef ArrayTy) {
   return _TheTangoLLVMEnv->TangoLLVMGetArrayLength(ArrayTy);
}

uint64_t TangoLLVMGetArrayLength2(LLVMTypeRef ArrayTy) {
   return _TheTangoLLVMEnv->TangoLLVMGetArrayLength2(ArrayTy);
}

LLVMTypeRef TangoLLVMPointerType(LLVMTypeRef ElementType, unsigned AddressSpace) {
   return _TheTangoLLVMEnv->TangoLLVMPointerType(ElementType, AddressSpace);
}

LLVMBool TangoLLVMPointerTypeIsOpaque(LLVMTypeRef Ty) {
   return _TheTangoLLVMEnv->TangoLLVMPointerTypeIsOpaque(Ty);
}

LLVMTypeRef TangoLLVMPointerTypeInContext(LLVMContextRef C, unsigned AddressSpace) {
   return _TheTangoLLVMEnv->TangoLLVMPointerTypeInContext(C, AddressSpace);
}

unsigned TangoLLVMGetPointerAddressSpace(LLVMTypeRef PointerTy) {
   return _TheTangoLLVMEnv->TangoLLVMGetPointerAddressSpace(PointerTy);
}

LLVMTypeRef TangoLLVMVectorType(LLVMTypeRef ElementType, unsigned ElementCount) {
   return _TheTangoLLVMEnv->TangoLLVMVectorType(ElementType, ElementCount);
}

LLVMTypeRef TangoLLVMScalableVectorType(LLVMTypeRef ElementType, unsigned ElementCount) {
   return _TheTangoLLVMEnv->TangoLLVMScalableVectorType(ElementType, ElementCount);
}

unsigned TangoLLVMGetVectorSize(LLVMTypeRef VectorTy) {
   return _TheTangoLLVMEnv->TangoLLVMGetVectorSize(VectorTy);
}

LLVMValueRef TangoLLVMGetConstantPtrAuthPointer(LLVMValueRef PtrAuth) {
   return _TheTangoLLVMEnv->TangoLLVMGetConstantPtrAuthPointer(PtrAuth);
}

LLVMValueRef TangoLLVMGetConstantPtrAuthKey(LLVMValueRef PtrAuth) {
   return _TheTangoLLVMEnv->TangoLLVMGetConstantPtrAuthKey(PtrAuth);
}

LLVMValueRef TangoLLVMGetConstantPtrAuthDiscriminator(LLVMValueRef PtrAuth) {
   return _TheTangoLLVMEnv->TangoLLVMGetConstantPtrAuthDiscriminator(PtrAuth);
}

LLVMValueRef TangoLLVMGetConstantPtrAuthAddrDiscriminator(LLVMValueRef PtrAuth) {
   return _TheTangoLLVMEnv->TangoLLVMGetConstantPtrAuthAddrDiscriminator(PtrAuth);
}

LLVMTypeRef TangoLLVMVoidTypeInContext(LLVMContextRef C) {
   return _TheTangoLLVMEnv->TangoLLVMVoidTypeInContext(C);
}

LLVMTypeRef TangoLLVMLabelTypeInContext(LLVMContextRef C) {
   return _TheTangoLLVMEnv->TangoLLVMLabelTypeInContext(C);
}

LLVMTypeRef TangoLLVMX86MMXTypeInContext(LLVMContextRef C) {
   return _TheTangoLLVMEnv->TangoLLVMX86MMXTypeInContext(C);
}

LLVMTypeRef TangoLLVMX86AMXTypeInContext(LLVMContextRef C) {
   return _TheTangoLLVMEnv->TangoLLVMX86AMXTypeInContext(C);
}

LLVMTypeRef TangoLLVMTokenTypeInContext(LLVMContextRef C) {
   return _TheTangoLLVMEnv->TangoLLVMTokenTypeInContext(C);
}

LLVMTypeRef TangoLLVMMetadataTypeInContext(LLVMContextRef C) {
   return _TheTangoLLVMEnv->TangoLLVMMetadataTypeInContext(C);
}

LLVMTypeRef TangoLLVMVoidType(void) {
   return _TheTangoLLVMEnv->TangoLLVMVoidType();
}

LLVMTypeRef TangoLLVMLabelType(void) {
   return _TheTangoLLVMEnv->TangoLLVMLabelType();
}

LLVMTypeRef TangoLLVMX86MMXType(void) {
   return _TheTangoLLVMEnv->TangoLLVMX86MMXType();
}

LLVMTypeRef TangoLLVMX86AMXType(void) {
   return _TheTangoLLVMEnv->TangoLLVMX86AMXType();
}

LLVMTypeRef TangoLLVMTargetExtTypeInContext(LLVMContextRef C, const char *Name, LLVMTypeRef *TypeParams, unsigned TypeParamCount, unsigned *IntParams, unsigned IntParamCount) {
   return _TheTangoLLVMEnv->TangoLLVMTargetExtTypeInContext(C, Name, TypeParams, TypeParamCount, IntParams, IntParamCount);
}

const char *TangoLLVMGetTargetExtTypeName(LLVMTypeRef TargetExtTy) {
   return _TheTangoLLVMEnv->TangoLLVMGetTargetExtTypeName(TargetExtTy);
}

unsigned TangoLLVMGetTargetExtTypeNumTypeParams(LLVMTypeRef TargetExtTy) {
   return _TheTangoLLVMEnv->TangoLLVMGetTargetExtTypeNumTypeParams(TargetExtTy);
}

LLVMTypeRef TangoLLVMGetTargetExtTypeTypeParam(LLVMTypeRef TargetExtTy, unsigned Idx) {
   return _TheTangoLLVMEnv->TangoLLVMGetTargetExtTypeTypeParam(TargetExtTy, Idx);
}

unsigned TangoLLVMGetTargetExtTypeNumIntParams(LLVMTypeRef TargetExtTy) {
   return _TheTangoLLVMEnv->TangoLLVMGetTargetExtTypeNumIntParams(TargetExtTy);
}

unsigned TangoLLVMGetTargetExtTypeIntParam(LLVMTypeRef TargetExtTy, unsigned Idx) {
   return _TheTangoLLVMEnv->TangoLLVMGetTargetExtTypeIntParam(TargetExtTy, Idx);
}

LLVMTypeRef TangoLLVMTypeOf(LLVMValueRef Val) {
   return _TheTangoLLVMEnv->TangoLLVMTypeOf(Val);
}

LLVMValueKind TangoLLVMGetValueKind(LLVMValueRef Val) {
   return _TheTangoLLVMEnv->TangoLLVMGetValueKind(Val);
}

const char *TangoLLVMGetValueName2(LLVMValueRef Val, size_t *Length) {
   return _TheTangoLLVMEnv->TangoLLVMGetValueName2(Val, Length);
}

void TangoLLVMSetValueName2(LLVMValueRef Val, const char *Name, size_t NameLen) {
  _TheTangoLLVMEnv->TangoLLVMSetValueName2(Val, Name, NameLen);
}

void TangoLLVMDumpValue(LLVMValueRef Val) {
  _TheTangoLLVMEnv->TangoLLVMDumpValue(Val);
}

char *TangoLLVMPrintValueToString(LLVMValueRef Val) {
   return _TheTangoLLVMEnv->TangoLLVMPrintValueToString(Val);
}

char *TangoLLVMPrintDbgRecordToString(LLVMDbgRecordRef Record) {
   return _TheTangoLLVMEnv->TangoLLVMPrintDbgRecordToString(Record);
}

void TangoLLVMReplaceAllUsesWith(LLVMValueRef OldVal, LLVMValueRef NewVal) {
  _TheTangoLLVMEnv->TangoLLVMReplaceAllUsesWith(OldVal, NewVal);
}

LLVMBool TangoLLVMIsConstant(LLVMValueRef Val) {
   return _TheTangoLLVMEnv->TangoLLVMIsConstant(Val);
}

LLVMBool TangoLLVMIsUndef(LLVMValueRef Val) {
   return _TheTangoLLVMEnv->TangoLLVMIsUndef(Val);
}

LLVMBool TangoLLVMIsPoison(LLVMValueRef Val) {
   return _TheTangoLLVMEnv->TangoLLVMIsPoison(Val);
}

LLVMValueRef TangoLLVMIsAMDNode(LLVMValueRef Val) {
   return _TheTangoLLVMEnv->TangoLLVMIsAMDNode(Val);
}

LLVMValueRef TangoLLVMIsAValueAsMetadata(LLVMValueRef Val) {
   return _TheTangoLLVMEnv->TangoLLVMIsAValueAsMetadata(Val);
}

LLVMValueRef TangoLLVMIsAMDString(LLVMValueRef Val) {
   return _TheTangoLLVMEnv->TangoLLVMIsAMDString(Val);
}

const char *TangoLLVMGetValueName(LLVMValueRef Val) {
   return _TheTangoLLVMEnv->TangoLLVMGetValueName(Val);
}

void TangoLLVMSetValueName(LLVMValueRef Val, const char *Name) {
  _TheTangoLLVMEnv->TangoLLVMSetValueName(Val, Name);
}

LLVMUseRef TangoLLVMGetFirstUse(LLVMValueRef Val) {
   return _TheTangoLLVMEnv->TangoLLVMGetFirstUse(Val);
}

LLVMUseRef TangoLLVMGetNextUse(LLVMUseRef U) {
   return _TheTangoLLVMEnv->TangoLLVMGetNextUse(U);
}

LLVMValueRef TangoLLVMGetUser(LLVMUseRef U) {
   return _TheTangoLLVMEnv->TangoLLVMGetUser(U);
}

LLVMValueRef TangoLLVMGetUsedValue(LLVMUseRef U) {
   return _TheTangoLLVMEnv->TangoLLVMGetUsedValue(U);
}

LLVMValueRef TangoLLVMGetOperand(LLVMValueRef Val, unsigned Index) {
   return _TheTangoLLVMEnv->TangoLLVMGetOperand(Val, Index);
}

LLVMUseRef TangoLLVMGetOperandUse(LLVMValueRef Val, unsigned Index) {
   return _TheTangoLLVMEnv->TangoLLVMGetOperandUse(Val, Index);
}

void TangoLLVMSetOperand(LLVMValueRef User, unsigned Index, LLVMValueRef Val) {
  _TheTangoLLVMEnv->TangoLLVMSetOperand(User, Index, Val);
}

int TangoLLVMGetNumOperands(LLVMValueRef Val) {
   return _TheTangoLLVMEnv->TangoLLVMGetNumOperands(Val);
}

LLVMValueRef TangoLLVMConstNull(LLVMTypeRef Ty) {
   return _TheTangoLLVMEnv->TangoLLVMConstNull(Ty);
}

LLVMValueRef TangoLLVMConstAllOnes(LLVMTypeRef Ty) {
   return _TheTangoLLVMEnv->TangoLLVMConstAllOnes(Ty);
}

LLVMValueRef TangoLLVMGetUndef(LLVMTypeRef Ty) {
   return _TheTangoLLVMEnv->TangoLLVMGetUndef(Ty);
}

LLVMValueRef TangoLLVMGetPoison(LLVMTypeRef Ty) {
   return _TheTangoLLVMEnv->TangoLLVMGetPoison(Ty);
}

LLVMBool TangoLLVMIsNull(LLVMValueRef Val) {
   return _TheTangoLLVMEnv->TangoLLVMIsNull(Val);
}

LLVMValueRef TangoLLVMConstPointerNull(LLVMTypeRef Ty) {
   return _TheTangoLLVMEnv->TangoLLVMConstPointerNull(Ty);
}

LLVMValueRef TangoLLVMConstInt(LLVMTypeRef IntTy, unsigned long long N, LLVMBool SignExtend) {
   return _TheTangoLLVMEnv->TangoLLVMConstInt(IntTy, N, SignExtend);
}

LLVMValueRef TangoLLVMConstIntOfArbitraryPrecision(LLVMTypeRef IntTy, unsigned NumWords, const uint64_t Words[]) {
   return _TheTangoLLVMEnv->TangoLLVMConstIntOfArbitraryPrecision(IntTy, NumWords, Words);
}

LLVMValueRef TangoLLVMConstIntOfString(LLVMTypeRef IntTy, const char *Text, uint8_t Radix) {
   return _TheTangoLLVMEnv->TangoLLVMConstIntOfString(IntTy, Text, Radix);
}

LLVMValueRef TangoLLVMConstIntOfStringAndSize(LLVMTypeRef IntTy, const char *Text, unsigned SLen, uint8_t Radix) {
   return _TheTangoLLVMEnv->TangoLLVMConstIntOfStringAndSize(IntTy, Text, SLen, Radix);
}

LLVMValueRef TangoLLVMConstReal(LLVMTypeRef RealTy, double N) {
   return _TheTangoLLVMEnv->TangoLLVMConstReal(RealTy, N);
}

LLVMValueRef TangoLLVMConstRealOfString(LLVMTypeRef RealTy, const char *Text) {
   return _TheTangoLLVMEnv->TangoLLVMConstRealOfString(RealTy, Text);
}

LLVMValueRef TangoLLVMConstRealOfStringAndSize(LLVMTypeRef RealTy, const char *Text, unsigned SLen) {
   return _TheTangoLLVMEnv->TangoLLVMConstRealOfStringAndSize(RealTy, Text, SLen);
}

unsigned long long TangoLLVMConstIntGetZExtValue(LLVMValueRef ConstantVal) {
   return _TheTangoLLVMEnv->TangoLLVMConstIntGetZExtValue(ConstantVal);
}

long long TangoLLVMConstIntGetSExtValue(LLVMValueRef ConstantVal) {
   return _TheTangoLLVMEnv->TangoLLVMConstIntGetSExtValue(ConstantVal);
}

double TangoLLVMConstRealGetDouble(LLVMValueRef ConstantVal, LLVMBool *losesInfo) {
   return _TheTangoLLVMEnv->TangoLLVMConstRealGetDouble(ConstantVal, losesInfo);
}

LLVMValueRef TangoLLVMConstStringInContext(LLVMContextRef C, const char *Str, unsigned Length, LLVMBool DontNullTerminate) {
   return _TheTangoLLVMEnv->TangoLLVMConstStringInContext(C, Str, Length, DontNullTerminate);
}

LLVMValueRef TangoLLVMConstStringInContext2(LLVMContextRef C, const char *Str, size_t Length, LLVMBool DontNullTerminate) {
   return _TheTangoLLVMEnv->TangoLLVMConstStringInContext2(C, Str, Length, DontNullTerminate);
}

LLVMValueRef TangoLLVMConstString(const char *Str, unsigned Length, LLVMBool DontNullTerminate) {
   return _TheTangoLLVMEnv->TangoLLVMConstString(Str, Length, DontNullTerminate);
}

LLVMBool TangoLLVMIsConstantString(LLVMValueRef c) {
   return _TheTangoLLVMEnv->TangoLLVMIsConstantString(c);
}

const char *TangoLLVMGetAsString(LLVMValueRef c, size_t *Length) {
   return _TheTangoLLVMEnv->TangoLLVMGetAsString(c, Length);
}

LLVMValueRef TangoLLVMConstStructInContext(LLVMContextRef C, LLVMValueRef *ConstantVals, unsigned Count, LLVMBool Packed) {
   return _TheTangoLLVMEnv->TangoLLVMConstStructInContext(C, ConstantVals, Count, Packed);
}

LLVMValueRef TangoLLVMConstStruct(LLVMValueRef *ConstantVals, unsigned Count, LLVMBool Packed) {
   return _TheTangoLLVMEnv->TangoLLVMConstStruct(ConstantVals, Count, Packed);
}

LLVMValueRef TangoLLVMConstArray(LLVMTypeRef ElementTy, LLVMValueRef *ConstantVals, unsigned Length) {
   return _TheTangoLLVMEnv->TangoLLVMConstArray(ElementTy, ConstantVals, Length);
}

LLVMValueRef TangoLLVMConstArray2(LLVMTypeRef ElementTy, LLVMValueRef *ConstantVals, uint64_t Length) {
   return _TheTangoLLVMEnv->TangoLLVMConstArray2(ElementTy, ConstantVals, Length);
}

LLVMValueRef TangoLLVMConstNamedStruct(LLVMTypeRef StructTy, LLVMValueRef *ConstantVals, unsigned Count) {
   return _TheTangoLLVMEnv->TangoLLVMConstNamedStruct(StructTy, ConstantVals, Count);
}

LLVMValueRef TangoLLVMGetAggregateElement(LLVMValueRef C, unsigned Idx) {
   return _TheTangoLLVMEnv->TangoLLVMGetAggregateElement(C, Idx);
}

LLVMValueRef TangoLLVMGetElementAsConstant(LLVMValueRef C, unsigned idx) {
   return _TheTangoLLVMEnv->TangoLLVMGetElementAsConstant(C, idx);
}

LLVMValueRef TangoLLVMConstVector(LLVMValueRef *ScalarConstantVals, unsigned Size) {
   return _TheTangoLLVMEnv->TangoLLVMConstVector(ScalarConstantVals, Size);
}

LLVMValueRef TangoLLVMConstantPtrAuth(LLVMValueRef Ptr, LLVMValueRef Key, LLVMValueRef Disc, LLVMValueRef AddrDisc) {
   return _TheTangoLLVMEnv->TangoLLVMConstantPtrAuth(Ptr, Key, Disc, AddrDisc);
}

LLVMOpcode TangoLLVMGetConstOpcode(LLVMValueRef ConstantVal) {
   return _TheTangoLLVMEnv->TangoLLVMGetConstOpcode(ConstantVal);
}

LLVMValueRef TangoLLVMAlignOf(LLVMTypeRef Ty) {
   return _TheTangoLLVMEnv->TangoLLVMAlignOf(Ty);
}

LLVMValueRef TangoLLVMSizeOf(LLVMTypeRef Ty) {
   return _TheTangoLLVMEnv->TangoLLVMSizeOf(Ty);
}

LLVMValueRef TangoLLVMConstNeg(LLVMValueRef ConstantVal) {
   return _TheTangoLLVMEnv->TangoLLVMConstNeg(ConstantVal);
}

LLVMValueRef TangoLLVMConstNSWNeg(LLVMValueRef ConstantVal) {
   return _TheTangoLLVMEnv->TangoLLVMConstNSWNeg(ConstantVal);
}

LLVMValueRef TangoLLVMConstNUWNeg(LLVMValueRef ConstantVal) {
   return _TheTangoLLVMEnv->TangoLLVMConstNUWNeg(ConstantVal);
}

LLVMValueRef TangoLLVMConstNot(LLVMValueRef ConstantVal) {
   return _TheTangoLLVMEnv->TangoLLVMConstNot(ConstantVal);
}

LLVMValueRef TangoLLVMConstAdd(LLVMValueRef LHSConstant, LLVMValueRef RHSConstant) {
   return _TheTangoLLVMEnv->TangoLLVMConstAdd(LHSConstant, RHSConstant);
}

LLVMValueRef TangoLLVMConstNSWAdd(LLVMValueRef LHSConstant, LLVMValueRef RHSConstant) {
   return _TheTangoLLVMEnv->TangoLLVMConstNSWAdd(LHSConstant, RHSConstant);
}

LLVMValueRef TangoLLVMConstNUWAdd(LLVMValueRef LHSConstant, LLVMValueRef RHSConstant) {
   return _TheTangoLLVMEnv->TangoLLVMConstNUWAdd(LHSConstant, RHSConstant);
}

LLVMValueRef TangoLLVMConstSub(LLVMValueRef LHSConstant, LLVMValueRef RHSConstant) {
   return _TheTangoLLVMEnv->TangoLLVMConstSub(LHSConstant, RHSConstant);
}

LLVMValueRef TangoLLVMConstNSWSub(LLVMValueRef LHSConstant, LLVMValueRef RHSConstant) {
   return _TheTangoLLVMEnv->TangoLLVMConstNSWSub(LHSConstant, RHSConstant);
}

LLVMValueRef TangoLLVMConstNUWSub(LLVMValueRef LHSConstant, LLVMValueRef RHSConstant) {
   return _TheTangoLLVMEnv->TangoLLVMConstNUWSub(LHSConstant, RHSConstant);
}

LLVMValueRef TangoLLVMConstMul(LLVMValueRef LHSConstant, LLVMValueRef RHSConstant) {
   return _TheTangoLLVMEnv->TangoLLVMConstMul(LHSConstant, RHSConstant);
}

LLVMValueRef TangoLLVMConstNSWMul(LLVMValueRef LHSConstant, LLVMValueRef RHSConstant) {
   return _TheTangoLLVMEnv->TangoLLVMConstNSWMul(LHSConstant, RHSConstant);
}

LLVMValueRef TangoLLVMConstNUWMul(LLVMValueRef LHSConstant, LLVMValueRef RHSConstant) {
   return _TheTangoLLVMEnv->TangoLLVMConstNUWMul(LHSConstant, RHSConstant);
}

LLVMValueRef TangoLLVMConstXor(LLVMValueRef LHSConstant, LLVMValueRef RHSConstant) {
   return _TheTangoLLVMEnv->TangoLLVMConstXor(LHSConstant, RHSConstant);
}

LLVMValueRef TangoLLVMConstGEP2(LLVMTypeRef Ty, LLVMValueRef ConstantVal, LLVMValueRef *ConstantIndices, unsigned NumIndices) {
   return _TheTangoLLVMEnv->TangoLLVMConstGEP2(Ty, ConstantVal, ConstantIndices, NumIndices);
}

LLVMValueRef TangoLLVMConstInBoundsGEP2(LLVMTypeRef Ty, LLVMValueRef ConstantVal, LLVMValueRef *ConstantIndices, unsigned NumIndices) {
   return _TheTangoLLVMEnv->TangoLLVMConstInBoundsGEP2(Ty, ConstantVal, ConstantIndices, NumIndices);
}

LLVMValueRef TangoLLVMConstGEPWithNoWrapFlags(LLVMTypeRef Ty, LLVMValueRef ConstantVal, LLVMValueRef *ConstantIndices, unsigned NumIndices, LLVMGEPNoWrapFlags NoWrapFlags) {
   return _TheTangoLLVMEnv->TangoLLVMConstGEPWithNoWrapFlags(Ty, ConstantVal, ConstantIndices, NumIndices, NoWrapFlags);
}

LLVMValueRef TangoLLVMConstTrunc(LLVMValueRef ConstantVal, LLVMTypeRef ToType) {
   return _TheTangoLLVMEnv->TangoLLVMConstTrunc(ConstantVal, ToType);
}

LLVMValueRef TangoLLVMConstPtrToInt(LLVMValueRef ConstantVal, LLVMTypeRef ToType) {
   return _TheTangoLLVMEnv->TangoLLVMConstPtrToInt(ConstantVal, ToType);
}

LLVMValueRef TangoLLVMConstIntToPtr(LLVMValueRef ConstantVal, LLVMTypeRef ToType) {
   return _TheTangoLLVMEnv->TangoLLVMConstIntToPtr(ConstantVal, ToType);
}

LLVMValueRef TangoLLVMConstBitCast(LLVMValueRef ConstantVal, LLVMTypeRef ToType) {
   return _TheTangoLLVMEnv->TangoLLVMConstBitCast(ConstantVal, ToType);
}

LLVMValueRef TangoLLVMConstAddrSpaceCast(LLVMValueRef ConstantVal, LLVMTypeRef ToType) {
   return _TheTangoLLVMEnv->TangoLLVMConstAddrSpaceCast(ConstantVal, ToType);
}

LLVMValueRef TangoLLVMConstTruncOrBitCast(LLVMValueRef ConstantVal, LLVMTypeRef ToType) {
   return _TheTangoLLVMEnv->TangoLLVMConstTruncOrBitCast(ConstantVal, ToType);
}

LLVMValueRef TangoLLVMConstPointerCast(LLVMValueRef ConstantVal, LLVMTypeRef ToType) {
   return _TheTangoLLVMEnv->TangoLLVMConstPointerCast(ConstantVal, ToType);
}

LLVMValueRef TangoLLVMConstExtractElement(LLVMValueRef VectorConstant, LLVMValueRef IndexConstant) {
   return _TheTangoLLVMEnv->TangoLLVMConstExtractElement(VectorConstant, IndexConstant);
}

LLVMValueRef TangoLLVMConstInsertElement(LLVMValueRef VectorConstant, LLVMValueRef ElementValueConstant, LLVMValueRef IndexConstant) {
   return _TheTangoLLVMEnv->TangoLLVMConstInsertElement(VectorConstant, ElementValueConstant, IndexConstant);
}

LLVMValueRef TangoLLVMConstShuffleVector(LLVMValueRef VectorAConstant, LLVMValueRef VectorBConstant, LLVMValueRef MaskConstant) {
   return _TheTangoLLVMEnv->TangoLLVMConstShuffleVector(VectorAConstant, VectorBConstant, MaskConstant);
}

LLVMValueRef TangoLLVMBlockAddress(LLVMValueRef F, LLVMBasicBlockRef BB) {
   return _TheTangoLLVMEnv->TangoLLVMBlockAddress(F, BB);
}

LLVMValueRef TangoLLVMGetBlockAddressFunction(LLVMValueRef BlockAddr) {
   return _TheTangoLLVMEnv->TangoLLVMGetBlockAddressFunction(BlockAddr);
}

LLVMBasicBlockRef TangoLLVMGetBlockAddressBasicBlock(LLVMValueRef BlockAddr) {
   return _TheTangoLLVMEnv->TangoLLVMGetBlockAddressBasicBlock(BlockAddr);
}

LLVMValueRef TangoLLVMConstInlineAsm(LLVMTypeRef Ty, const char *AsmString, const char *Constraints, LLVMBool HasSideEffects, LLVMBool IsAlignStack) {
   return _TheTangoLLVMEnv->TangoLLVMConstInlineAsm(Ty, AsmString, Constraints, HasSideEffects, IsAlignStack);
}

LLVMModuleRef TangoLLVMGetGlobalParent(LLVMValueRef Global) {
   return _TheTangoLLVMEnv->TangoLLVMGetGlobalParent(Global);
}

LLVMBool TangoLLVMIsDeclaration(LLVMValueRef Global) {
   return _TheTangoLLVMEnv->TangoLLVMIsDeclaration(Global);
}

LLVMLinkage TangoLLVMGetLinkage(LLVMValueRef Global) {
   return _TheTangoLLVMEnv->TangoLLVMGetLinkage(Global);
}

void TangoLLVMSetLinkage(LLVMValueRef Global, LLVMLinkage Linkage) {
  _TheTangoLLVMEnv->TangoLLVMSetLinkage(Global, Linkage);
}

const char *TangoLLVMGetSection(LLVMValueRef Global) {
   return _TheTangoLLVMEnv->TangoLLVMGetSection(Global);
}

void TangoLLVMSetSection(LLVMValueRef Global, const char *Section) {
  _TheTangoLLVMEnv->TangoLLVMSetSection(Global, Section);
}

LLVMVisibility TangoLLVMGetVisibility(LLVMValueRef Global) {
   return _TheTangoLLVMEnv->TangoLLVMGetVisibility(Global);
}

void TangoLLVMSetVisibility(LLVMValueRef Global, LLVMVisibility Viz) {
  _TheTangoLLVMEnv->TangoLLVMSetVisibility(Global, Viz);
}

LLVMDLLStorageClass TangoLLVMGetDLLStorageClass(LLVMValueRef Global) {
   return _TheTangoLLVMEnv->TangoLLVMGetDLLStorageClass(Global);
}

void TangoLLVMSetDLLStorageClass(LLVMValueRef Global, LLVMDLLStorageClass Class) {
  _TheTangoLLVMEnv->TangoLLVMSetDLLStorageClass(Global, Class);
}

LLVMUnnamedAddr TangoLLVMGetUnnamedAddress(LLVMValueRef Global) {
   return _TheTangoLLVMEnv->TangoLLVMGetUnnamedAddress(Global);
}

void TangoLLVMSetUnnamedAddress(LLVMValueRef Global, LLVMUnnamedAddr UnnamedAddr) {
  _TheTangoLLVMEnv->TangoLLVMSetUnnamedAddress(Global, UnnamedAddr);
}

LLVMTypeRef TangoLLVMGlobalGetValueType(LLVMValueRef Global) {
   return _TheTangoLLVMEnv->TangoLLVMGlobalGetValueType(Global);
}

LLVMBool TangoLLVMHasUnnamedAddr(LLVMValueRef Global) {
   return _TheTangoLLVMEnv->TangoLLVMHasUnnamedAddr(Global);
}

void TangoLLVMSetUnnamedAddr(LLVMValueRef Global, LLVMBool HasUnnamedAddr) {
  _TheTangoLLVMEnv->TangoLLVMSetUnnamedAddr(Global, HasUnnamedAddr);
}

unsigned TangoLLVMGetAlignment(LLVMValueRef V) {
   return _TheTangoLLVMEnv->TangoLLVMGetAlignment(V);
}

void TangoLLVMSetAlignment(LLVMValueRef V, unsigned Bytes) {
  _TheTangoLLVMEnv->TangoLLVMSetAlignment(V, Bytes);
}

void TangoLLVMGlobalSetMetadata(LLVMValueRef Global, unsigned Kind, LLVMMetadataRef MD) {
  _TheTangoLLVMEnv->TangoLLVMGlobalSetMetadata(Global, Kind, MD);
}

void TangoLLVMGlobalEraseMetadata(LLVMValueRef Global, unsigned Kind) {
  _TheTangoLLVMEnv->TangoLLVMGlobalEraseMetadata(Global, Kind);
}

void TangoLLVMGlobalClearMetadata(LLVMValueRef Global) {
  _TheTangoLLVMEnv->TangoLLVMGlobalClearMetadata(Global);
}

LLVMValueMetadataEntry *TangoLLVMGlobalCopyAllMetadata(LLVMValueRef Value, size_t *NumEntries) {
   return _TheTangoLLVMEnv->TangoLLVMGlobalCopyAllMetadata(Value, NumEntries);
}

void TangoLLVMDisposeValueMetadataEntries(LLVMValueMetadataEntry *Entries) {
  _TheTangoLLVMEnv->TangoLLVMDisposeValueMetadataEntries(Entries);
}

unsigned TangoLLVMValueMetadataEntriesGetKind(LLVMValueMetadataEntry *Entries, unsigned Index) {
   return _TheTangoLLVMEnv->TangoLLVMValueMetadataEntriesGetKind(Entries, Index);
}

LLVMMetadataRef TangoLLVMValueMetadataEntriesGetMetadata(LLVMValueMetadataEntry *Entries, unsigned Index) {
   return _TheTangoLLVMEnv->TangoLLVMValueMetadataEntriesGetMetadata(Entries, Index);
}

LLVMValueRef TangoLLVMAddGlobal(LLVMModuleRef M, LLVMTypeRef Ty, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMAddGlobal(M, Ty, Name);
}

LLVMValueRef TangoLLVMAddGlobalInAddressSpace(LLVMModuleRef M, LLVMTypeRef Ty, const char *Name, unsigned AddressSpace) {
   return _TheTangoLLVMEnv->TangoLLVMAddGlobalInAddressSpace(M, Ty, Name, AddressSpace);
}

LLVMValueRef TangoLLVMGetNamedGlobal(LLVMModuleRef M, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMGetNamedGlobal(M, Name);
}

LLVMValueRef TangoLLVMGetFirstGlobal(LLVMModuleRef M) {
   return _TheTangoLLVMEnv->TangoLLVMGetFirstGlobal(M);
}

LLVMValueRef TangoLLVMGetLastGlobal(LLVMModuleRef M) {
   return _TheTangoLLVMEnv->TangoLLVMGetLastGlobal(M);
}

LLVMValueRef TangoLLVMGetNextGlobal(LLVMValueRef GlobalVar) {
   return _TheTangoLLVMEnv->TangoLLVMGetNextGlobal(GlobalVar);
}

LLVMValueRef TangoLLVMGetPreviousGlobal(LLVMValueRef GlobalVar) {
   return _TheTangoLLVMEnv->TangoLLVMGetPreviousGlobal(GlobalVar);
}

void TangoLLVMDeleteGlobal(LLVMValueRef GlobalVar) {
  _TheTangoLLVMEnv->TangoLLVMDeleteGlobal(GlobalVar);
}

LLVMValueRef TangoLLVMGetInitializer(LLVMValueRef GlobalVar) {
   return _TheTangoLLVMEnv->TangoLLVMGetInitializer(GlobalVar);
}

void TangoLLVMSetInitializer(LLVMValueRef GlobalVar, LLVMValueRef ConstantVal) {
  _TheTangoLLVMEnv->TangoLLVMSetInitializer(GlobalVar, ConstantVal);
}

LLVMBool TangoLLVMIsThreadLocal(LLVMValueRef GlobalVar) {
   return _TheTangoLLVMEnv->TangoLLVMIsThreadLocal(GlobalVar);
}

void TangoLLVMSetThreadLocal(LLVMValueRef GlobalVar, LLVMBool IsThreadLocal) {
  _TheTangoLLVMEnv->TangoLLVMSetThreadLocal(GlobalVar, IsThreadLocal);
}

LLVMBool TangoLLVMIsGlobalConstant(LLVMValueRef GlobalVar) {
   return _TheTangoLLVMEnv->TangoLLVMIsGlobalConstant(GlobalVar);
}

void TangoLLVMSetGlobalConstant(LLVMValueRef GlobalVar, LLVMBool IsConstant) {
  _TheTangoLLVMEnv->TangoLLVMSetGlobalConstant(GlobalVar, IsConstant);
}

LLVMThreadLocalMode TangoLLVMGetThreadLocalMode(LLVMValueRef GlobalVar) {
   return _TheTangoLLVMEnv->TangoLLVMGetThreadLocalMode(GlobalVar);
}

void TangoLLVMSetThreadLocalMode(LLVMValueRef GlobalVar, LLVMThreadLocalMode Mode) {
  _TheTangoLLVMEnv->TangoLLVMSetThreadLocalMode(GlobalVar, Mode);
}

LLVMBool TangoLLVMIsExternallyInitialized(LLVMValueRef GlobalVar) {
   return _TheTangoLLVMEnv->TangoLLVMIsExternallyInitialized(GlobalVar);
}

void TangoLLVMSetExternallyInitialized(LLVMValueRef GlobalVar, LLVMBool IsExtInit) {
  _TheTangoLLVMEnv->TangoLLVMSetExternallyInitialized(GlobalVar, IsExtInit);
}

LLVMValueRef TangoLLVMAddAlias2(LLVMModuleRef M, LLVMTypeRef ValueTy, unsigned AddrSpace, LLVMValueRef Aliasee, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMAddAlias2(M, ValueTy, AddrSpace, Aliasee, Name);
}

LLVMValueRef TangoLLVMGetNamedGlobalAlias(LLVMModuleRef M, const char *Name, size_t NameLen) {
   return _TheTangoLLVMEnv->TangoLLVMGetNamedGlobalAlias(M, Name, NameLen);
}

LLVMValueRef TangoLLVMGetFirstGlobalAlias(LLVMModuleRef M) {
   return _TheTangoLLVMEnv->TangoLLVMGetFirstGlobalAlias(M);
}

LLVMValueRef TangoLLVMGetLastGlobalAlias(LLVMModuleRef M) {
   return _TheTangoLLVMEnv->TangoLLVMGetLastGlobalAlias(M);
}

LLVMValueRef TangoLLVMGetNextGlobalAlias(LLVMValueRef GA) {
   return _TheTangoLLVMEnv->TangoLLVMGetNextGlobalAlias(GA);
}

LLVMValueRef TangoLLVMGetPreviousGlobalAlias(LLVMValueRef GA) {
   return _TheTangoLLVMEnv->TangoLLVMGetPreviousGlobalAlias(GA);
}

LLVMValueRef TangoLLVMAliasGetAliasee(LLVMValueRef Alias) {
   return _TheTangoLLVMEnv->TangoLLVMAliasGetAliasee(Alias);
}

void TangoLLVMAliasSetAliasee(LLVMValueRef Alias, LLVMValueRef Aliasee) {
  _TheTangoLLVMEnv->TangoLLVMAliasSetAliasee(Alias, Aliasee);
}

void TangoLLVMDeleteFunction(LLVMValueRef Fn) {
  _TheTangoLLVMEnv->TangoLLVMDeleteFunction(Fn);
}

LLVMBool TangoLLVMHasPersonalityFn(LLVMValueRef Fn) {
   return _TheTangoLLVMEnv->TangoLLVMHasPersonalityFn(Fn);
}

LLVMValueRef TangoLLVMGetPersonalityFn(LLVMValueRef Fn) {
   return _TheTangoLLVMEnv->TangoLLVMGetPersonalityFn(Fn);
}

void TangoLLVMSetPersonalityFn(LLVMValueRef Fn, LLVMValueRef PersonalityFn) {
  _TheTangoLLVMEnv->TangoLLVMSetPersonalityFn(Fn, PersonalityFn);
}

unsigned TangoLLVMLookupIntrinsicID(const char *Name, size_t NameLen) {
   return _TheTangoLLVMEnv->TangoLLVMLookupIntrinsicID(Name, NameLen);
}

unsigned TangoLLVMGetIntrinsicID(LLVMValueRef Fn) {
   return _TheTangoLLVMEnv->TangoLLVMGetIntrinsicID(Fn);
}

LLVMValueRef TangoLLVMGetIntrinsicDeclaration(LLVMModuleRef Mod, unsigned ID, LLVMTypeRef *ParamTypes, size_t ParamCount) {
   return _TheTangoLLVMEnv->TangoLLVMGetIntrinsicDeclaration(Mod, ID, ParamTypes, ParamCount);
}

LLVMTypeRef TangoLLVMIntrinsicGetType(LLVMContextRef Ctx, unsigned ID, LLVMTypeRef *ParamTypes, size_t ParamCount) {
   return _TheTangoLLVMEnv->TangoLLVMIntrinsicGetType(Ctx, ID, ParamTypes, ParamCount);
}

const char *TangoLLVMIntrinsicGetName(unsigned ID, size_t *NameLength) {
   return _TheTangoLLVMEnv->TangoLLVMIntrinsicGetName(ID, NameLength);
}

const char *TangoLLVMIntrinsicCopyOverloadedName(unsigned ID, LLVMTypeRef *ParamTypes, size_t ParamCount, size_t *NameLength) {
   return _TheTangoLLVMEnv->TangoLLVMIntrinsicCopyOverloadedName(ID, ParamTypes, ParamCount, NameLength);
}

const char *TangoLLVMIntrinsicCopyOverloadedName2(LLVMModuleRef Mod, unsigned ID, LLVMTypeRef *ParamTypes, size_t ParamCount, size_t *NameLength) {
   return _TheTangoLLVMEnv->TangoLLVMIntrinsicCopyOverloadedName2(Mod, ID, ParamTypes, ParamCount, NameLength);
}

LLVMBool TangoLLVMIntrinsicIsOverloaded(unsigned ID) {
   return _TheTangoLLVMEnv->TangoLLVMIntrinsicIsOverloaded(ID);
}

unsigned TangoLLVMGetFunctionCallConv(LLVMValueRef Fn) {
   return _TheTangoLLVMEnv->TangoLLVMGetFunctionCallConv(Fn);
}

void TangoLLVMSetFunctionCallConv(LLVMValueRef Fn, unsigned CC) {
  _TheTangoLLVMEnv->TangoLLVMSetFunctionCallConv(Fn, CC);
}

const char *TangoLLVMGetGC(LLVMValueRef Fn) {
   return _TheTangoLLVMEnv->TangoLLVMGetGC(Fn);
}

void TangoLLVMSetGC(LLVMValueRef Fn, const char *Name) {
  _TheTangoLLVMEnv->TangoLLVMSetGC(Fn, Name);
}

LLVMValueRef TangoLLVMGetPrefixData(LLVMValueRef Fn) {
   return _TheTangoLLVMEnv->TangoLLVMGetPrefixData(Fn);
}

LLVMBool TangoLLVMHasPrefixData(LLVMValueRef Fn) {
   return _TheTangoLLVMEnv->TangoLLVMHasPrefixData(Fn);
}

void TangoLLVMSetPrefixData(LLVMValueRef Fn, LLVMValueRef prefixData) {
  _TheTangoLLVMEnv->TangoLLVMSetPrefixData(Fn, prefixData);
}

LLVMValueRef TangoLLVMGetPrologueData(LLVMValueRef Fn) {
   return _TheTangoLLVMEnv->TangoLLVMGetPrologueData(Fn);
}

LLVMBool TangoLLVMHasPrologueData(LLVMValueRef Fn) {
   return _TheTangoLLVMEnv->TangoLLVMHasPrologueData(Fn);
}

void TangoLLVMSetPrologueData(LLVMValueRef Fn, LLVMValueRef prologueData) {
  _TheTangoLLVMEnv->TangoLLVMSetPrologueData(Fn, prologueData);
}

void TangoLLVMAddAttributeAtIndex(LLVMValueRef F, LLVMAttributeIndex Idx, LLVMAttributeRef A) {
  _TheTangoLLVMEnv->TangoLLVMAddAttributeAtIndex(F, Idx, A);
}

unsigned TangoLLVMGetAttributeCountAtIndex(LLVMValueRef F, LLVMAttributeIndex Idx) {
   return _TheTangoLLVMEnv->TangoLLVMGetAttributeCountAtIndex(F, Idx);
}

void TangoLLVMGetAttributesAtIndex(LLVMValueRef F, LLVMAttributeIndex Idx, LLVMAttributeRef *Attrs) {
  _TheTangoLLVMEnv->TangoLLVMGetAttributesAtIndex(F, Idx, Attrs);
}

LLVMAttributeRef TangoLLVMGetEnumAttributeAtIndex(LLVMValueRef F, LLVMAttributeIndex Idx, unsigned KindID) {
   return _TheTangoLLVMEnv->TangoLLVMGetEnumAttributeAtIndex(F, Idx, KindID);
}

LLVMAttributeRef TangoLLVMGetStringAttributeAtIndex(LLVMValueRef F, LLVMAttributeIndex Idx, const char *K, unsigned KLen) {
   return _TheTangoLLVMEnv->TangoLLVMGetStringAttributeAtIndex(F, Idx, K, KLen);
}

void TangoLLVMRemoveEnumAttributeAtIndex(LLVMValueRef F, LLVMAttributeIndex Idx, unsigned KindID) {
  _TheTangoLLVMEnv->TangoLLVMRemoveEnumAttributeAtIndex(F, Idx, KindID);
}

void TangoLLVMRemoveStringAttributeAtIndex(LLVMValueRef F, LLVMAttributeIndex Idx, const char *K, unsigned KLen) {
  _TheTangoLLVMEnv->TangoLLVMRemoveStringAttributeAtIndex(F, Idx, K, KLen);
}

void TangoLLVMAddTargetDependentFunctionAttr(LLVMValueRef Fn, const char *A, const char *V) {
  _TheTangoLLVMEnv->TangoLLVMAddTargetDependentFunctionAttr(Fn, A, V);
}

unsigned TangoLLVMCountParams(LLVMValueRef Fn) {
   return _TheTangoLLVMEnv->TangoLLVMCountParams(Fn);
}

void TangoLLVMGetParams(LLVMValueRef Fn, LLVMValueRef *Params) {
  _TheTangoLLVMEnv->TangoLLVMGetParams(Fn, Params);
}

LLVMValueRef TangoLLVMGetParam(LLVMValueRef Fn, unsigned Index) {
   return _TheTangoLLVMEnv->TangoLLVMGetParam(Fn, Index);
}

LLVMValueRef TangoLLVMGetParamParent(LLVMValueRef Inst) {
   return _TheTangoLLVMEnv->TangoLLVMGetParamParent(Inst);
}

LLVMValueRef TangoLLVMGetFirstParam(LLVMValueRef Fn) {
   return _TheTangoLLVMEnv->TangoLLVMGetFirstParam(Fn);
}

LLVMValueRef TangoLLVMGetLastParam(LLVMValueRef Fn) {
   return _TheTangoLLVMEnv->TangoLLVMGetLastParam(Fn);
}

LLVMValueRef TangoLLVMGetNextParam(LLVMValueRef Arg) {
   return _TheTangoLLVMEnv->TangoLLVMGetNextParam(Arg);
}

LLVMValueRef TangoLLVMGetPreviousParam(LLVMValueRef Arg) {
   return _TheTangoLLVMEnv->TangoLLVMGetPreviousParam(Arg);
}

void TangoLLVMSetParamAlignment(LLVMValueRef Arg, unsigned Align) {
  _TheTangoLLVMEnv->TangoLLVMSetParamAlignment(Arg, Align);
}

LLVMValueRef TangoLLVMAddGlobalIFunc(LLVMModuleRef M, const char *Name, size_t NameLen, LLVMTypeRef Ty, unsigned AddrSpace, LLVMValueRef Resolver) {
   return _TheTangoLLVMEnv->TangoLLVMAddGlobalIFunc(M, Name, NameLen, Ty, AddrSpace, Resolver);
}

LLVMValueRef TangoLLVMGetNamedGlobalIFunc(LLVMModuleRef M, const char *Name, size_t NameLen) {
   return _TheTangoLLVMEnv->TangoLLVMGetNamedGlobalIFunc(M, Name, NameLen);
}

LLVMValueRef TangoLLVMGetFirstGlobalIFunc(LLVMModuleRef M) {
   return _TheTangoLLVMEnv->TangoLLVMGetFirstGlobalIFunc(M);
}

LLVMValueRef TangoLLVMGetLastGlobalIFunc(LLVMModuleRef M) {
   return _TheTangoLLVMEnv->TangoLLVMGetLastGlobalIFunc(M);
}

LLVMValueRef TangoLLVMGetNextGlobalIFunc(LLVMValueRef IFunc) {
   return _TheTangoLLVMEnv->TangoLLVMGetNextGlobalIFunc(IFunc);
}

LLVMValueRef TangoLLVMGetPreviousGlobalIFunc(LLVMValueRef IFunc) {
   return _TheTangoLLVMEnv->TangoLLVMGetPreviousGlobalIFunc(IFunc);
}

LLVMValueRef TangoLLVMGetGlobalIFuncResolver(LLVMValueRef IFunc) {
   return _TheTangoLLVMEnv->TangoLLVMGetGlobalIFuncResolver(IFunc);
}

void TangoLLVMSetGlobalIFuncResolver(LLVMValueRef IFunc, LLVMValueRef Resolver) {
  _TheTangoLLVMEnv->TangoLLVMSetGlobalIFuncResolver(IFunc, Resolver);
}

void TangoLLVMEraseGlobalIFunc(LLVMValueRef IFunc) {
  _TheTangoLLVMEnv->TangoLLVMEraseGlobalIFunc(IFunc);
}

void TangoLLVMRemoveGlobalIFunc(LLVMValueRef IFunc) {
  _TheTangoLLVMEnv->TangoLLVMRemoveGlobalIFunc(IFunc);
}

LLVMMetadataRef TangoLLVMMDStringInContext2(LLVMContextRef C, const char *Str, size_t SLen) {
   return _TheTangoLLVMEnv->TangoLLVMMDStringInContext2(C, Str, SLen);
}

LLVMMetadataRef TangoLLVMMDNodeInContext2(LLVMContextRef C, LLVMMetadataRef *MDs, size_t Count) {
   return _TheTangoLLVMEnv->TangoLLVMMDNodeInContext2(C, MDs, Count);
}

LLVMValueRef TangoLLVMMetadataAsValue(LLVMContextRef C, LLVMMetadataRef MD) {
   return _TheTangoLLVMEnv->TangoLLVMMetadataAsValue(C, MD);
}

LLVMMetadataRef TangoLLVMValueAsMetadata(LLVMValueRef Val) {
   return _TheTangoLLVMEnv->TangoLLVMValueAsMetadata(Val);
}

const char *TangoLLVMGetMDString(LLVMValueRef V, unsigned *Length) {
   return _TheTangoLLVMEnv->TangoLLVMGetMDString(V, Length);
}

unsigned TangoLLVMGetMDNodeNumOperands(LLVMValueRef V) {
   return _TheTangoLLVMEnv->TangoLLVMGetMDNodeNumOperands(V);
}

void TangoLLVMGetMDNodeOperands(LLVMValueRef V, LLVMValueRef *Dest) {
  _TheTangoLLVMEnv->TangoLLVMGetMDNodeOperands(V, Dest);
}

void TangoLLVMReplaceMDNodeOperandWith(LLVMValueRef V, unsigned Index, LLVMMetadataRef Replacement) {
  _TheTangoLLVMEnv->TangoLLVMReplaceMDNodeOperandWith(V, Index, Replacement);
}

LLVMValueRef TangoLLVMMDStringInContext(LLVMContextRef C, const char *Str, unsigned SLen) {
   return _TheTangoLLVMEnv->TangoLLVMMDStringInContext(C, Str, SLen);
}

LLVMValueRef TangoLLVMMDString(const char *Str, unsigned SLen) {
   return _TheTangoLLVMEnv->TangoLLVMMDString(Str, SLen);
}

LLVMValueRef TangoLLVMMDNodeInContext(LLVMContextRef C, LLVMValueRef *Vals, unsigned Count) {
   return _TheTangoLLVMEnv->TangoLLVMMDNodeInContext(C, Vals, Count);
}

LLVMValueRef TangoLLVMMDNode(LLVMValueRef *Vals, unsigned Count) {
   return _TheTangoLLVMEnv->TangoLLVMMDNode(Vals, Count);
}

LLVMOperandBundleRef TangoLLVMCreateOperandBundle(const char *Tag, size_t TagLen, LLVMValueRef *Args, unsigned NumArgs) {
   return _TheTangoLLVMEnv->TangoLLVMCreateOperandBundle(Tag, TagLen, Args, NumArgs);
}

void TangoLLVMDisposeOperandBundle(LLVMOperandBundleRef Bundle) {
  _TheTangoLLVMEnv->TangoLLVMDisposeOperandBundle(Bundle);
}

const char *TangoLLVMGetOperandBundleTag(LLVMOperandBundleRef Bundle, size_t *Len) {
   return _TheTangoLLVMEnv->TangoLLVMGetOperandBundleTag(Bundle, Len);
}

unsigned TangoLLVMGetNumOperandBundleArgs(LLVMOperandBundleRef Bundle) {
   return _TheTangoLLVMEnv->TangoLLVMGetNumOperandBundleArgs(Bundle);
}

LLVMValueRef TangoLLVMGetOperandBundleArgAtIndex(LLVMOperandBundleRef Bundle, unsigned Index) {
   return _TheTangoLLVMEnv->TangoLLVMGetOperandBundleArgAtIndex(Bundle, Index);
}

LLVMValueRef TangoLLVMBasicBlockAsValue(LLVMBasicBlockRef BB) {
   return _TheTangoLLVMEnv->TangoLLVMBasicBlockAsValue(BB);
}

LLVMBool TangoLLVMValueIsBasicBlock(LLVMValueRef Val) {
   return _TheTangoLLVMEnv->TangoLLVMValueIsBasicBlock(Val);
}

LLVMBasicBlockRef TangoLLVMValueAsBasicBlock(LLVMValueRef Val) {
   return _TheTangoLLVMEnv->TangoLLVMValueAsBasicBlock(Val);
}

const char *TangoLLVMGetBasicBlockName(LLVMBasicBlockRef BB) {
   return _TheTangoLLVMEnv->TangoLLVMGetBasicBlockName(BB);
}

LLVMValueRef TangoLLVMGetBasicBlockParent(LLVMBasicBlockRef BB) {
   return _TheTangoLLVMEnv->TangoLLVMGetBasicBlockParent(BB);
}

LLVMValueRef TangoLLVMGetBasicBlockTerminator(LLVMBasicBlockRef BB) {
   return _TheTangoLLVMEnv->TangoLLVMGetBasicBlockTerminator(BB);
}

unsigned TangoLLVMCountBasicBlocks(LLVMValueRef Fn) {
   return _TheTangoLLVMEnv->TangoLLVMCountBasicBlocks(Fn);
}

void TangoLLVMGetBasicBlocks(LLVMValueRef Fn, LLVMBasicBlockRef *BasicBlocks) {
  _TheTangoLLVMEnv->TangoLLVMGetBasicBlocks(Fn, BasicBlocks);
}

LLVMBasicBlockRef TangoLLVMGetFirstBasicBlock(LLVMValueRef Fn) {
   return _TheTangoLLVMEnv->TangoLLVMGetFirstBasicBlock(Fn);
}

LLVMBasicBlockRef TangoLLVMGetLastBasicBlock(LLVMValueRef Fn) {
   return _TheTangoLLVMEnv->TangoLLVMGetLastBasicBlock(Fn);
}

LLVMBasicBlockRef TangoLLVMGetNextBasicBlock(LLVMBasicBlockRef BB) {
   return _TheTangoLLVMEnv->TangoLLVMGetNextBasicBlock(BB);
}

LLVMBasicBlockRef TangoLLVMGetPreviousBasicBlock(LLVMBasicBlockRef BB) {
   return _TheTangoLLVMEnv->TangoLLVMGetPreviousBasicBlock(BB);
}

LLVMBasicBlockRef TangoLLVMGetEntryBasicBlock(LLVMValueRef Fn) {
   return _TheTangoLLVMEnv->TangoLLVMGetEntryBasicBlock(Fn);
}

void TangoLLVMInsertExistingBasicBlockAfterInsertBlock(LLVMBuilderRef Builder, LLVMBasicBlockRef BB) {
  _TheTangoLLVMEnv->TangoLLVMInsertExistingBasicBlockAfterInsertBlock(Builder, BB);
}

void TangoLLVMAppendExistingBasicBlock(LLVMValueRef Fn, LLVMBasicBlockRef BB) {
  _TheTangoLLVMEnv->TangoLLVMAppendExistingBasicBlock(Fn, BB);
}

LLVMBasicBlockRef TangoLLVMCreateBasicBlockInContext(LLVMContextRef C, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMCreateBasicBlockInContext(C, Name);
}

LLVMBasicBlockRef TangoLLVMAppendBasicBlockInContext(LLVMContextRef C, LLVMValueRef Fn, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMAppendBasicBlockInContext(C, Fn, Name);
}

LLVMBasicBlockRef TangoLLVMAppendBasicBlock(LLVMValueRef Fn, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMAppendBasicBlock(Fn, Name);
}

LLVMBasicBlockRef TangoLLVMInsertBasicBlockInContext(LLVMContextRef C, LLVMBasicBlockRef BB, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMInsertBasicBlockInContext(C, BB, Name);
}

LLVMBasicBlockRef TangoLLVMInsertBasicBlock(LLVMBasicBlockRef InsertBeforeBB, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMInsertBasicBlock(InsertBeforeBB, Name);
}

void TangoLLVMDeleteBasicBlock(LLVMBasicBlockRef BB) {
  _TheTangoLLVMEnv->TangoLLVMDeleteBasicBlock(BB);
}

void TangoLLVMRemoveBasicBlockFromParent(LLVMBasicBlockRef BB) {
  _TheTangoLLVMEnv->TangoLLVMRemoveBasicBlockFromParent(BB);
}

void TangoLLVMMoveBasicBlockBefore(LLVMBasicBlockRef BB, LLVMBasicBlockRef MovePos) {
  _TheTangoLLVMEnv->TangoLLVMMoveBasicBlockBefore(BB, MovePos);
}

void TangoLLVMMoveBasicBlockAfter(LLVMBasicBlockRef BB, LLVMBasicBlockRef MovePos) {
  _TheTangoLLVMEnv->TangoLLVMMoveBasicBlockAfter(BB, MovePos);
}

LLVMValueRef TangoLLVMGetFirstInstruction(LLVMBasicBlockRef BB) {
   return _TheTangoLLVMEnv->TangoLLVMGetFirstInstruction(BB);
}

LLVMValueRef TangoLLVMGetLastInstruction(LLVMBasicBlockRef BB) {
   return _TheTangoLLVMEnv->TangoLLVMGetLastInstruction(BB);
}

int TangoLLVMHasMetadata(LLVMValueRef Val) {
   return _TheTangoLLVMEnv->TangoLLVMHasMetadata(Val);
}

LLVMValueRef TangoLLVMGetMetadata(LLVMValueRef Val, unsigned KindID) {
   return _TheTangoLLVMEnv->TangoLLVMGetMetadata(Val, KindID);
}

void TangoLLVMSetMetadata(LLVMValueRef Val, unsigned KindID, LLVMValueRef Node) {
  _TheTangoLLVMEnv->TangoLLVMSetMetadata(Val, KindID, Node);
}

LLVMValueMetadataEntry* TangoLLVMInstructionGetAllMetadataOtherThanDebugLoc(LLVMValueRef Instr, size_t *NumEntries) {
   return _TheTangoLLVMEnv->TangoLLVMInstructionGetAllMetadataOtherThanDebugLoc(Instr, NumEntries);
}

LLVMBasicBlockRef TangoLLVMGetInstructionParent(LLVMValueRef Inst) {
   return _TheTangoLLVMEnv->TangoLLVMGetInstructionParent(Inst);
}

LLVMValueRef TangoLLVMGetNextInstruction(LLVMValueRef Inst) {
   return _TheTangoLLVMEnv->TangoLLVMGetNextInstruction(Inst);
}

LLVMValueRef TangoLLVMGetPreviousInstruction(LLVMValueRef Inst) {
   return _TheTangoLLVMEnv->TangoLLVMGetPreviousInstruction(Inst);
}

void TangoLLVMInstructionRemoveFromParent(LLVMValueRef Inst) {
  _TheTangoLLVMEnv->TangoLLVMInstructionRemoveFromParent(Inst);
}

void TangoLLVMInstructionEraseFromParent(LLVMValueRef Inst) {
  _TheTangoLLVMEnv->TangoLLVMInstructionEraseFromParent(Inst);
}

void TangoLLVMDeleteInstruction(LLVMValueRef Inst) {
  _TheTangoLLVMEnv->TangoLLVMDeleteInstruction(Inst);
}

LLVMOpcode TangoLLVMGetInstructionOpcode(LLVMValueRef Inst) {
   return _TheTangoLLVMEnv->TangoLLVMGetInstructionOpcode(Inst);
}

LLVMIntPredicate TangoLLVMGetICmpPredicate(LLVMValueRef Inst) {
   return _TheTangoLLVMEnv->TangoLLVMGetICmpPredicate(Inst);
}

LLVMRealPredicate TangoLLVMGetFCmpPredicate(LLVMValueRef Inst) {
   return _TheTangoLLVMEnv->TangoLLVMGetFCmpPredicate(Inst);
}

LLVMValueRef TangoLLVMInstructionClone(LLVMValueRef Inst) {
   return _TheTangoLLVMEnv->TangoLLVMInstructionClone(Inst);
}

LLVMValueRef TangoLLVMIsATerminatorInst(LLVMValueRef Inst) {
   return _TheTangoLLVMEnv->TangoLLVMIsATerminatorInst(Inst);
}

unsigned TangoLLVMGetNumArgOperands(LLVMValueRef Instr) {
   return _TheTangoLLVMEnv->TangoLLVMGetNumArgOperands(Instr);
}

void TangoLLVMSetInstructionCallConv(LLVMValueRef Instr, unsigned CC) {
  _TheTangoLLVMEnv->TangoLLVMSetInstructionCallConv(Instr, CC);
}

unsigned TangoLLVMGetInstructionCallConv(LLVMValueRef Instr) {
   return _TheTangoLLVMEnv->TangoLLVMGetInstructionCallConv(Instr);
}

void TangoLLVMSetInstrParamAlignment(LLVMValueRef Instr, LLVMAttributeIndex Idx, unsigned Align) {
  _TheTangoLLVMEnv->TangoLLVMSetInstrParamAlignment(Instr, Idx, Align);
}

void TangoLLVMAddCallSiteAttribute(LLVMValueRef C, LLVMAttributeIndex Idx, LLVMAttributeRef A) {
  _TheTangoLLVMEnv->TangoLLVMAddCallSiteAttribute(C, Idx, A);
}

unsigned TangoLLVMGetCallSiteAttributeCount(LLVMValueRef C, LLVMAttributeIndex Idx) {
   return _TheTangoLLVMEnv->TangoLLVMGetCallSiteAttributeCount(C, Idx);
}

void TangoLLVMGetCallSiteAttributes(LLVMValueRef C, LLVMAttributeIndex Idx, LLVMAttributeRef *Attrs) {
  _TheTangoLLVMEnv->TangoLLVMGetCallSiteAttributes(C, Idx, Attrs);
}

LLVMAttributeRef TangoLLVMGetCallSiteEnumAttribute(LLVMValueRef C, LLVMAttributeIndex Idx, unsigned KindID) {
   return _TheTangoLLVMEnv->TangoLLVMGetCallSiteEnumAttribute(C, Idx, KindID);
}

LLVMAttributeRef TangoLLVMGetCallSiteStringAttribute(LLVMValueRef C, LLVMAttributeIndex Idx, const char *K, unsigned KLen) {
   return _TheTangoLLVMEnv->TangoLLVMGetCallSiteStringAttribute(C, Idx, K, KLen);
}

void TangoLLVMRemoveCallSiteEnumAttribute(LLVMValueRef C, LLVMAttributeIndex Idx, unsigned KindID) {
  _TheTangoLLVMEnv->TangoLLVMRemoveCallSiteEnumAttribute(C, Idx, KindID);
}

void TangoLLVMRemoveCallSiteStringAttribute(LLVMValueRef C, LLVMAttributeIndex Idx, const char *K, unsigned KLen) {
  _TheTangoLLVMEnv->TangoLLVMRemoveCallSiteStringAttribute(C, Idx, K, KLen);
}

LLVMTypeRef TangoLLVMGetCalledFunctionType(LLVMValueRef C) {
   return _TheTangoLLVMEnv->TangoLLVMGetCalledFunctionType(C);
}

LLVMValueRef TangoLLVMGetCalledValue(LLVMValueRef Instr) {
   return _TheTangoLLVMEnv->TangoLLVMGetCalledValue(Instr);
}

unsigned TangoLLVMGetNumOperandBundles(LLVMValueRef C) {
   return _TheTangoLLVMEnv->TangoLLVMGetNumOperandBundles(C);
}

LLVMOperandBundleRef TangoLLVMGetOperandBundleAtIndex(LLVMValueRef C, unsigned Index) {
   return _TheTangoLLVMEnv->TangoLLVMGetOperandBundleAtIndex(C, Index);
}

LLVMBool TangoLLVMIsTailCall(LLVMValueRef CallInst) {
   return _TheTangoLLVMEnv->TangoLLVMIsTailCall(CallInst);
}

void TangoLLVMSetTailCall(LLVMValueRef CallInst, LLVMBool IsTailCall) {
  _TheTangoLLVMEnv->TangoLLVMSetTailCall(CallInst, IsTailCall);
}

LLVMTailCallKind TangoLLVMGetTailCallKind(LLVMValueRef CallInst) {
   return _TheTangoLLVMEnv->TangoLLVMGetTailCallKind(CallInst);
}

void TangoLLVMSetTailCallKind(LLVMValueRef CallInst, LLVMTailCallKind kind) {
  _TheTangoLLVMEnv->TangoLLVMSetTailCallKind(CallInst, kind);
}

LLVMBasicBlockRef TangoLLVMGetNormalDest(LLVMValueRef InvokeInst) {
   return _TheTangoLLVMEnv->TangoLLVMGetNormalDest(InvokeInst);
}

LLVMBasicBlockRef TangoLLVMGetUnwindDest(LLVMValueRef InvokeInst) {
   return _TheTangoLLVMEnv->TangoLLVMGetUnwindDest(InvokeInst);
}

void TangoLLVMSetNormalDest(LLVMValueRef InvokeInst, LLVMBasicBlockRef B) {
  _TheTangoLLVMEnv->TangoLLVMSetNormalDest(InvokeInst, B);
}

void TangoLLVMSetUnwindDest(LLVMValueRef InvokeInst, LLVMBasicBlockRef B) {
  _TheTangoLLVMEnv->TangoLLVMSetUnwindDest(InvokeInst, B);
}

LLVMBasicBlockRef TangoLLVMGetCallBrDefaultDest(LLVMValueRef CallBr) {
   return _TheTangoLLVMEnv->TangoLLVMGetCallBrDefaultDest(CallBr);
}

unsigned TangoLLVMGetCallBrNumIndirectDests(LLVMValueRef CallBr) {
   return _TheTangoLLVMEnv->TangoLLVMGetCallBrNumIndirectDests(CallBr);
}

LLVMBasicBlockRef TangoLLVMGetCallBrIndirectDest(LLVMValueRef CallBr, unsigned Idx) {
   return _TheTangoLLVMEnv->TangoLLVMGetCallBrIndirectDest(CallBr, Idx);
}

unsigned TangoLLVMGetNumSuccessors(LLVMValueRef Term) {
   return _TheTangoLLVMEnv->TangoLLVMGetNumSuccessors(Term);
}

LLVMBasicBlockRef TangoLLVMGetSuccessor(LLVMValueRef Term, unsigned i) {
   return _TheTangoLLVMEnv->TangoLLVMGetSuccessor(Term, i);
}

void TangoLLVMSetSuccessor(LLVMValueRef Term, unsigned i, LLVMBasicBlockRef block) {
  _TheTangoLLVMEnv->TangoLLVMSetSuccessor(Term, i, block);
}

LLVMBool TangoLLVMIsConditional(LLVMValueRef Branch) {
   return _TheTangoLLVMEnv->TangoLLVMIsConditional(Branch);
}

LLVMValueRef TangoLLVMGetCondition(LLVMValueRef Branch) {
   return _TheTangoLLVMEnv->TangoLLVMGetCondition(Branch);
}

void TangoLLVMSetCondition(LLVMValueRef Branch, LLVMValueRef Cond) {
  _TheTangoLLVMEnv->TangoLLVMSetCondition(Branch, Cond);
}

LLVMBasicBlockRef TangoLLVMGetSwitchDefaultDest(LLVMValueRef SwitchInstr) {
   return _TheTangoLLVMEnv->TangoLLVMGetSwitchDefaultDest(SwitchInstr);
}

LLVMTypeRef TangoLLVMGetAllocatedType(LLVMValueRef Alloca) {
   return _TheTangoLLVMEnv->TangoLLVMGetAllocatedType(Alloca);
}

LLVMBool TangoLLVMIsInBounds(LLVMValueRef GEP) {
   return _TheTangoLLVMEnv->TangoLLVMIsInBounds(GEP);
}

void TangoLLVMSetIsInBounds(LLVMValueRef GEP, LLVMBool InBounds) {
  _TheTangoLLVMEnv->TangoLLVMSetIsInBounds(GEP, InBounds);
}

LLVMTypeRef TangoLLVMGetGEPSourceElementType(LLVMValueRef GEP) {
   return _TheTangoLLVMEnv->TangoLLVMGetGEPSourceElementType(GEP);
}

LLVMGEPNoWrapFlags TangoLLVMGEPGetNoWrapFlags(LLVMValueRef GEP) {
   return _TheTangoLLVMEnv->TangoLLVMGEPGetNoWrapFlags(GEP);
}

void TangoLLVMGEPSetNoWrapFlags(LLVMValueRef GEP, LLVMGEPNoWrapFlags NoWrapFlags) {
  _TheTangoLLVMEnv->TangoLLVMGEPSetNoWrapFlags(GEP, NoWrapFlags);
}

void TangoLLVMAddIncoming(LLVMValueRef PhiNode, LLVMValueRef *IncomingValues, LLVMBasicBlockRef *IncomingBlocks, unsigned Count) {
  _TheTangoLLVMEnv->TangoLLVMAddIncoming(PhiNode, IncomingValues, IncomingBlocks, Count);
}

unsigned TangoLLVMCountIncoming(LLVMValueRef PhiNode) {
   return _TheTangoLLVMEnv->TangoLLVMCountIncoming(PhiNode);
}

LLVMValueRef TangoLLVMGetIncomingValue(LLVMValueRef PhiNode, unsigned Index) {
   return _TheTangoLLVMEnv->TangoLLVMGetIncomingValue(PhiNode, Index);
}

LLVMBasicBlockRef TangoLLVMGetIncomingBlock(LLVMValueRef PhiNode, unsigned Index) {
   return _TheTangoLLVMEnv->TangoLLVMGetIncomingBlock(PhiNode, Index);
}

unsigned TangoLLVMGetNumIndices(LLVMValueRef Inst) {
   return _TheTangoLLVMEnv->TangoLLVMGetNumIndices(Inst);
}

const unsigned *TangoLLVMGetIndices(LLVMValueRef Inst) {
   return _TheTangoLLVMEnv->TangoLLVMGetIndices(Inst);
}

LLVMBuilderRef TangoLLVMCreateBuilderInContext(LLVMContextRef C) {
   return _TheTangoLLVMEnv->TangoLLVMCreateBuilderInContext(C);
}

LLVMBuilderRef TangoLLVMCreateBuilder(void) {
   return _TheTangoLLVMEnv->TangoLLVMCreateBuilder();
}

void TangoLLVMPositionBuilder(LLVMBuilderRef Builder, LLVMBasicBlockRef Block, LLVMValueRef Instr) {
  _TheTangoLLVMEnv->TangoLLVMPositionBuilder(Builder, Block, Instr);
}

void TangoLLVMPositionBuilderBeforeDbgRecords(LLVMBuilderRef Builder, LLVMBasicBlockRef Block, LLVMValueRef Inst) {
  _TheTangoLLVMEnv->TangoLLVMPositionBuilderBeforeDbgRecords(Builder, Block, Inst);
}

void TangoLLVMPositionBuilderBefore(LLVMBuilderRef Builder, LLVMValueRef Instr) {
  _TheTangoLLVMEnv->TangoLLVMPositionBuilderBefore(Builder, Instr);
}

void TangoLLVMPositionBuilderBeforeInstrAndDbgRecords(LLVMBuilderRef Builder, LLVMValueRef Instr) {
  _TheTangoLLVMEnv->TangoLLVMPositionBuilderBeforeInstrAndDbgRecords(Builder, Instr);
}

void TangoLLVMPositionBuilderAtEnd(LLVMBuilderRef Builder, LLVMBasicBlockRef Block) {
  _TheTangoLLVMEnv->TangoLLVMPositionBuilderAtEnd(Builder, Block);
}

LLVMBasicBlockRef TangoLLVMGetInsertBlock(LLVMBuilderRef Builder) {
   return _TheTangoLLVMEnv->TangoLLVMGetInsertBlock(Builder);
}

void TangoLLVMClearInsertionPosition(LLVMBuilderRef Builder) {
  _TheTangoLLVMEnv->TangoLLVMClearInsertionPosition(Builder);
}

void TangoLLVMInsertIntoBuilder(LLVMBuilderRef Builder, LLVMValueRef Instr) {
  _TheTangoLLVMEnv->TangoLLVMInsertIntoBuilder(Builder, Instr);
}

void TangoLLVMInsertIntoBuilderWithName(LLVMBuilderRef Builder, LLVMValueRef Instr, const char *Name) {
  _TheTangoLLVMEnv->TangoLLVMInsertIntoBuilderWithName(Builder, Instr, Name);
}

void TangoLLVMDisposeBuilder(LLVMBuilderRef Builder) {
  _TheTangoLLVMEnv->TangoLLVMDisposeBuilder(Builder);
}

LLVMMetadataRef TangoLLVMGetCurrentDebugLocation2(LLVMBuilderRef Builder) {
   return _TheTangoLLVMEnv->TangoLLVMGetCurrentDebugLocation2(Builder);
}

void TangoLLVMSetCurrentDebugLocation2(LLVMBuilderRef Builder, LLVMMetadataRef Loc) {
  _TheTangoLLVMEnv->TangoLLVMSetCurrentDebugLocation2(Builder, Loc);
}

void TangoLLVMSetInstDebugLocation(LLVMBuilderRef Builder, LLVMValueRef Inst) {
  _TheTangoLLVMEnv->TangoLLVMSetInstDebugLocation(Builder, Inst);
}

void TangoLLVMAddMetadataToInst(LLVMBuilderRef Builder, LLVMValueRef Inst) {
  _TheTangoLLVMEnv->TangoLLVMAddMetadataToInst(Builder, Inst);
}

LLVMMetadataRef TangoLLVMBuilderGetDefaultFPMathTag(LLVMBuilderRef Builder) {
   return _TheTangoLLVMEnv->TangoLLVMBuilderGetDefaultFPMathTag(Builder);
}

void TangoLLVMBuilderSetDefaultFPMathTag(LLVMBuilderRef Builder, LLVMMetadataRef FPMathTag) {
  _TheTangoLLVMEnv->TangoLLVMBuilderSetDefaultFPMathTag(Builder, FPMathTag);
}

void TangoLLVMSetCurrentDebugLocation(LLVMBuilderRef Builder, LLVMValueRef L) {
  _TheTangoLLVMEnv->TangoLLVMSetCurrentDebugLocation(Builder, L);
}

LLVMValueRef TangoLLVMGetCurrentDebugLocation(LLVMBuilderRef Builder) {
   return _TheTangoLLVMEnv->TangoLLVMGetCurrentDebugLocation(Builder);
}

LLVMValueRef TangoLLVMBuildRetVoid(LLVMBuilderRef Ref) {
   return _TheTangoLLVMEnv->TangoLLVMBuildRetVoid(Ref);
}

LLVMValueRef TangoLLVMBuildRet(LLVMBuilderRef Builder, LLVMValueRef V) {
   return _TheTangoLLVMEnv->TangoLLVMBuildRet(Builder, V);
}

LLVMValueRef TangoLLVMBuildAggregateRet(LLVMBuilderRef Builder, LLVMValueRef *RetVals, unsigned N) {
   return _TheTangoLLVMEnv->TangoLLVMBuildAggregateRet(Builder, RetVals, N);
}

LLVMValueRef TangoLLVMBuildBr(LLVMBuilderRef Builder, LLVMBasicBlockRef Dest) {
   return _TheTangoLLVMEnv->TangoLLVMBuildBr(Builder, Dest);
}

LLVMValueRef TangoLLVMBuildCondBr(LLVMBuilderRef Builder, LLVMValueRef If, LLVMBasicBlockRef Then, LLVMBasicBlockRef Else) {
   return _TheTangoLLVMEnv->TangoLLVMBuildCondBr(Builder, If, Then, Else);
}

LLVMValueRef TangoLLVMBuildSwitch(LLVMBuilderRef Builder, LLVMValueRef V, LLVMBasicBlockRef Else, unsigned NumCases) {
   return _TheTangoLLVMEnv->TangoLLVMBuildSwitch(Builder, V, Else, NumCases);
}

LLVMValueRef TangoLLVMBuildIndirectBr(LLVMBuilderRef B, LLVMValueRef Addr, unsigned NumDests) {
   return _TheTangoLLVMEnv->TangoLLVMBuildIndirectBr(B, Addr, NumDests);
}

LLVMValueRef TangoLLVMBuildCallBr(LLVMBuilderRef B, LLVMTypeRef Ty, LLVMValueRef Fn, LLVMBasicBlockRef DefaultDest, LLVMBasicBlockRef *IndirectDests, unsigned NumIndirectDests, LLVMValueRef *Args, unsigned NumArgs, LLVMOperandBundleRef *Bundles, unsigned NumBundles, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildCallBr(B, Ty, Fn, DefaultDest, IndirectDests, NumIndirectDests, Args, NumArgs, Bundles, NumBundles, Name);
}

LLVMValueRef TangoLLVMBuildInvoke2(LLVMBuilderRef Builder, LLVMTypeRef Ty, LLVMValueRef Fn, LLVMValueRef *Args, unsigned NumArgs, LLVMBasicBlockRef Then, LLVMBasicBlockRef Catch, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildInvoke2(Builder, Ty, Fn, Args, NumArgs, Then, Catch, Name);
}

LLVMValueRef TangoLLVMBuildInvokeWithOperandBundles(LLVMBuilderRef Builder, LLVMTypeRef Ty, LLVMValueRef Fn, LLVMValueRef *Args, unsigned NumArgs, LLVMBasicBlockRef Then, LLVMBasicBlockRef Catch, LLVMOperandBundleRef *Bundles, unsigned NumBundles, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildInvokeWithOperandBundles(Builder, Ty, Fn, Args, NumArgs, Then, Catch, Bundles, NumBundles, Name);
}

LLVMValueRef TangoLLVMBuildUnreachable(LLVMBuilderRef Builder) {
   return _TheTangoLLVMEnv->TangoLLVMBuildUnreachable(Builder);
}

LLVMValueRef TangoLLVMBuildResume(LLVMBuilderRef B, LLVMValueRef Exn) {
   return _TheTangoLLVMEnv->TangoLLVMBuildResume(B, Exn);
}

LLVMValueRef TangoLLVMBuildLandingPad(LLVMBuilderRef B, LLVMTypeRef Ty, LLVMValueRef PersFn, unsigned NumClauses, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildLandingPad(B, Ty, PersFn, NumClauses, Name);
}

LLVMValueRef TangoLLVMBuildCleanupRet(LLVMBuilderRef B, LLVMValueRef CatchPad, LLVMBasicBlockRef BB) {
   return _TheTangoLLVMEnv->TangoLLVMBuildCleanupRet(B, CatchPad, BB);
}

LLVMValueRef TangoLLVMBuildCatchRet(LLVMBuilderRef B, LLVMValueRef CatchPad, LLVMBasicBlockRef BB) {
   return _TheTangoLLVMEnv->TangoLLVMBuildCatchRet(B, CatchPad, BB);
}

LLVMValueRef TangoLLVMBuildCatchPad(LLVMBuilderRef B, LLVMValueRef ParentPad, LLVMValueRef *Args, unsigned NumArgs, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildCatchPad(B, ParentPad, Args, NumArgs, Name);
}

LLVMValueRef TangoLLVMBuildCleanupPad(LLVMBuilderRef B, LLVMValueRef ParentPad, LLVMValueRef *Args, unsigned NumArgs, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildCleanupPad(B, ParentPad, Args, NumArgs, Name);
}

LLVMValueRef TangoLLVMBuildCatchSwitch(LLVMBuilderRef B, LLVMValueRef ParentPad, LLVMBasicBlockRef UnwindBB, unsigned NumHandlers, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildCatchSwitch(B, ParentPad, UnwindBB, NumHandlers, Name);
}

void TangoLLVMAddCase(LLVMValueRef Switch, LLVMValueRef OnVal, LLVMBasicBlockRef Dest) {
  _TheTangoLLVMEnv->TangoLLVMAddCase(Switch, OnVal, Dest);
}

void TangoLLVMAddDestination(LLVMValueRef IndirectBr, LLVMBasicBlockRef Dest) {
  _TheTangoLLVMEnv->TangoLLVMAddDestination(IndirectBr, Dest);
}

unsigned TangoLLVMGetNumClauses(LLVMValueRef LandingPad) {
   return _TheTangoLLVMEnv->TangoLLVMGetNumClauses(LandingPad);
}

LLVMValueRef TangoLLVMGetClause(LLVMValueRef LandingPad, unsigned Idx) {
   return _TheTangoLLVMEnv->TangoLLVMGetClause(LandingPad, Idx);
}

void TangoLLVMAddClause(LLVMValueRef LandingPad, LLVMValueRef ClauseVal) {
  _TheTangoLLVMEnv->TangoLLVMAddClause(LandingPad, ClauseVal);
}

LLVMBool TangoLLVMIsCleanup(LLVMValueRef LandingPad) {
   return _TheTangoLLVMEnv->TangoLLVMIsCleanup(LandingPad);
}

void TangoLLVMSetCleanup(LLVMValueRef LandingPad, LLVMBool Val) {
  _TheTangoLLVMEnv->TangoLLVMSetCleanup(LandingPad, Val);
}

void TangoLLVMAddHandler(LLVMValueRef CatchSwitch, LLVMBasicBlockRef Dest) {
  _TheTangoLLVMEnv->TangoLLVMAddHandler(CatchSwitch, Dest);
}

unsigned TangoLLVMGetNumHandlers(LLVMValueRef CatchSwitch) {
   return _TheTangoLLVMEnv->TangoLLVMGetNumHandlers(CatchSwitch);
}

void TangoLLVMGetHandlers(LLVMValueRef CatchSwitch, LLVMBasicBlockRef *Handlers) {
  _TheTangoLLVMEnv->TangoLLVMGetHandlers(CatchSwitch, Handlers);
}

LLVMValueRef TangoLLVMGetArgOperand(LLVMValueRef Funclet, unsigned i) {
   return _TheTangoLLVMEnv->TangoLLVMGetArgOperand(Funclet, i);
}

void TangoLLVMSetArgOperand(LLVMValueRef Funclet, unsigned i, LLVMValueRef value) {
  _TheTangoLLVMEnv->TangoLLVMSetArgOperand(Funclet, i, value);
}

LLVMValueRef TangoLLVMGetParentCatchSwitch(LLVMValueRef CatchPad) {
   return _TheTangoLLVMEnv->TangoLLVMGetParentCatchSwitch(CatchPad);
}

void TangoLLVMSetParentCatchSwitch(LLVMValueRef CatchPad, LLVMValueRef CatchSwitch) {
  _TheTangoLLVMEnv->TangoLLVMSetParentCatchSwitch(CatchPad, CatchSwitch);
}

LLVMValueRef TangoLLVMBuildAdd(LLVMBuilderRef Builder, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildAdd(Builder, LHS, RHS, Name);
}

LLVMValueRef TangoLLVMBuildNSWAdd(LLVMBuilderRef Builder, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildNSWAdd(Builder, LHS, RHS, Name);
}

LLVMValueRef TangoLLVMBuildNUWAdd(LLVMBuilderRef Builder, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildNUWAdd(Builder, LHS, RHS, Name);
}

LLVMValueRef TangoLLVMBuildFAdd(LLVMBuilderRef Builder, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildFAdd(Builder, LHS, RHS, Name);
}

LLVMValueRef TangoLLVMBuildSub(LLVMBuilderRef Builder, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildSub(Builder, LHS, RHS, Name);
}

LLVMValueRef TangoLLVMBuildNSWSub(LLVMBuilderRef Builder, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildNSWSub(Builder, LHS, RHS, Name);
}

LLVMValueRef TangoLLVMBuildNUWSub(LLVMBuilderRef Builder, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildNUWSub(Builder, LHS, RHS, Name);
}

LLVMValueRef TangoLLVMBuildFSub(LLVMBuilderRef Builder, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildFSub(Builder, LHS, RHS, Name);
}

LLVMValueRef TangoLLVMBuildMul(LLVMBuilderRef Builder, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildMul(Builder, LHS, RHS, Name);
}

LLVMValueRef TangoLLVMBuildNSWMul(LLVMBuilderRef Builder, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildNSWMul(Builder, LHS, RHS, Name);
}

LLVMValueRef TangoLLVMBuildNUWMul(LLVMBuilderRef Builder, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildNUWMul(Builder, LHS, RHS, Name);
}

LLVMValueRef TangoLLVMBuildFMul(LLVMBuilderRef Builder, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildFMul(Builder, LHS, RHS, Name);
}

LLVMValueRef TangoLLVMBuildUDiv(LLVMBuilderRef Builder, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildUDiv(Builder, LHS, RHS, Name);
}

LLVMValueRef TangoLLVMBuildExactUDiv(LLVMBuilderRef Builder, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildExactUDiv(Builder, LHS, RHS, Name);
}

LLVMValueRef TangoLLVMBuildSDiv(LLVMBuilderRef Builder, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildSDiv(Builder, LHS, RHS, Name);
}

LLVMValueRef TangoLLVMBuildExactSDiv(LLVMBuilderRef Builder, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildExactSDiv(Builder, LHS, RHS, Name);
}

LLVMValueRef TangoLLVMBuildFDiv(LLVMBuilderRef Builder, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildFDiv(Builder, LHS, RHS, Name);
}

LLVMValueRef TangoLLVMBuildURem(LLVMBuilderRef Builder, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildURem(Builder, LHS, RHS, Name);
}

LLVMValueRef TangoLLVMBuildSRem(LLVMBuilderRef Builder, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildSRem(Builder, LHS, RHS, Name);
}

LLVMValueRef TangoLLVMBuildFRem(LLVMBuilderRef Builder, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildFRem(Builder, LHS, RHS, Name);
}

LLVMValueRef TangoLLVMBuildShl(LLVMBuilderRef Builder, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildShl(Builder, LHS, RHS, Name);
}

LLVMValueRef TangoLLVMBuildLShr(LLVMBuilderRef Builder, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildLShr(Builder, LHS, RHS, Name);
}

LLVMValueRef TangoLLVMBuildAShr(LLVMBuilderRef Builder, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildAShr(Builder, LHS, RHS, Name);
}

LLVMValueRef TangoLLVMBuildAnd(LLVMBuilderRef Builder, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildAnd(Builder, LHS, RHS, Name);
}

LLVMValueRef TangoLLVMBuildOr(LLVMBuilderRef Builder, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildOr(Builder, LHS, RHS, Name);
}

LLVMValueRef TangoLLVMBuildXor(LLVMBuilderRef Builder, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildXor(Builder, LHS, RHS, Name);
}

LLVMValueRef TangoLLVMBuildBinOp(LLVMBuilderRef B, LLVMOpcode Op, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildBinOp(B, Op, LHS, RHS, Name);
}

LLVMValueRef TangoLLVMBuildNeg(LLVMBuilderRef Builder, LLVMValueRef V, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildNeg(Builder, V, Name);
}

LLVMValueRef TangoLLVMBuildNSWNeg(LLVMBuilderRef B, LLVMValueRef V, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildNSWNeg(B, V, Name);
}

LLVMValueRef TangoLLVMBuildNUWNeg(LLVMBuilderRef B, LLVMValueRef V, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildNUWNeg(B, V, Name);
}

LLVMValueRef TangoLLVMBuildFNeg(LLVMBuilderRef Builder, LLVMValueRef V, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildFNeg(Builder, V, Name);
}

LLVMValueRef TangoLLVMBuildNot(LLVMBuilderRef Builder, LLVMValueRef V, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildNot(Builder, V, Name);
}

LLVMBool TangoLLVMGetNUW(LLVMValueRef ArithInst) {
   return _TheTangoLLVMEnv->TangoLLVMGetNUW(ArithInst);
}

void TangoLLVMSetNUW(LLVMValueRef ArithInst, LLVMBool HasNUW) {
  _TheTangoLLVMEnv->TangoLLVMSetNUW(ArithInst, HasNUW);
}

LLVMBool TangoLLVMGetNSW(LLVMValueRef ArithInst) {
   return _TheTangoLLVMEnv->TangoLLVMGetNSW(ArithInst);
}

void TangoLLVMSetNSW(LLVMValueRef ArithInst, LLVMBool HasNSW) {
  _TheTangoLLVMEnv->TangoLLVMSetNSW(ArithInst, HasNSW);
}

LLVMBool TangoLLVMGetExact(LLVMValueRef DivOrShrInst) {
   return _TheTangoLLVMEnv->TangoLLVMGetExact(DivOrShrInst);
}

void TangoLLVMSetExact(LLVMValueRef DivOrShrInst, LLVMBool IsExact) {
  _TheTangoLLVMEnv->TangoLLVMSetExact(DivOrShrInst, IsExact);
}

LLVMBool TangoLLVMGetNNeg(LLVMValueRef NonNegInst) {
   return _TheTangoLLVMEnv->TangoLLVMGetNNeg(NonNegInst);
}

void TangoLLVMSetNNeg(LLVMValueRef NonNegInst, LLVMBool IsNonNeg) {
  _TheTangoLLVMEnv->TangoLLVMSetNNeg(NonNegInst, IsNonNeg);
}

LLVMFastMathFlags TangoLLVMGetFastMathFlags(LLVMValueRef FPMathInst) {
   return _TheTangoLLVMEnv->TangoLLVMGetFastMathFlags(FPMathInst);
}

void TangoLLVMSetFastMathFlags(LLVMValueRef FPMathInst, LLVMFastMathFlags FMF) {
  _TheTangoLLVMEnv->TangoLLVMSetFastMathFlags(FPMathInst, FMF);
}

LLVMBool TangoLLVMCanValueUseFastMathFlags(LLVMValueRef Inst) {
   return _TheTangoLLVMEnv->TangoLLVMCanValueUseFastMathFlags(Inst);
}

LLVMBool TangoLLVMGetIsDisjoint(LLVMValueRef Inst) {
   return _TheTangoLLVMEnv->TangoLLVMGetIsDisjoint(Inst);
}

void TangoLLVMSetIsDisjoint(LLVMValueRef Inst, LLVMBool IsDisjoint) {
  _TheTangoLLVMEnv->TangoLLVMSetIsDisjoint(Inst, IsDisjoint);
}

LLVMValueRef TangoLLVMBuildMalloc(LLVMBuilderRef Builder, LLVMTypeRef Ty, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildMalloc(Builder, Ty, Name);
}

LLVMValueRef TangoLLVMBuildArrayMalloc(LLVMBuilderRef Builder, LLVMTypeRef Ty, LLVMValueRef Val, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildArrayMalloc(Builder, Ty, Val, Name);
}

LLVMValueRef TangoLLVMBuildMemSet(LLVMBuilderRef B, LLVMValueRef Ptr, LLVMValueRef Val, LLVMValueRef Len, unsigned Align) {
   return _TheTangoLLVMEnv->TangoLLVMBuildMemSet(B, Ptr, Val, Len, Align);
}

LLVMValueRef TangoLLVMBuildMemCpy(LLVMBuilderRef B, LLVMValueRef Dst, unsigned DstAlign, LLVMValueRef Src, unsigned SrcAlign, LLVMValueRef Size) {
   return _TheTangoLLVMEnv->TangoLLVMBuildMemCpy(B, Dst, DstAlign, Src, SrcAlign, Size);
}

LLVMValueRef TangoLLVMBuildMemMove(LLVMBuilderRef B, LLVMValueRef Dst, unsigned DstAlign, LLVMValueRef Src, unsigned SrcAlign, LLVMValueRef Size) {
   return _TheTangoLLVMEnv->TangoLLVMBuildMemMove(B, Dst, DstAlign, Src, SrcAlign, Size);
}

LLVMValueRef TangoLLVMBuildAlloca(LLVMBuilderRef Builder, LLVMTypeRef Ty, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildAlloca(Builder, Ty, Name);
}

LLVMValueRef TangoLLVMBuildArrayAlloca(LLVMBuilderRef Builder, LLVMTypeRef Ty, LLVMValueRef Val, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildArrayAlloca(Builder, Ty, Val, Name);
}

LLVMValueRef TangoLLVMBuildFree(LLVMBuilderRef Builder, LLVMValueRef PointerVal) {
   return _TheTangoLLVMEnv->TangoLLVMBuildFree(Builder, PointerVal);
}

LLVMValueRef TangoLLVMBuildLoad2(LLVMBuilderRef Builder, LLVMTypeRef Ty, LLVMValueRef PointerVal, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildLoad2(Builder, Ty, PointerVal, Name);
}

LLVMValueRef TangoLLVMBuildStore(LLVMBuilderRef Builder, LLVMValueRef Val, LLVMValueRef Ptr) {
   return _TheTangoLLVMEnv->TangoLLVMBuildStore(Builder, Val, Ptr);
}

LLVMValueRef TangoLLVMBuildGEP2(LLVMBuilderRef B, LLVMTypeRef Ty, LLVMValueRef Pointer, LLVMValueRef *Indices, unsigned NumIndices, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildGEP2(B, Ty, Pointer, Indices, NumIndices, Name);
}

LLVMValueRef TangoLLVMBuildInBoundsGEP2(LLVMBuilderRef B, LLVMTypeRef Ty, LLVMValueRef Pointer, LLVMValueRef *Indices, unsigned NumIndices, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildInBoundsGEP2(B, Ty, Pointer, Indices, NumIndices, Name);
}

LLVMValueRef TangoLLVMBuildGEPWithNoWrapFlags(LLVMBuilderRef B, LLVMTypeRef Ty, LLVMValueRef Pointer, LLVMValueRef *Indices, unsigned NumIndices, const char *Name, LLVMGEPNoWrapFlags NoWrapFlags) {
   return _TheTangoLLVMEnv->TangoLLVMBuildGEPWithNoWrapFlags(B, Ty, Pointer, Indices, NumIndices, Name, NoWrapFlags);
}

LLVMValueRef TangoLLVMBuildStructGEP2(LLVMBuilderRef B, LLVMTypeRef Ty, LLVMValueRef Pointer, unsigned Idx, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildStructGEP2(B, Ty, Pointer, Idx, Name);
}

LLVMValueRef TangoLLVMBuildGlobalString(LLVMBuilderRef B, const char *Str, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildGlobalString(B, Str, Name);
}

LLVMValueRef TangoLLVMBuildGlobalStringPtr(LLVMBuilderRef B, const char *Str, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildGlobalStringPtr(B, Str, Name);
}

LLVMBool TangoLLVMGetVolatile(LLVMValueRef MemoryAccessInst) {
   return _TheTangoLLVMEnv->TangoLLVMGetVolatile(MemoryAccessInst);
}

void TangoLLVMSetVolatile(LLVMValueRef MemoryAccessInst, LLVMBool IsVolatile) {
  _TheTangoLLVMEnv->TangoLLVMSetVolatile(MemoryAccessInst, IsVolatile);
}

LLVMBool TangoLLVMGetWeak(LLVMValueRef CmpXchgInst) {
   return _TheTangoLLVMEnv->TangoLLVMGetWeak(CmpXchgInst);
}

void TangoLLVMSetWeak(LLVMValueRef CmpXchgInst, LLVMBool IsWeak) {
  _TheTangoLLVMEnv->TangoLLVMSetWeak(CmpXchgInst, IsWeak);
}

LLVMAtomicOrdering TangoLLVMGetOrdering(LLVMValueRef MemoryAccessInst) {
   return _TheTangoLLVMEnv->TangoLLVMGetOrdering(MemoryAccessInst);
}

void TangoLLVMSetOrdering(LLVMValueRef MemoryAccessInst, LLVMAtomicOrdering Ordering) {
  _TheTangoLLVMEnv->TangoLLVMSetOrdering(MemoryAccessInst, Ordering);
}

LLVMAtomicRMWBinOp TangoLLVMGetAtomicRMWBinOp(LLVMValueRef AtomicRMWInst) {
   return _TheTangoLLVMEnv->TangoLLVMGetAtomicRMWBinOp(AtomicRMWInst);
}

void TangoLLVMSetAtomicRMWBinOp(LLVMValueRef AtomicRMWInst, LLVMAtomicRMWBinOp BinOp) {
  _TheTangoLLVMEnv->TangoLLVMSetAtomicRMWBinOp(AtomicRMWInst, BinOp);
}

LLVMValueRef TangoLLVMBuildTrunc(LLVMBuilderRef Builder, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildTrunc(Builder, Val, DestTy, Name);
}

LLVMValueRef TangoLLVMBuildZExt(LLVMBuilderRef Builder, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildZExt(Builder, Val, DestTy, Name);
}

LLVMValueRef TangoLLVMBuildSExt(LLVMBuilderRef Builder, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildSExt(Builder, Val, DestTy, Name);
}

LLVMValueRef TangoLLVMBuildFPToUI(LLVMBuilderRef Builder, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildFPToUI(Builder, Val, DestTy, Name);
}

LLVMValueRef TangoLLVMBuildFPToSI(LLVMBuilderRef Builder, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildFPToSI(Builder, Val, DestTy, Name);
}

LLVMValueRef TangoLLVMBuildUIToFP(LLVMBuilderRef Builder, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildUIToFP(Builder, Val, DestTy, Name);
}

LLVMValueRef TangoLLVMBuildSIToFP(LLVMBuilderRef Builder, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildSIToFP(Builder, Val, DestTy, Name);
}

LLVMValueRef TangoLLVMBuildFPTrunc(LLVMBuilderRef Builder, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildFPTrunc(Builder, Val, DestTy, Name);
}

LLVMValueRef TangoLLVMBuildFPExt(LLVMBuilderRef Builder, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildFPExt(Builder, Val, DestTy, Name);
}

LLVMValueRef TangoLLVMBuildPtrToInt(LLVMBuilderRef Builder, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildPtrToInt(Builder, Val, DestTy, Name);
}

LLVMValueRef TangoLLVMBuildIntToPtr(LLVMBuilderRef Builder, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildIntToPtr(Builder, Val, DestTy, Name);
}

LLVMValueRef TangoLLVMBuildBitCast(LLVMBuilderRef Builder, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildBitCast(Builder, Val, DestTy, Name);
}

LLVMValueRef TangoLLVMBuildAddrSpaceCast(LLVMBuilderRef Builder, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildAddrSpaceCast(Builder, Val, DestTy, Name);
}

LLVMValueRef TangoLLVMBuildZExtOrBitCast(LLVMBuilderRef Builder, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildZExtOrBitCast(Builder, Val, DestTy, Name);
}

LLVMValueRef TangoLLVMBuildSExtOrBitCast(LLVMBuilderRef Builder, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildSExtOrBitCast(Builder, Val, DestTy, Name);
}

LLVMValueRef TangoLLVMBuildTruncOrBitCast(LLVMBuilderRef Builder, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildTruncOrBitCast(Builder, Val, DestTy, Name);
}

LLVMValueRef TangoLLVMBuildCast(LLVMBuilderRef B, LLVMOpcode Op, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildCast(B, Op, Val, DestTy, Name);
}

LLVMValueRef TangoLLVMBuildPointerCast(LLVMBuilderRef Builder, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildPointerCast(Builder, Val, DestTy, Name);
}

LLVMValueRef TangoLLVMBuildIntCast2(LLVMBuilderRef Builder, LLVMValueRef Val, LLVMTypeRef DestTy, LLVMBool IsSigned, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildIntCast2(Builder, Val, DestTy, IsSigned, Name);
}

LLVMValueRef TangoLLVMBuildFPCast(LLVMBuilderRef Builder, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildFPCast(Builder, Val, DestTy, Name);
}

LLVMValueRef TangoLLVMBuildIntCast(LLVMBuilderRef Builder, LLVMValueRef Val, LLVMTypeRef DestTy, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildIntCast(Builder, Val, DestTy, Name);
}

LLVMOpcode TangoLLVMGetCastOpcode(LLVMValueRef Src, LLVMBool SrcIsSigned, LLVMTypeRef DestTy, LLVMBool DestIsSigned) {
   return _TheTangoLLVMEnv->TangoLLVMGetCastOpcode(Src, SrcIsSigned, DestTy, DestIsSigned);
}

LLVMValueRef TangoLLVMBuildICmp(LLVMBuilderRef Builder, LLVMIntPredicate Op, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildICmp(Builder, Op, LHS, RHS, Name);
}

LLVMValueRef TangoLLVMBuildFCmp(LLVMBuilderRef Builder, LLVMRealPredicate Op, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildFCmp(Builder, Op, LHS, RHS, Name);
}

LLVMValueRef TangoLLVMBuildPhi(LLVMBuilderRef Builder, LLVMTypeRef Ty, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildPhi(Builder, Ty, Name);
}

LLVMValueRef TangoLLVMBuildCall2(LLVMBuilderRef Builder, LLVMTypeRef Ty, LLVMValueRef Fn, LLVMValueRef *Args, unsigned NumArgs, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildCall2(Builder, Ty, Fn, Args, NumArgs, Name);
}

LLVMValueRef TangoLLVMBuildCallWithOperandBundles(LLVMBuilderRef Builder, LLVMTypeRef Ty, LLVMValueRef Fn, LLVMValueRef *Args, unsigned NumArgs, LLVMOperandBundleRef *Bundles, unsigned NumBundles, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildCallWithOperandBundles(Builder, Ty, Fn, Args, NumArgs, Bundles, NumBundles, Name);
}

LLVMValueRef TangoLLVMBuildSelect(LLVMBuilderRef Builder, LLVMValueRef If, LLVMValueRef Then, LLVMValueRef Else, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildSelect(Builder, If, Then, Else, Name);
}

LLVMValueRef TangoLLVMBuildVAArg(LLVMBuilderRef Builder, LLVMValueRef List, LLVMTypeRef Ty, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildVAArg(Builder, List, Ty, Name);
}

LLVMValueRef TangoLLVMBuildExtractElement(LLVMBuilderRef Builder, LLVMValueRef VecVal, LLVMValueRef Index, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildExtractElement(Builder, VecVal, Index, Name);
}

LLVMValueRef TangoLLVMBuildInsertElement(LLVMBuilderRef Builder, LLVMValueRef VecVal, LLVMValueRef EltVal, LLVMValueRef Index, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildInsertElement(Builder, VecVal, EltVal, Index, Name);
}

LLVMValueRef TangoLLVMBuildShuffleVector(LLVMBuilderRef Builder, LLVMValueRef V1, LLVMValueRef V2, LLVMValueRef Mask, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildShuffleVector(Builder, V1, V2, Mask, Name);
}

LLVMValueRef TangoLLVMBuildExtractValue(LLVMBuilderRef Builder, LLVMValueRef AggVal, unsigned Index, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildExtractValue(Builder, AggVal, Index, Name);
}

LLVMValueRef TangoLLVMBuildInsertValue(LLVMBuilderRef Builder, LLVMValueRef AggVal, LLVMValueRef EltVal, unsigned Index, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildInsertValue(Builder, AggVal, EltVal, Index, Name);
}

LLVMValueRef TangoLLVMBuildFreeze(LLVMBuilderRef Builder, LLVMValueRef Val, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildFreeze(Builder, Val, Name);
}

LLVMValueRef TangoLLVMBuildIsNull(LLVMBuilderRef Builder, LLVMValueRef Val, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildIsNull(Builder, Val, Name);
}

LLVMValueRef TangoLLVMBuildIsNotNull(LLVMBuilderRef Builder, LLVMValueRef Val, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildIsNotNull(Builder, Val, Name);
}

LLVMValueRef TangoLLVMBuildPtrDiff2(LLVMBuilderRef Builder, LLVMTypeRef ElemTy, LLVMValueRef LHS, LLVMValueRef RHS, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildPtrDiff2(Builder, ElemTy, LHS, RHS, Name);
}

LLVMValueRef TangoLLVMBuildFence(LLVMBuilderRef B, LLVMAtomicOrdering ordering, LLVMBool singleThread, const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMBuildFence(B, ordering, singleThread, Name);
}

LLVMValueRef TangoLLVMBuildAtomicRMW(LLVMBuilderRef B, LLVMAtomicRMWBinOp op, LLVMValueRef PTR, LLVMValueRef Val, LLVMAtomicOrdering ordering, LLVMBool singleThread) {
   return _TheTangoLLVMEnv->TangoLLVMBuildAtomicRMW(B, op, PTR, Val, ordering, singleThread);
}

LLVMValueRef TangoLLVMBuildAtomicCmpXchg(LLVMBuilderRef B, LLVMValueRef Ptr, LLVMValueRef Cmp, LLVMValueRef New, LLVMAtomicOrdering SuccessOrdering, LLVMAtomicOrdering FailureOrdering, LLVMBool SingleThread) {
   return _TheTangoLLVMEnv->TangoLLVMBuildAtomicCmpXchg(B, Ptr, Cmp, New, SuccessOrdering, FailureOrdering, SingleThread);
}

unsigned TangoLLVMGetNumMaskElements(LLVMValueRef ShuffleVectorInst) {
   return _TheTangoLLVMEnv->TangoLLVMGetNumMaskElements(ShuffleVectorInst);
}

int TangoLLVMGetUndefMaskElem(void) {
   return _TheTangoLLVMEnv->TangoLLVMGetUndefMaskElem();
}

int TangoLLVMGetMaskValue(LLVMValueRef ShuffleVectorInst, unsigned Elt) {
   return _TheTangoLLVMEnv->TangoLLVMGetMaskValue(ShuffleVectorInst, Elt);
}

LLVMBool TangoLLVMIsAtomicSingleThread(LLVMValueRef AtomicInst) {
   return _TheTangoLLVMEnv->TangoLLVMIsAtomicSingleThread(AtomicInst);
}

void TangoLLVMSetAtomicSingleThread(LLVMValueRef AtomicInst, LLVMBool SingleThread) {
  _TheTangoLLVMEnv->TangoLLVMSetAtomicSingleThread(AtomicInst, SingleThread);
}

LLVMAtomicOrdering TangoLLVMGetCmpXchgSuccessOrdering(LLVMValueRef CmpXchgInst) {
   return _TheTangoLLVMEnv->TangoLLVMGetCmpXchgSuccessOrdering(CmpXchgInst);
}

void TangoLLVMSetCmpXchgSuccessOrdering(LLVMValueRef CmpXchgInst, LLVMAtomicOrdering Ordering) {
  _TheTangoLLVMEnv->TangoLLVMSetCmpXchgSuccessOrdering(CmpXchgInst, Ordering);
}

LLVMAtomicOrdering TangoLLVMGetCmpXchgFailureOrdering(LLVMValueRef CmpXchgInst) {
   return _TheTangoLLVMEnv->TangoLLVMGetCmpXchgFailureOrdering(CmpXchgInst);
}

void TangoLLVMSetCmpXchgFailureOrdering(LLVMValueRef CmpXchgInst, LLVMAtomicOrdering Ordering) {
  _TheTangoLLVMEnv->TangoLLVMSetCmpXchgFailureOrdering(CmpXchgInst, Ordering);
}

LLVMModuleProviderRef TangoLLVMCreateModuleProviderForExistingModule(LLVMModuleRef M) {
   return _TheTangoLLVMEnv->TangoLLVMCreateModuleProviderForExistingModule(M);
}

void TangoLLVMDisposeModuleProvider(LLVMModuleProviderRef M) {
  _TheTangoLLVMEnv->TangoLLVMDisposeModuleProvider(M);
}

LLVMBool TangoLLVMCreateMemoryBufferWithContentsOfFile(const char *Path, LLVMMemoryBufferRef *OutMemBuf, char **OutMessage) {
   return _TheTangoLLVMEnv->TangoLLVMCreateMemoryBufferWithContentsOfFile(Path, OutMemBuf, OutMessage);
}

LLVMBool TangoLLVMCreateMemoryBufferWithSTDIN(LLVMMemoryBufferRef *OutMemBuf, char **OutMessage) {
   return _TheTangoLLVMEnv->TangoLLVMCreateMemoryBufferWithSTDIN(OutMemBuf, OutMessage);
}

LLVMMemoryBufferRef TangoLLVMCreateMemoryBufferWithMemoryRange(const char *InputData, size_t InputDataLength, const char *BufferName, LLVMBool RequiresNullTerminator) {
   return _TheTangoLLVMEnv->TangoLLVMCreateMemoryBufferWithMemoryRange(InputData, InputDataLength, BufferName, RequiresNullTerminator);
}

LLVMMemoryBufferRef TangoLLVMCreateMemoryBufferWithMemoryRangeCopy(const char *InputData, size_t InputDataLength, const char *BufferName) {
   return _TheTangoLLVMEnv->TangoLLVMCreateMemoryBufferWithMemoryRangeCopy(InputData, InputDataLength, BufferName);
}

const char *TangoLLVMGetBufferStart(LLVMMemoryBufferRef MemBuf) {
   return _TheTangoLLVMEnv->TangoLLVMGetBufferStart(MemBuf);
}

size_t TangoLLVMGetBufferSize(LLVMMemoryBufferRef MemBuf) {
   return _TheTangoLLVMEnv->TangoLLVMGetBufferSize(MemBuf);
}

void TangoLLVMDisposeMemoryBuffer(LLVMMemoryBufferRef MemBuf) {
  _TheTangoLLVMEnv->TangoLLVMDisposeMemoryBuffer(MemBuf);
}

LLVMPassManagerRef TangoLLVMCreatePassManager(void) {
   return _TheTangoLLVMEnv->TangoLLVMCreatePassManager();
}

LLVMPassManagerRef TangoLLVMCreateFunctionPassManagerForModule(LLVMModuleRef M) {
   return _TheTangoLLVMEnv->TangoLLVMCreateFunctionPassManagerForModule(M);
}

LLVMPassManagerRef TangoLLVMCreateFunctionPassManager(LLVMModuleProviderRef MP) {
   return _TheTangoLLVMEnv->TangoLLVMCreateFunctionPassManager(MP);
}

LLVMBool TangoLLVMRunPassManager(LLVMPassManagerRef PM, LLVMModuleRef M) {
   return _TheTangoLLVMEnv->TangoLLVMRunPassManager(PM, M);
}

LLVMBool TangoLLVMInitializeFunctionPassManager(LLVMPassManagerRef FPM) {
   return _TheTangoLLVMEnv->TangoLLVMInitializeFunctionPassManager(FPM);
}

LLVMBool TangoLLVMRunFunctionPassManager(LLVMPassManagerRef FPM, LLVMValueRef F) {
   return _TheTangoLLVMEnv->TangoLLVMRunFunctionPassManager(FPM, F);
}

LLVMBool TangoLLVMFinalizeFunctionPassManager(LLVMPassManagerRef FPM) {
   return _TheTangoLLVMEnv->TangoLLVMFinalizeFunctionPassManager(FPM);
}

void TangoLLVMDisposePassManager(LLVMPassManagerRef PM) {
  _TheTangoLLVMEnv->TangoLLVMDisposePassManager(PM);
}

LLVMBool TangoLLVMStartMultithreaded(void) {
   return _TheTangoLLVMEnv->TangoLLVMStartMultithreaded();
}

void TangoLLVMStopMultithreaded(void) {
  _TheTangoLLVMEnv->TangoLLVMStopMultithreaded();
}

LLVMBool TangoLLVMIsMultithreaded(void) {
   return _TheTangoLLVMEnv->TangoLLVMIsMultithreaded();
}

int TangoLLVMWriteBitcodeToFile(LLVMModuleRef M, const char *Path) {
   return _TheTangoLLVMEnv->TangoLLVMWriteBitcodeToFile(M, Path);
}

int TangoLLVMWriteBitcodeToFD(LLVMModuleRef M, int FD, int ShouldClose, int Unbuffered) {
   return _TheTangoLLVMEnv->TangoLLVMWriteBitcodeToFD(M, FD, ShouldClose, Unbuffered);
}

int TangoLLVMWriteBitcodeToFileHandle(LLVMModuleRef M, int Handle) {
   return _TheTangoLLVMEnv->TangoLLVMWriteBitcodeToFileHandle(M, Handle);
}

LLVMMemoryBufferRef TangoLLVMWriteBitcodeToMemoryBuffer(LLVMModuleRef M) {
   return _TheTangoLLVMEnv->TangoLLVMWriteBitcodeToMemoryBuffer(M);
}

LLVMBool TangoLLVMParseBitcode(LLVMMemoryBufferRef MemBuf, LLVMModuleRef *OutModule, char **OutMessage) {
   return _TheTangoLLVMEnv->TangoLLVMParseBitcode(MemBuf, OutModule, OutMessage);
}

LLVMBool TangoLLVMParseBitcode2(LLVMMemoryBufferRef MemBuf, LLVMModuleRef *OutModule) {
   return _TheTangoLLVMEnv->TangoLLVMParseBitcode2(MemBuf, OutModule);
}

LLVMBool TangoLLVMParseBitcodeInContext(LLVMContextRef ContextRef, LLVMMemoryBufferRef MemBuf, LLVMModuleRef *OutModule, char **OutMessage) {
   return _TheTangoLLVMEnv->TangoLLVMParseBitcodeInContext(ContextRef, MemBuf, OutModule, OutMessage);
}

LLVMBool TangoLLVMParseBitcodeInContext2(LLVMContextRef ContextRef, LLVMMemoryBufferRef MemBuf, LLVMModuleRef *OutModule) {
   return _TheTangoLLVMEnv->TangoLLVMParseBitcodeInContext2(ContextRef, MemBuf, OutModule);
}

LLVMBool TangoLLVMGetBitcodeModuleInContext(LLVMContextRef ContextRef, LLVMMemoryBufferRef MemBuf, LLVMModuleRef *OutM, char **OutMessage) {
   return _TheTangoLLVMEnv->TangoLLVMGetBitcodeModuleInContext(ContextRef, MemBuf, OutM, OutMessage);
}

LLVMBool TangoLLVMGetBitcodeModuleInContext2(LLVMContextRef ContextRef, LLVMMemoryBufferRef MemBuf, LLVMModuleRef *OutM) {
   return _TheTangoLLVMEnv->TangoLLVMGetBitcodeModuleInContext2(ContextRef, MemBuf, OutM);
}

LLVMBool TangoLLVMGetBitcodeModule(LLVMMemoryBufferRef MemBuf, LLVMModuleRef *OutM, char **OutMessage) {
   return _TheTangoLLVMEnv->TangoLLVMGetBitcodeModule(MemBuf, OutM, OutMessage);
}

LLVMBool TangoLLVMGetBitcodeModule2(LLVMMemoryBufferRef MemBuf, LLVMModuleRef *OutM) {
   return _TheTangoLLVMEnv->TangoLLVMGetBitcodeModule2(MemBuf, OutM);
}

LLVMBool TangoLLVMVerifyModule(LLVMModuleRef M, LLVMVerifierFailureAction Action, char **OutMessage) {
   return _TheTangoLLVMEnv->TangoLLVMVerifyModule(M, Action, OutMessage);
}

LLVMBool TangoLLVMVerifyFunction(LLVMValueRef Fn, LLVMVerifierFailureAction Action) {
   return _TheTangoLLVMEnv->TangoLLVMVerifyFunction(Fn, Action);
}

void TangoLLVMViewFunctionCFG(LLVMValueRef Fn) {
  _TheTangoLLVMEnv->TangoLLVMViewFunctionCFG(Fn);
}

void TangoLLVMViewFunctionCFGOnly(LLVMValueRef Fn) {
  _TheTangoLLVMEnv->TangoLLVMViewFunctionCFGOnly(Fn);
}

unsigned TangoLLVMDebugMetadataVersion(void) {
   return _TheTangoLLVMEnv->TangoLLVMDebugMetadataVersion();
}

unsigned TangoLLVMGetModuleDebugMetadataVersion(LLVMModuleRef Module) {
   return _TheTangoLLVMEnv->TangoLLVMGetModuleDebugMetadataVersion(Module);
}

LLVMBool TangoLLVMStripModuleDebugInfo(LLVMModuleRef Module) {
   return _TheTangoLLVMEnv->TangoLLVMStripModuleDebugInfo(Module);
}

LLVMDIBuilderRef TangoLLVMCreateDIBuilderDisallowUnresolved(LLVMModuleRef M) {
   return _TheTangoLLVMEnv->TangoLLVMCreateDIBuilderDisallowUnresolved(M);
}

LLVMDIBuilderRef TangoLLVMCreateDIBuilder(LLVMModuleRef M) {
   return _TheTangoLLVMEnv->TangoLLVMCreateDIBuilder(M);
}

void TangoLLVMDisposeDIBuilder(LLVMDIBuilderRef Builder) {
  _TheTangoLLVMEnv->TangoLLVMDisposeDIBuilder(Builder);
}

void TangoLLVMDIBuilderFinalize(LLVMDIBuilderRef Builder) {
  _TheTangoLLVMEnv->TangoLLVMDIBuilderFinalize(Builder);
}

void TangoLLVMDIBuilderFinalizeSubprogram(LLVMDIBuilderRef Builder, LLVMMetadataRef Subprogram) {
  _TheTangoLLVMEnv->TangoLLVMDIBuilderFinalizeSubprogram(Builder, Subprogram);
}

LLVMMetadataRef TangoLLVMDIBuilderCreateCompileUnit(LLVMDIBuilderRef Builder, LLVMDWARFSourceLanguage Lang, LLVMMetadataRef FileRef, const char *Producer, size_t ProducerLen, LLVMBool isOptimized, const char *Flags, size_t FlagsLen, unsigned RuntimeVer, const char *SplitName, size_t SplitNameLen, LLVMDWARFEmissionKind Kind, unsigned DWOId, LLVMBool SplitDebugInlining, LLVMBool DebugInfoForProfiling, const char *SysRoot, size_t SysRootLen, const char *SDK, size_t SDKLen) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderCreateCompileUnit(Builder, Lang, FileRef, Producer, ProducerLen, isOptimized, Flags, FlagsLen, RuntimeVer, SplitName, SplitNameLen, Kind, DWOId, SplitDebugInlining, DebugInfoForProfiling, SysRoot, SysRootLen, SDK, SDKLen);
}

LLVMMetadataRef TangoLLVMDIBuilderCreateFile(LLVMDIBuilderRef Builder, const char *Filename, size_t FilenameLen, const char *Directory, size_t DirectoryLen) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderCreateFile(Builder, Filename, FilenameLen, Directory, DirectoryLen);
}

LLVMMetadataRef TangoLLVMDIBuilderCreateModule(LLVMDIBuilderRef Builder, LLVMMetadataRef ParentScope, const char *Name, size_t NameLen, const char *ConfigMacros, size_t ConfigMacrosLen, const char *IncludePath, size_t IncludePathLen, const char *APINotesFile, size_t APINotesFileLen) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderCreateModule(Builder, ParentScope, Name, NameLen, ConfigMacros, ConfigMacrosLen, IncludePath, IncludePathLen, APINotesFile, APINotesFileLen);
}

LLVMMetadataRef TangoLLVMDIBuilderCreateNameSpace(LLVMDIBuilderRef Builder, LLVMMetadataRef ParentScope, const char *Name, size_t NameLen, LLVMBool ExportSymbols) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderCreateNameSpace(Builder, ParentScope, Name, NameLen, ExportSymbols);
}

LLVMMetadataRef TangoLLVMDIBuilderCreateFunction(LLVMDIBuilderRef Builder, LLVMMetadataRef Scope, const char *Name, size_t NameLen, const char *LinkageName, size_t LinkageNameLen, LLVMMetadataRef File, unsigned LineNo, LLVMMetadataRef Ty, LLVMBool IsLocalToUnit, LLVMBool IsDefinition, unsigned ScopeLine, LLVMDIFlags Flags, LLVMBool IsOptimized) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderCreateFunction(Builder, Scope, Name, NameLen, LinkageName, LinkageNameLen, File, LineNo, Ty, IsLocalToUnit, IsDefinition, ScopeLine, Flags, IsOptimized);
}

LLVMMetadataRef TangoLLVMDIBuilderCreateLexicalBlock(LLVMDIBuilderRef Builder, LLVMMetadataRef Scope, LLVMMetadataRef File, unsigned Line, unsigned Column) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderCreateLexicalBlock(Builder, Scope, File, Line, Column);
}

LLVMMetadataRef TangoLLVMDIBuilderCreateLexicalBlockFile(LLVMDIBuilderRef Builder, LLVMMetadataRef Scope, LLVMMetadataRef File, unsigned Discriminator) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderCreateLexicalBlockFile(Builder, Scope, File, Discriminator);
}

LLVMMetadataRef TangoLLVMDIBuilderCreateImportedModuleFromNamespace(LLVMDIBuilderRef Builder, LLVMMetadataRef Scope, LLVMMetadataRef NS, LLVMMetadataRef File, unsigned Line) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderCreateImportedModuleFromNamespace(Builder, Scope, NS, File, Line);
}

LLVMMetadataRef TangoLLVMDIBuilderCreateImportedModuleFromAlias(LLVMDIBuilderRef Builder, LLVMMetadataRef Scope, LLVMMetadataRef ImportedEntity, LLVMMetadataRef File, unsigned Line, LLVMMetadataRef *Elements, unsigned NumElements) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderCreateImportedModuleFromAlias(Builder, Scope, ImportedEntity, File, Line, Elements, NumElements);
}

LLVMMetadataRef TangoLLVMDIBuilderCreateImportedModuleFromModule(LLVMDIBuilderRef Builder, LLVMMetadataRef Scope, LLVMMetadataRef M, LLVMMetadataRef File, unsigned Line, LLVMMetadataRef *Elements, unsigned NumElements) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderCreateImportedModuleFromModule(Builder, Scope, M, File, Line, Elements, NumElements);
}

LLVMMetadataRef TangoLLVMDIBuilderCreateImportedDeclaration(LLVMDIBuilderRef Builder, LLVMMetadataRef Scope, LLVMMetadataRef Decl, LLVMMetadataRef File, unsigned Line, const char *Name, size_t NameLen, LLVMMetadataRef *Elements, unsigned NumElements) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderCreateImportedDeclaration(Builder, Scope, Decl, File, Line, Name, NameLen, Elements, NumElements);
}

LLVMMetadataRef TangoLLVMDIBuilderCreateDebugLocation(LLVMContextRef Ctx, unsigned Line, unsigned Column, LLVMMetadataRef Scope, LLVMMetadataRef InlinedAt) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderCreateDebugLocation(Ctx, Line, Column, Scope, InlinedAt);
}

unsigned TangoLLVMDILocationGetLine(LLVMMetadataRef Location) {
   return _TheTangoLLVMEnv->TangoLLVMDILocationGetLine(Location);
}

unsigned TangoLLVMDILocationGetColumn(LLVMMetadataRef Location) {
   return _TheTangoLLVMEnv->TangoLLVMDILocationGetColumn(Location);
}

LLVMMetadataRef TangoLLVMDILocationGetScope(LLVMMetadataRef Location) {
   return _TheTangoLLVMEnv->TangoLLVMDILocationGetScope(Location);
}

LLVMMetadataRef TangoLLVMDILocationGetInlinedAt(LLVMMetadataRef Location) {
   return _TheTangoLLVMEnv->TangoLLVMDILocationGetInlinedAt(Location);
}

LLVMMetadataRef TangoLLVMDIScopeGetFile(LLVMMetadataRef Scope) {
   return _TheTangoLLVMEnv->TangoLLVMDIScopeGetFile(Scope);
}

const char *TangoLLVMDIFileGetDirectory(LLVMMetadataRef File, unsigned *Len) {
   return _TheTangoLLVMEnv->TangoLLVMDIFileGetDirectory(File, Len);
}

const char *TangoLLVMDIFileGetFilename(LLVMMetadataRef File, unsigned *Len) {
   return _TheTangoLLVMEnv->TangoLLVMDIFileGetFilename(File, Len);
}

const char *TangoLLVMDIFileGetSource(LLVMMetadataRef File, unsigned *Len) {
   return _TheTangoLLVMEnv->TangoLLVMDIFileGetSource(File, Len);
}

LLVMMetadataRef TangoLLVMDIBuilderGetOrCreateTypeArray(LLVMDIBuilderRef Builder, LLVMMetadataRef *Data, size_t NumElements) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderGetOrCreateTypeArray(Builder, Data, NumElements);
}

LLVMMetadataRef TangoLLVMDIBuilderCreateSubroutineType(LLVMDIBuilderRef Builder, LLVMMetadataRef File, LLVMMetadataRef *ParameterTypes, unsigned NumParameterTypes, LLVMDIFlags Flags) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderCreateSubroutineType(Builder, File, ParameterTypes, NumParameterTypes, Flags);
}

LLVMMetadataRef TangoLLVMDIBuilderCreateMacro(LLVMDIBuilderRef Builder, LLVMMetadataRef ParentMacroFile, unsigned Line, LLVMDWARFMacinfoRecordType RecordType, const char *Name, size_t NameLen, const char *Value, size_t ValueLen) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderCreateMacro(Builder, ParentMacroFile, Line, RecordType, Name, NameLen, Value, ValueLen);
}

LLVMMetadataRef TangoLLVMDIBuilderCreateTempMacroFile(LLVMDIBuilderRef Builder, LLVMMetadataRef ParentMacroFile, unsigned Line, LLVMMetadataRef File) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderCreateTempMacroFile(Builder, ParentMacroFile, Line, File);
}

LLVMMetadataRef TangoLLVMDIBuilderCreateEnumerator(LLVMDIBuilderRef Builder, const char *Name, size_t NameLen, int64_t Value, LLVMBool IsUnsigned) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderCreateEnumerator(Builder, Name, NameLen, Value, IsUnsigned);
}

LLVMMetadataRef TangoLLVMDIBuilderCreateEnumerationType(LLVMDIBuilderRef Builder, LLVMMetadataRef Scope, const char *Name, size_t NameLen, LLVMMetadataRef File, unsigned LineNumber, uint64_t SizeInBits, uint32_t AlignInBits, LLVMMetadataRef *Elements, unsigned NumElements, LLVMMetadataRef ClassTy) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderCreateEnumerationType(Builder, Scope, Name, NameLen, File, LineNumber, SizeInBits, AlignInBits, Elements, NumElements, ClassTy);
}

LLVMMetadataRef TangoLLVMDIBuilderCreateUnionType(LLVMDIBuilderRef Builder, LLVMMetadataRef Scope, const char *Name, size_t NameLen, LLVMMetadataRef File, unsigned LineNumber, uint64_t SizeInBits, uint32_t AlignInBits, LLVMDIFlags Flags, LLVMMetadataRef *Elements, unsigned NumElements, unsigned RunTimeLang, const char *UniqueId, size_t UniqueIdLen) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderCreateUnionType(Builder, Scope, Name, NameLen, File, LineNumber, SizeInBits, AlignInBits, Flags, Elements, NumElements, RunTimeLang, UniqueId, UniqueIdLen);
}

LLVMMetadataRef TangoLLVMDIBuilderCreateArrayType(LLVMDIBuilderRef Builder, uint64_t Size, uint32_t AlignInBits, LLVMMetadataRef Ty, LLVMMetadataRef *Subscripts, unsigned NumSubscripts) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderCreateArrayType(Builder, Size, AlignInBits, Ty, Subscripts, NumSubscripts);
}

LLVMMetadataRef TangoLLVMDIBuilderCreateVectorType(LLVMDIBuilderRef Builder, uint64_t Size, uint32_t AlignInBits, LLVMMetadataRef Ty, LLVMMetadataRef *Subscripts, unsigned NumSubscripts) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderCreateVectorType(Builder, Size, AlignInBits, Ty, Subscripts, NumSubscripts);
}

LLVMMetadataRef TangoLLVMDIBuilderCreateUnspecifiedType(LLVMDIBuilderRef Builder, const char *Name, size_t NameLen) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderCreateUnspecifiedType(Builder, Name, NameLen);
}

LLVMMetadataRef TangoLLVMDIBuilderCreateBasicType(LLVMDIBuilderRef Builder, const char *Name, size_t NameLen, uint64_t SizeInBits, LLVMDWARFTypeEncoding Encoding, LLVMDIFlags Flags) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderCreateBasicType(Builder, Name, NameLen, SizeInBits, Encoding, Flags);
}

LLVMMetadataRef TangoLLVMDIBuilderCreatePointerType(LLVMDIBuilderRef Builder, LLVMMetadataRef PointeeTy, uint64_t SizeInBits, uint32_t AlignInBits, unsigned AddressSpace, const char *Name, size_t NameLen) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderCreatePointerType(Builder, PointeeTy, SizeInBits, AlignInBits, AddressSpace, Name, NameLen);
}

LLVMMetadataRef TangoLLVMDIBuilderCreateStructType(LLVMDIBuilderRef Builder, LLVMMetadataRef Scope, const char *Name, size_t NameLen, LLVMMetadataRef File, unsigned LineNumber, uint64_t SizeInBits, uint32_t AlignInBits, LLVMDIFlags Flags, LLVMMetadataRef DerivedFrom, LLVMMetadataRef *Elements, unsigned NumElements, unsigned RunTimeLang, LLVMMetadataRef VTableHolder, const char *UniqueId, size_t UniqueIdLen) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderCreateStructType(Builder, Scope, Name, NameLen, File, LineNumber, SizeInBits, AlignInBits, Flags, DerivedFrom, Elements, NumElements, RunTimeLang, VTableHolder, UniqueId, UniqueIdLen);
}

LLVMMetadataRef TangoLLVMDIBuilderCreateMemberType(LLVMDIBuilderRef Builder, LLVMMetadataRef Scope, const char *Name, size_t NameLen, LLVMMetadataRef File, unsigned LineNo, uint64_t SizeInBits, uint32_t AlignInBits, uint64_t OffsetInBits, LLVMDIFlags Flags, LLVMMetadataRef Ty) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderCreateMemberType(Builder, Scope, Name, NameLen, File, LineNo, SizeInBits, AlignInBits, OffsetInBits, Flags, Ty);
}

LLVMMetadataRef TangoLLVMDIBuilderCreateStaticMemberType(  LLVMDIBuilderRef Builder, LLVMMetadataRef Scope, const char *Name, size_t NameLen, LLVMMetadataRef File, unsigned LineNumber, LLVMMetadataRef Type, LLVMDIFlags Flags, LLVMValueRef ConstantVal, uint32_t AlignInBits) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderCreateStaticMemberType(Builder, Scope, Name, NameLen, File, LineNumber, Type, Flags, ConstantVal, AlignInBits);
}

LLVMMetadataRef TangoLLVMDIBuilderCreateMemberPointerType(LLVMDIBuilderRef Builder, LLVMMetadataRef PointeeType, LLVMMetadataRef ClassType, uint64_t SizeInBits, uint32_t AlignInBits, LLVMDIFlags Flags) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderCreateMemberPointerType(Builder, PointeeType, ClassType, SizeInBits, AlignInBits, Flags);
}

LLVMMetadataRef TangoLLVMDIBuilderCreateObjCIVar(LLVMDIBuilderRef Builder, const char *Name, size_t NameLen, LLVMMetadataRef File, unsigned LineNo, uint64_t SizeInBits, uint32_t AlignInBits, uint64_t OffsetInBits, LLVMDIFlags Flags, LLVMMetadataRef Ty, LLVMMetadataRef PropertyNode) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderCreateObjCIVar(Builder, Name, NameLen, File, LineNo, SizeInBits, AlignInBits, OffsetInBits, Flags, Ty, PropertyNode);
}

LLVMMetadataRef TangoLLVMDIBuilderCreateObjCProperty(LLVMDIBuilderRef Builder, const char *Name, size_t NameLen, LLVMMetadataRef File, unsigned LineNo, const char *GetterName, size_t GetterNameLen, const char *SetterName, size_t SetterNameLen, unsigned PropertyAttributes, LLVMMetadataRef Ty) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderCreateObjCProperty(Builder, Name, NameLen, File, LineNo, GetterName, GetterNameLen, SetterName, SetterNameLen, PropertyAttributes, Ty);
}

LLVMMetadataRef TangoLLVMDIBuilderCreateObjectPointerType(LLVMDIBuilderRef Builder, LLVMMetadataRef Type) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderCreateObjectPointerType(Builder, Type);
}

LLVMMetadataRef TangoLLVMDIBuilderCreateQualifiedType(LLVMDIBuilderRef Builder, unsigned Tag, LLVMMetadataRef Type) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderCreateQualifiedType(Builder, Tag, Type);
}

LLVMMetadataRef TangoLLVMDIBuilderCreateReferenceType(LLVMDIBuilderRef Builder, unsigned Tag, LLVMMetadataRef Type) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderCreateReferenceType(Builder, Tag, Type);
}

LLVMMetadataRef TangoLLVMDIBuilderCreateNullPtrType(LLVMDIBuilderRef Builder) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderCreateNullPtrType(Builder);
}

LLVMMetadataRef TangoLLVMDIBuilderCreateTypedef(LLVMDIBuilderRef Builder, LLVMMetadataRef Type, const char *Name, size_t NameLen, LLVMMetadataRef File, unsigned LineNo, LLVMMetadataRef Scope, uint32_t AlignInBits) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderCreateTypedef(Builder, Type, Name, NameLen, File, LineNo, Scope, AlignInBits);
}

LLVMMetadataRef TangoLLVMDIBuilderCreateInheritance(LLVMDIBuilderRef Builder, LLVMMetadataRef Ty, LLVMMetadataRef BaseTy, uint64_t BaseOffset, uint32_t VBPtrOffset, LLVMDIFlags Flags) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderCreateInheritance(Builder, Ty, BaseTy, BaseOffset, VBPtrOffset, Flags);
}

LLVMMetadataRef TangoLLVMDIBuilderCreateForwardDecl(LLVMDIBuilderRef Builder, unsigned Tag, const char *Name, size_t NameLen, LLVMMetadataRef Scope, LLVMMetadataRef File, unsigned Line, unsigned RuntimeLang, uint64_t SizeInBits, uint32_t AlignInBits, const char *UniqueIdentifier, size_t UniqueIdentifierLen) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderCreateForwardDecl(Builder, Tag, Name, NameLen, Scope, File, Line, RuntimeLang, SizeInBits, AlignInBits, UniqueIdentifier, UniqueIdentifierLen);
}

LLVMMetadataRef TangoLLVMDIBuilderCreateReplaceableCompositeType(  LLVMDIBuilderRef Builder, unsigned Tag, const char *Name, size_t NameLen, LLVMMetadataRef Scope, LLVMMetadataRef File, unsigned Line, unsigned RuntimeLang, uint64_t SizeInBits, uint32_t AlignInBits, LLVMDIFlags Flags, const char *UniqueIdentifier, size_t UniqueIdentifierLen) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderCreateReplaceableCompositeType(Builder, Tag, Name, NameLen, Scope, File, Line, RuntimeLang, SizeInBits, AlignInBits, Flags, UniqueIdentifier, UniqueIdentifierLen);
}

LLVMMetadataRef TangoLLVMDIBuilderCreateBitFieldMemberType(LLVMDIBuilderRef Builder, LLVMMetadataRef Scope, const char *Name, size_t NameLen, LLVMMetadataRef File, unsigned LineNumber, uint64_t SizeInBits, uint64_t OffsetInBits, uint64_t StorageOffsetInBits, LLVMDIFlags Flags, LLVMMetadataRef Type) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderCreateBitFieldMemberType(Builder, Scope, Name, NameLen, File, LineNumber, SizeInBits, OffsetInBits, StorageOffsetInBits, Flags, Type);
}

LLVMMetadataRef TangoLLVMDIBuilderCreateClassType(LLVMDIBuilderRef Builder, LLVMMetadataRef Scope, const char *Name, size_t NameLen, LLVMMetadataRef File, unsigned LineNumber, uint64_t SizeInBits, uint32_t AlignInBits, uint64_t OffsetInBits, LLVMDIFlags Flags, LLVMMetadataRef DerivedFrom, LLVMMetadataRef *Elements, unsigned NumElements, LLVMMetadataRef VTableHolder, LLVMMetadataRef TemplateParamsNode, const char *UniqueIdentifier, size_t UniqueIdentifierLen) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderCreateClassType(Builder, Scope, Name, NameLen, File, LineNumber, SizeInBits, AlignInBits, OffsetInBits, Flags, DerivedFrom, Elements, NumElements, VTableHolder, TemplateParamsNode, UniqueIdentifier, UniqueIdentifierLen);
}

LLVMMetadataRef TangoLLVMDIBuilderCreateArtificialType(LLVMDIBuilderRef Builder, LLVMMetadataRef Type) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderCreateArtificialType(Builder, Type);
}

const char *TangoLLVMDITypeGetName(LLVMMetadataRef DType, size_t *Length) {
   return _TheTangoLLVMEnv->TangoLLVMDITypeGetName(DType, Length);
}

uint64_t TangoLLVMDITypeGetSizeInBits(LLVMMetadataRef DType) {
   return _TheTangoLLVMEnv->TangoLLVMDITypeGetSizeInBits(DType);
}

uint64_t TangoLLVMDITypeGetOffsetInBits(LLVMMetadataRef DType) {
   return _TheTangoLLVMEnv->TangoLLVMDITypeGetOffsetInBits(DType);
}

uint32_t TangoLLVMDITypeGetAlignInBits(LLVMMetadataRef DType) {
   return _TheTangoLLVMEnv->TangoLLVMDITypeGetAlignInBits(DType);
}

unsigned TangoLLVMDITypeGetLine(LLVMMetadataRef DType) {
   return _TheTangoLLVMEnv->TangoLLVMDITypeGetLine(DType);
}

LLVMDIFlags TangoLLVMDITypeGetFlags(LLVMMetadataRef DType) {
   return _TheTangoLLVMEnv->TangoLLVMDITypeGetFlags(DType);
}

LLVMMetadataRef TangoLLVMDIBuilderGetOrCreateSubrange(LLVMDIBuilderRef Builder, int64_t LowerBound, int64_t Count) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderGetOrCreateSubrange(Builder, LowerBound, Count);
}

LLVMMetadataRef TangoLLVMDIBuilderGetOrCreateArray(LLVMDIBuilderRef Builder, LLVMMetadataRef *Data, size_t NumElements) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderGetOrCreateArray(Builder, Data, NumElements);
}

LLVMMetadataRef TangoLLVMDIBuilderCreateExpression(LLVMDIBuilderRef Builder, uint64_t *Addr, size_t Length) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderCreateExpression(Builder, Addr, Length);
}

LLVMMetadataRef TangoLLVMDIBuilderCreateConstantValueExpression(LLVMDIBuilderRef Builder, uint64_t Value) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderCreateConstantValueExpression(Builder, Value);
}

LLVMMetadataRef TangoLLVMDIBuilderCreateGlobalVariableExpression(LLVMDIBuilderRef Builder, LLVMMetadataRef Scope, const char *Name, size_t NameLen, const char *Linkage, size_t LinkLen, LLVMMetadataRef File, unsigned LineNo, LLVMMetadataRef Ty, LLVMBool LocalToUnit, LLVMMetadataRef Expr, LLVMMetadataRef Decl, uint32_t AlignInBits) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderCreateGlobalVariableExpression(Builder, Scope, Name, NameLen, Linkage, LinkLen, File, LineNo, Ty, LocalToUnit, Expr, Decl, AlignInBits);
}

uint16_t TangoLLVMGetDINodeTag(LLVMMetadataRef MD) {
   return _TheTangoLLVMEnv->TangoLLVMGetDINodeTag(MD);
}

LLVMMetadataRef TangoLLVMDIGlobalVariableExpressionGetVariable(LLVMMetadataRef GVE) {
   return _TheTangoLLVMEnv->TangoLLVMDIGlobalVariableExpressionGetVariable(GVE);
}

LLVMMetadataRef TangoLLVMDIGlobalVariableExpressionGetExpression(LLVMMetadataRef GVE) {
   return _TheTangoLLVMEnv->TangoLLVMDIGlobalVariableExpressionGetExpression(GVE);
}

LLVMMetadataRef TangoLLVMDIVariableGetFile(LLVMMetadataRef Var) {
   return _TheTangoLLVMEnv->TangoLLVMDIVariableGetFile(Var);
}

LLVMMetadataRef TangoLLVMDIVariableGetScope(LLVMMetadataRef Var) {
   return _TheTangoLLVMEnv->TangoLLVMDIVariableGetScope(Var);
}

unsigned TangoLLVMDIVariableGetLine(LLVMMetadataRef Var) {
   return _TheTangoLLVMEnv->TangoLLVMDIVariableGetLine(Var);
}

LLVMMetadataRef TangoLLVMTemporaryMDNode(LLVMContextRef Ctx, LLVMMetadataRef *Data, size_t NumElements) {
   return _TheTangoLLVMEnv->TangoLLVMTemporaryMDNode(Ctx, Data, NumElements);
}

void TangoLLVMDisposeTemporaryMDNode(LLVMMetadataRef TempNode) {
  _TheTangoLLVMEnv->TangoLLVMDisposeTemporaryMDNode(TempNode);
}

void TangoLLVMMetadataReplaceAllUsesWith(LLVMMetadataRef TempTargetMetadata, LLVMMetadataRef Replacement) {
  _TheTangoLLVMEnv->TangoLLVMMetadataReplaceAllUsesWith(TempTargetMetadata, Replacement);
}

LLVMMetadataRef TangoLLVMDIBuilderCreateTempGlobalVariableFwdDecl(LLVMDIBuilderRef Builder, LLVMMetadataRef Scope, const char *Name, size_t NameLen, const char *Linkage, size_t LnkLen, LLVMMetadataRef File, unsigned LineNo, LLVMMetadataRef Ty, LLVMBool LocalToUnit, LLVMMetadataRef Decl, uint32_t AlignInBits) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderCreateTempGlobalVariableFwdDecl(Builder, Scope, Name, NameLen, Linkage, LnkLen, File, LineNo, Ty, LocalToUnit, Decl, AlignInBits);
}

LLVMDbgRecordRef TangoLLVMDIBuilderInsertDeclareRecordBefore(LLVMDIBuilderRef Builder, LLVMValueRef Storage, LLVMMetadataRef VarInfo, LLVMMetadataRef Expr, LLVMMetadataRef DebugLoc, LLVMValueRef Instr) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderInsertDeclareRecordBefore(Builder, Storage, VarInfo, Expr, DebugLoc, Instr);
}

LLVMDbgRecordRef TangoLLVMDIBuilderInsertDeclareRecordAtEnd(LLVMDIBuilderRef Builder, LLVMValueRef Storage, LLVMMetadataRef VarInfo, LLVMMetadataRef Expr, LLVMMetadataRef DebugLoc, LLVMBasicBlockRef Block) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderInsertDeclareRecordAtEnd(Builder, Storage, VarInfo, Expr, DebugLoc, Block);
}

LLVMDbgRecordRef TangoLLVMDIBuilderInsertDbgValueRecordBefore(LLVMDIBuilderRef Builder, LLVMValueRef Val, LLVMMetadataRef VarInfo, LLVMMetadataRef Expr, LLVMMetadataRef DebugLoc, LLVMValueRef Instr) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderInsertDbgValueRecordBefore(Builder, Val, VarInfo, Expr, DebugLoc, Instr);
}

LLVMDbgRecordRef TangoLLVMDIBuilderInsertDbgValueRecordAtEnd(LLVMDIBuilderRef Builder, LLVMValueRef Val, LLVMMetadataRef VarInfo, LLVMMetadataRef Expr, LLVMMetadataRef DebugLoc, LLVMBasicBlockRef Block) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderInsertDbgValueRecordAtEnd(Builder, Val, VarInfo, Expr, DebugLoc, Block);
}

LLVMMetadataRef TangoLLVMDIBuilderCreateAutoVariable(LLVMDIBuilderRef Builder, LLVMMetadataRef Scope, const char *Name, size_t NameLen, LLVMMetadataRef File, unsigned LineNo, LLVMMetadataRef Ty, LLVMBool AlwaysPreserve, LLVMDIFlags Flags, uint32_t AlignInBits) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderCreateAutoVariable(Builder, Scope, Name, NameLen, File, LineNo, Ty, AlwaysPreserve, Flags, AlignInBits);
}

LLVMMetadataRef TangoLLVMDIBuilderCreateParameterVariable(LLVMDIBuilderRef Builder, LLVMMetadataRef Scope, const char *Name, size_t NameLen, unsigned ArgNo, LLVMMetadataRef File, unsigned LineNo, LLVMMetadataRef Ty, LLVMBool AlwaysPreserve, LLVMDIFlags Flags) {
   return _TheTangoLLVMEnv->TangoLLVMDIBuilderCreateParameterVariable(Builder, Scope, Name, NameLen, ArgNo, File, LineNo, Ty, AlwaysPreserve, Flags);
}

LLVMMetadataRef TangoLLVMGetSubprogram(LLVMValueRef Func) {
   return _TheTangoLLVMEnv->TangoLLVMGetSubprogram(Func);
}

void TangoLLVMSetSubprogram(LLVMValueRef Func, LLVMMetadataRef SP) {
  _TheTangoLLVMEnv->TangoLLVMSetSubprogram(Func, SP);
}

unsigned TangoLLVMDISubprogramGetLine(LLVMMetadataRef Subprogram) {
   return _TheTangoLLVMEnv->TangoLLVMDISubprogramGetLine(Subprogram);
}

LLVMMetadataRef TangoLLVMInstructionGetDebugLoc(LLVMValueRef Inst) {
   return _TheTangoLLVMEnv->TangoLLVMInstructionGetDebugLoc(Inst);
}

void TangoLLVMInstructionSetDebugLoc(LLVMValueRef Inst, LLVMMetadataRef Loc) {
  _TheTangoLLVMEnv->TangoLLVMInstructionSetDebugLoc(Inst, Loc);
}

LLVMMetadataKind TangoLLVMGetMetadataKind(LLVMMetadataRef Metadata) {
   return _TheTangoLLVMEnv->TangoLLVMGetMetadataKind(Metadata);
}

LLVMBool TangoLLVMLoadLibraryPermanently(const char* Filename) {
   return _TheTangoLLVMEnv->TangoLLVMLoadLibraryPermanently(Filename);
}

void TangoLLVMParseCommandLineOptions(int argc, const char *const *argv, const char *Overview) {
  _TheTangoLLVMEnv->TangoLLVMParseCommandLineOptions(argc, argv, Overview);
}

void *TangoLLVMSearchForAddressOfSymbol(const char *symbolName) {
  return _TheTangoLLVMEnv->TangoLLVMSearchForAddressOfSymbol(symbolName);
}

void TangoLLVMAddSymbol(const char *symbolName, void *symbolValue) {
  _TheTangoLLVMEnv->TangoLLVMAddSymbol(symbolName, symbolValue);
}

LLVMTargetDataRef TangoLLVMGetModuleDataLayout(LLVMModuleRef M) {
   return _TheTangoLLVMEnv->TangoLLVMGetModuleDataLayout(M);
}

void TangoLLVMSetModuleDataLayout(LLVMModuleRef M, LLVMTargetDataRef DL) {
  _TheTangoLLVMEnv->TangoLLVMSetModuleDataLayout(M, DL);
}

LLVMTargetDataRef TangoLLVMCreateTargetData(const char *StringRep) {
   return _TheTangoLLVMEnv->TangoLLVMCreateTargetData(StringRep);
}

void TangoLLVMDisposeTargetData(LLVMTargetDataRef TD) {
  _TheTangoLLVMEnv->TangoLLVMDisposeTargetData(TD);
}

void TangoLLVMAddTargetLibraryInfo(LLVMTargetLibraryInfoRef TLI, LLVMPassManagerRef PM) {
  _TheTangoLLVMEnv->TangoLLVMAddTargetLibraryInfo(TLI, PM);
}

char *TangoLLVMCopyStringRepOfTargetData(LLVMTargetDataRef TD) {
   return _TheTangoLLVMEnv->TangoLLVMCopyStringRepOfTargetData(TD);
}

enum LLVMByteOrdering TangoLLVMByteOrder(LLVMTargetDataRef TD) {
   return _TheTangoLLVMEnv->TangoLLVMByteOrder(TD);
}

unsigned TangoLLVMPointerSize(LLVMTargetDataRef TD) {
   return _TheTangoLLVMEnv->TangoLLVMPointerSize(TD);
}

unsigned TangoLLVMPointerSizeForAS(LLVMTargetDataRef TD, unsigned AS) {
   return _TheTangoLLVMEnv->TangoLLVMPointerSizeForAS(TD, AS);
}

LLVMTypeRef TangoLLVMIntPtrType(LLVMTargetDataRef TD) {
   return _TheTangoLLVMEnv->TangoLLVMIntPtrType(TD);
}

LLVMTypeRef TangoLLVMIntPtrTypeForAS(LLVMTargetDataRef TD, unsigned AS) {
   return _TheTangoLLVMEnv->TangoLLVMIntPtrTypeForAS(TD, AS);
}

LLVMTypeRef TangoLLVMIntPtrTypeInContext(LLVMContextRef C, LLVMTargetDataRef TD) {
   return _TheTangoLLVMEnv->TangoLLVMIntPtrTypeInContext(C, TD);
}

LLVMTypeRef TangoLLVMIntPtrTypeForASInContext(LLVMContextRef C, LLVMTargetDataRef TD, unsigned AS) {
   return _TheTangoLLVMEnv->TangoLLVMIntPtrTypeForASInContext(C, TD, AS);
}

unsigned long long TangoLLVMSizeOfTypeInBits(LLVMTargetDataRef TD, LLVMTypeRef Ty) {
   return _TheTangoLLVMEnv->TangoLLVMSizeOfTypeInBits(TD, Ty);
}

unsigned long long TangoLLVMStoreSizeOfType(LLVMTargetDataRef TD, LLVMTypeRef Ty) {
   return _TheTangoLLVMEnv->TangoLLVMStoreSizeOfType(TD, Ty);
}

unsigned long long TangoLLVMABISizeOfType(LLVMTargetDataRef TD, LLVMTypeRef Ty) {
   return _TheTangoLLVMEnv->TangoLLVMABISizeOfType(TD, Ty);
}

unsigned TangoLLVMABIAlignmentOfType(LLVMTargetDataRef TD, LLVMTypeRef Ty) {
   return _TheTangoLLVMEnv->TangoLLVMABIAlignmentOfType(TD, Ty);
}

unsigned TangoLLVMCallFrameAlignmentOfType(LLVMTargetDataRef TD, LLVMTypeRef Ty) {
   return _TheTangoLLVMEnv->TangoLLVMCallFrameAlignmentOfType(TD, Ty);
}

unsigned TangoLLVMPreferredAlignmentOfType(LLVMTargetDataRef TD, LLVMTypeRef Ty) {
   return _TheTangoLLVMEnv->TangoLLVMPreferredAlignmentOfType(TD, Ty);
}

unsigned TangoLLVMPreferredAlignmentOfGlobal(LLVMTargetDataRef TD, LLVMValueRef GlobalVar) {
   return _TheTangoLLVMEnv->TangoLLVMPreferredAlignmentOfGlobal(TD, GlobalVar);
}

unsigned TangoLLVMElementAtOffset(LLVMTargetDataRef TD, LLVMTypeRef StructTy, unsigned long long Offset) {
   return _TheTangoLLVMEnv->TangoLLVMElementAtOffset(TD, StructTy, Offset);
}

unsigned long long TangoLLVMOffsetOfElement(LLVMTargetDataRef TD, LLVMTypeRef StructTy, unsigned Element) {
   return _TheTangoLLVMEnv->TangoLLVMOffsetOfElement(TD, StructTy, Element);
}

LLVMTargetRef TangoLLVMGetFirstTarget(void) {
   return _TheTangoLLVMEnv->TangoLLVMGetFirstTarget();
}

LLVMTargetRef TangoLLVMGetNextTarget(LLVMTargetRef T) {
   return _TheTangoLLVMEnv->TangoLLVMGetNextTarget(T);
}

LLVMTargetRef TangoLLVMGetTargetFromName(const char *Name) {
   return _TheTangoLLVMEnv->TangoLLVMGetTargetFromName(Name);
}

LLVMBool TangoLLVMGetTargetFromTriple(const char* Triple, LLVMTargetRef *T, char **ErrorMessage) {
   return _TheTangoLLVMEnv->TangoLLVMGetTargetFromTriple(Triple, T, ErrorMessage);
}

const char *TangoLLVMGetTargetName(LLVMTargetRef T) {
   return _TheTangoLLVMEnv->TangoLLVMGetTargetName(T);
}

const char *TangoLLVMGetTargetDescription(LLVMTargetRef T) {
   return _TheTangoLLVMEnv->TangoLLVMGetTargetDescription(T);
}

LLVMBool TangoLLVMTargetHasJIT(LLVMTargetRef T) {
   return _TheTangoLLVMEnv->TangoLLVMTargetHasJIT(T);
}

LLVMBool TangoLLVMTargetHasTargetMachine(LLVMTargetRef T) {
   return _TheTangoLLVMEnv->TangoLLVMTargetHasTargetMachine(T);
}

LLVMBool TangoLLVMTargetHasAsmBackend(LLVMTargetRef T) {
   return _TheTangoLLVMEnv->TangoLLVMTargetHasAsmBackend(T);
}

LLVMTargetMachineOptionsRef TangoLLVMCreateTargetMachineOptions(void) {
   return _TheTangoLLVMEnv->TangoLLVMCreateTargetMachineOptions();
}

void TangoLLVMDisposeTargetMachineOptions(LLVMTargetMachineOptionsRef Options) {
  _TheTangoLLVMEnv->TangoLLVMDisposeTargetMachineOptions(Options);
}

void TangoLLVMTargetMachineOptionsSetCPU(LLVMTargetMachineOptionsRef Options, const char *CPU) {
  _TheTangoLLVMEnv->TangoLLVMTargetMachineOptionsSetCPU(Options, CPU);
}

void TangoLLVMTargetMachineOptionsSetFeatures(LLVMTargetMachineOptionsRef Options, const char *Features) {
  _TheTangoLLVMEnv->TangoLLVMTargetMachineOptionsSetFeatures(Options, Features);
}

void TangoLLVMTargetMachineOptionsSetABI(LLVMTargetMachineOptionsRef Options, const char *ABI) {
  _TheTangoLLVMEnv->TangoLLVMTargetMachineOptionsSetABI(Options, ABI);
}

void TangoLLVMTargetMachineOptionsSetCodeGenOptLevel(LLVMTargetMachineOptionsRef Options, LLVMCodeGenOptLevel Level) {
  _TheTangoLLVMEnv->TangoLLVMTargetMachineOptionsSetCodeGenOptLevel(Options, Level);
}

void TangoLLVMTargetMachineOptionsSetRelocMode(LLVMTargetMachineOptionsRef Options, LLVMRelocMode Reloc) {
  _TheTangoLLVMEnv->TangoLLVMTargetMachineOptionsSetRelocMode(Options, Reloc);
}

void TangoLLVMTargetMachineOptionsSetCodeModel(LLVMTargetMachineOptionsRef Options, LLVMCodeModel CodeModel) {
  _TheTangoLLVMEnv->TangoLLVMTargetMachineOptionsSetCodeModel(Options, CodeModel);
}

LLVMTargetMachineRef TangoLLVMCreateTargetMachineWithOptions(LLVMTargetRef T, const char *Triple, LLVMTargetMachineOptionsRef Options) {
   return _TheTangoLLVMEnv->TangoLLVMCreateTargetMachineWithOptions(T, Triple, Options);
}

LLVMTargetMachineRef TangoLLVMCreateTargetMachine(LLVMTargetRef T, const char *Triple, const char *CPU, const char *Features, LLVMCodeGenOptLevel Level, LLVMRelocMode Reloc, LLVMCodeModel CodeModel) {
   return _TheTangoLLVMEnv->TangoLLVMCreateTargetMachine(T, Triple, CPU, Features, Level, Reloc, CodeModel);
}

void TangoLLVMDisposeTargetMachine(LLVMTargetMachineRef T) {
  _TheTangoLLVMEnv->TangoLLVMDisposeTargetMachine(T);
}

LLVMTargetRef TangoLLVMGetTargetMachineTarget(LLVMTargetMachineRef T) {
   return _TheTangoLLVMEnv->TangoLLVMGetTargetMachineTarget(T);
}

char *TangoLLVMGetTargetMachineTriple(LLVMTargetMachineRef T) {
   return _TheTangoLLVMEnv->TangoLLVMGetTargetMachineTriple(T);
}

char *TangoLLVMGetTargetMachineCPU(LLVMTargetMachineRef T) {
   return _TheTangoLLVMEnv->TangoLLVMGetTargetMachineCPU(T);
}

char *TangoLLVMGetTargetMachineFeatureString(LLVMTargetMachineRef T) {
   return _TheTangoLLVMEnv->TangoLLVMGetTargetMachineFeatureString(T);
}

LLVMTargetDataRef TangoLLVMCreateTargetDataLayout(LLVMTargetMachineRef T) {
   return _TheTangoLLVMEnv->TangoLLVMCreateTargetDataLayout(T);
}

void TangoLLVMSetTargetMachineAsmVerbosity(LLVMTargetMachineRef T, LLVMBool VerboseAsm) {
  _TheTangoLLVMEnv->TangoLLVMSetTargetMachineAsmVerbosity(T, VerboseAsm);
}

void TangoLLVMSetTargetMachineFastISel(LLVMTargetMachineRef T, LLVMBool Enable) {
  _TheTangoLLVMEnv->TangoLLVMSetTargetMachineFastISel(T, Enable);
}

void TangoLLVMSetTargetMachineGlobalISel(LLVMTargetMachineRef T, LLVMBool Enable) {
  _TheTangoLLVMEnv->TangoLLVMSetTargetMachineGlobalISel(T, Enable);
}

void TangoLLVMSetTargetMachineGlobalISelAbort(LLVMTargetMachineRef T, LLVMGlobalISelAbortMode Mode) {
  _TheTangoLLVMEnv->TangoLLVMSetTargetMachineGlobalISelAbort(T, Mode);
}

void TangoLLVMSetTargetMachineMachineOutliner(LLVMTargetMachineRef T, LLVMBool Enable) {
  _TheTangoLLVMEnv->TangoLLVMSetTargetMachineMachineOutliner(T, Enable);
}

LLVMBool TangoLLVMTargetMachineEmitToFile(LLVMTargetMachineRef T, LLVMModuleRef M, const char *Filename, LLVMCodeGenFileType codegen, char **ErrorMessage) {
   return _TheTangoLLVMEnv->TangoLLVMTargetMachineEmitToFile(T, M, Filename, codegen, ErrorMessage);
}

LLVMBool TangoLLVMTargetMachineEmitToMemoryBuffer(LLVMTargetMachineRef T, LLVMModuleRef M, LLVMCodeGenFileType codegen, char** ErrorMessage, LLVMMemoryBufferRef *OutMemBuf) {
   return _TheTangoLLVMEnv->TangoLLVMTargetMachineEmitToMemoryBuffer(T, M, codegen, ErrorMessage, OutMemBuf);
}

char* TangoLLVMGetDefaultTargetTriple(void) {
   return _TheTangoLLVMEnv->TangoLLVMGetDefaultTargetTriple();
}

char* TangoLLVMNormalizeTargetTriple(const char* triple) {
   return _TheTangoLLVMEnv->TangoLLVMNormalizeTargetTriple(triple);
}

char* TangoLLVMGetHostCPUName(void) {
   return _TheTangoLLVMEnv->TangoLLVMGetHostCPUName();
}

char* TangoLLVMGetHostCPUFeatures(void) {
   return _TheTangoLLVMEnv->TangoLLVMGetHostCPUFeatures();
}

void TangoLLVMAddAnalysisPasses(LLVMTargetMachineRef T, LLVMPassManagerRef PM) {
  _TheTangoLLVMEnv->TangoLLVMAddAnalysisPasses(T, PM);
}

void TangoLLVMRemoveFunctionBody(LLVMValueRef C) {
   _TheTangoLLVMEnv->TangoLLVMRemoveFunctionBody(C);
}

LLVMValueRef TangoLLVMBuildIntrinsic(LLVMBuilderRef B, unsigned IntrinsicID,
      LLVMTypeRef *Types,
      unsigned NumTypes,
      LLVMValueRef *Args,
      unsigned NumArgs, const char *Name) {
  return _TheTangoLLVMEnv->TangoLLVMBuildIntrinsic(B, IntrinsicID, Types, NumTypes, Args, NumArgs, Name);
}

void TangoLLVMInitializeAll(void) {
  _TheTangoLLVMEnv->TangoLLVMInitializeAll();
}

#define TANGO_LLVM_DEFINE_TANGOLLVM_H(name) \
  LLVMValueRef TangoLLVMIsA##name(LLVMValueRef Val) {\
    return _TheTangoLLVMEnv->TangoLLVMIsA##name(Val); \
  }
LLVM_FOR_EACH_VALUE_SUBCLASS(TANGO_LLVM_DEFINE_TANGOLLVM_H)
