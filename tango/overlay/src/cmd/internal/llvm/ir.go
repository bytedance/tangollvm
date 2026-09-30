//===- ir.go - Bindings for ir --------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 ByteDance Ltd. and/or its affiliates.
// This file has been modified by ByteDance Ltd. and/or its affiliates on 2025-06-18.
//
// Adapted from the LLVM Go bindings maintained at
// https://github.com/tinygo-org/go-llvm.
// See LICENSE.txt in this directory for license information.
// The modified file remains under Apache-2.0 WITH LLVM-exception.
//===----------------------------------------------------------------------===//
//
// This file defines bindings for the ir component.
//
//===----------------------------------------------------------------------===//

//go:build goexperiment.tangollvm && linux && amd64 && cgo && !compiler_bootstrap

package llvm

/*
#include "tangollvm.h"
#include <stdlib.h>
*/
import "C"
import (
	"unsafe"
)

type (
	// We use these weird structs here because *Ref types are pointers and
	// Go's spec says that a pointer cannot be used as a receiver base type.
	Context struct {
		C C.LLVMContextRef
	}
	Module struct {
		C C.LLVMModuleRef
	}
	Type struct {
		C C.LLVMTypeRef
	}
	Value struct {
		C C.LLVMValueRef
	}
	BasicBlock struct {
		C C.LLVMBasicBlockRef
	}
	Builder struct {
		C C.LLVMBuilderRef
	}
	ModuleProvider struct {
		C C.LLVMModuleProviderRef
	}
	MemoryBuffer struct {
		C C.LLVMMemoryBufferRef
	}
	PassManager struct {
		C C.LLVMPassManagerRef
	}
	Use struct {
		C C.LLVMUseRef
	}
	Metadata struct {
		C C.LLVMMetadataRef
	}
	Attribute struct {
		C C.LLVMAttributeRef
	}
	Opcode           C.LLVMOpcode
	AtomicRMWBinOp   C.LLVMAtomicRMWBinOp
	AtomicOrdering   C.LLVMAtomicOrdering
	TypeKind         C.LLVMTypeKind
	Linkage          C.LLVMLinkage
	Visibility       C.LLVMVisibility
	CallConv         C.LLVMCallConv
	IntPredicate     C.LLVMIntPredicate
	FloatPredicate   C.LLVMRealPredicate
	LandingPadClause C.LLVMLandingPadClauseTy
	InlineAsmDialect C.LLVMInlineAsmDialect
	ValueKind        C.LLVMValueKind
)

func (c Context) IsNil() bool        { return c.C == nil }
func (c Module) IsNil() bool         { return c.C == nil }
func (c Type) IsNil() bool           { return c.C == nil }
func (c Value) IsNil() bool          { return c.C == nil }
func (c BasicBlock) IsNil() bool     { return c.C == nil }
func (c Builder) IsNil() bool        { return c.C == nil }
func (c ModuleProvider) IsNil() bool { return c.C == nil }
func (c MemoryBuffer) IsNil() bool   { return c.C == nil }
func (c PassManager) IsNil() bool    { return c.C == nil }
func (c Use) IsNil() bool            { return c.C == nil }
func (c Attribute) IsNil() bool      { return c.C == nil }
func (c Metadata) IsNil() bool       { return c.C == nil }

// helpers
func llvmTypeRefPtr(t *Type) *C.LLVMTypeRef    { return (*C.LLVMTypeRef)(unsafe.Pointer(t)) }
func llvmValueRefPtr(t *Value) *C.LLVMValueRef { return (*C.LLVMValueRef)(unsafe.Pointer(t)) }
func llvmMetadataRefPtr(t *Metadata) *C.LLVMMetadataRef {
	return (*C.LLVMMetadataRef)(unsafe.Pointer(t))
}
func llvmBasicBlockRefPtr(t *BasicBlock) *C.LLVMBasicBlockRef {
	return (*C.LLVMBasicBlockRef)(unsafe.Pointer(t))
}
func boolToLLVMBool(b bool) C.LLVMBool {
	if b {
		return C.LLVMBool(1)
	}
	return C.LLVMBool(0)
}

func llvmValueRefs(values []Value) (*C.LLVMValueRef, C.unsigned) {
	var pt *C.LLVMValueRef
	ptlen := C.unsigned(len(values))
	if ptlen > 0 {
		pt = llvmValueRefPtr(&values[0])
	}
	return pt, ptlen
}

func llvmTypeRefs(types []Type) (*C.LLVMTypeRef, C.unsigned) {
	var pt *C.LLVMTypeRef
	ptlen := C.unsigned(len(types))
	if ptlen > 0 {
		pt = llvmTypeRefPtr(&types[0])
	}
	return pt, ptlen
}

func llvmMetadataRefs(mds []Metadata) (*C.LLVMMetadataRef, C.unsigned) {
	var pt *C.LLVMMetadataRef
	ptlen := C.unsigned(len(mds))
	if ptlen > 0 {
		pt = llvmMetadataRefPtr(&mds[0])
	}
	return pt, ptlen
}

//-------------------------------------------------------------------------
// llvm.Opcode
//-------------------------------------------------------------------------

const (
	Ret         Opcode = C.LLVMRet
	Br          Opcode = C.LLVMBr
	Switch      Opcode = C.LLVMSwitch
	IndirectBr  Opcode = C.LLVMIndirectBr
	Invoke      Opcode = C.LLVMInvoke
	Unreachable Opcode = C.LLVMUnreachable

	// Standard Binary Operators
	Add  Opcode = C.LLVMAdd
	FAdd Opcode = C.LLVMFAdd
	Sub  Opcode = C.LLVMSub
	FSub Opcode = C.LLVMFSub
	Mul  Opcode = C.LLVMMul
	FMul Opcode = C.LLVMFMul
	UDiv Opcode = C.LLVMUDiv
	SDiv Opcode = C.LLVMSDiv
	FDiv Opcode = C.LLVMFDiv
	URem Opcode = C.LLVMURem
	SRem Opcode = C.LLVMSRem
	FRem Opcode = C.LLVMFRem

	// Logical Operators
	Shl  Opcode = C.LLVMShl
	LShr Opcode = C.LLVMLShr
	AShr Opcode = C.LLVMAShr
	And  Opcode = C.LLVMAnd
	Or   Opcode = C.LLVMOr
	Xor  Opcode = C.LLVMXor

	// Memory Operators
	Alloca        Opcode = C.LLVMAlloca
	Load          Opcode = C.LLVMLoad
	Store         Opcode = C.LLVMStore
	GetElementPtr Opcode = C.LLVMGetElementPtr

	// Cast Operators
	Trunc    Opcode = C.LLVMTrunc
	ZExt     Opcode = C.LLVMZExt
	SExt     Opcode = C.LLVMSExt
	FPToUI   Opcode = C.LLVMFPToUI
	FPToSI   Opcode = C.LLVMFPToSI
	UIToFP   Opcode = C.LLVMUIToFP
	SIToFP   Opcode = C.LLVMSIToFP
	FPTrunc  Opcode = C.LLVMFPTrunc
	FPExt    Opcode = C.LLVMFPExt
	PtrToInt Opcode = C.LLVMPtrToInt
	IntToPtr Opcode = C.LLVMIntToPtr
	BitCast  Opcode = C.LLVMBitCast

	// Other Operators
	ICmp   Opcode = C.LLVMICmp
	FCmp   Opcode = C.LLVMFCmp
	PHI    Opcode = C.LLVMPHI
	Call   Opcode = C.LLVMCall
	Select Opcode = C.LLVMSelect
	// UserOp1
	// UserOp2
	VAArg          Opcode = C.LLVMVAArg
	ExtractElement Opcode = C.LLVMExtractElement
	InsertElement  Opcode = C.LLVMInsertElement
	ShuffleVector  Opcode = C.LLVMShuffleVector
	ExtractValue   Opcode = C.LLVMExtractValue
	InsertValue    Opcode = C.LLVMInsertValue
)

const (
	ArgumentValueKind   ValueKind = C.LLVMArgumentValueKind
	BasicBlockValueKind ValueKind = C.LLVMBasicBlockValueKind
	MemoryUseValueKind  ValueKind = C.LLVMMemoryUseValueKind
	MemoryDefValueKind  ValueKind = C.LLVMMemoryDefValueKind
	MemoryPhiValueKind  ValueKind = C.LLVMMemoryPhiValueKind

	FunctionValueKind       ValueKind = C.LLVMFunctionValueKind
	GlobalAliasValueKind    ValueKind = C.LLVMGlobalAliasValueKind
	GlobalIFuncValueKind    ValueKind = C.LLVMGlobalIFuncValueKind
	GlobalVariableValueKind ValueKind = C.LLVMGlobalVariableValueKind
	BlockAddressValueKind   ValueKind = C.LLVMBlockAddressValueKind
	ConstantExprValueKind   ValueKind = C.LLVMConstantExprValueKind
	ConstantArrayValueKind  ValueKind = C.LLVMConstantArrayValueKind
	ConstantStructValueKind ValueKind = C.LLVMConstantStructValueKind
	ConstantVectorValueKind ValueKind = C.LLVMConstantVectorValueKind

	UndefValueValueKind            ValueKind = C.LLVMUndefValueValueKind
	ConstantAggregateZeroValueKind ValueKind = C.LLVMConstantAggregateZeroValueKind
	ConstantDataArrayValueKind     ValueKind = C.LLVMConstantDataArrayValueKind
	ConstantDataVectorValueKind    ValueKind = C.LLVMConstantDataVectorValueKind
	ConstantIntValueKind           ValueKind = C.LLVMConstantIntValueKind
	ConstantFPValueKind            ValueKind = C.LLVMConstantFPValueKind
	ConstantPointerNullValueKind   ValueKind = C.LLVMConstantPointerNullValueKind
	ConstantTokenNoneValueKind     ValueKind = C.LLVMConstantTokenNoneValueKind

	MetadataAsValueValueKind ValueKind = C.LLVMMetadataAsValueValueKind
	InlineAsmValueKind       ValueKind = C.LLVMInlineAsmValueKind

	InstructionValueKind ValueKind = C.LLVMInstructionValueKind
	PoisonValueValueKind ValueKind = C.LLVMPoisonValueValueKind
)

const (
	AtomicRMWBinOpXchg AtomicRMWBinOp = C.LLVMAtomicRMWBinOpXchg
	AtomicRMWBinOpAdd  AtomicRMWBinOp = C.LLVMAtomicRMWBinOpAdd
	AtomicRMWBinOpSub  AtomicRMWBinOp = C.LLVMAtomicRMWBinOpSub
	AtomicRMWBinOpAnd  AtomicRMWBinOp = C.LLVMAtomicRMWBinOpAnd
	AtomicRMWBinOpNand AtomicRMWBinOp = C.LLVMAtomicRMWBinOpNand
	AtomicRMWBinOpOr   AtomicRMWBinOp = C.LLVMAtomicRMWBinOpOr
	AtomicRMWBinOpXor  AtomicRMWBinOp = C.LLVMAtomicRMWBinOpXor
	AtomicRMWBinOpMax  AtomicRMWBinOp = C.LLVMAtomicRMWBinOpMax
	AtomicRMWBinOpMin  AtomicRMWBinOp = C.LLVMAtomicRMWBinOpMin
	AtomicRMWBinOpUMax AtomicRMWBinOp = C.LLVMAtomicRMWBinOpUMax
	AtomicRMWBinOpUMin AtomicRMWBinOp = C.LLVMAtomicRMWBinOpUMin
)

const (
	AtomicOrderingNotAtomic              AtomicOrdering = C.LLVMAtomicOrderingNotAtomic
	AtomicOrderingUnordered              AtomicOrdering = C.LLVMAtomicOrderingUnordered
	AtomicOrderingMonotonic              AtomicOrdering = C.LLVMAtomicOrderingMonotonic
	AtomicOrderingAcquire                AtomicOrdering = C.LLVMAtomicOrderingAcquire
	AtomicOrderingRelease                AtomicOrdering = C.LLVMAtomicOrderingRelease
	AtomicOrderingAcquireRelease         AtomicOrdering = C.LLVMAtomicOrderingAcquireRelease
	AtomicOrderingSequentiallyConsistent AtomicOrdering = C.LLVMAtomicOrderingSequentiallyConsistent
)

//-------------------------------------------------------------------------
// llvm.TypeKind
//-------------------------------------------------------------------------

const (
	VoidTypeKind      TypeKind = C.LLVMVoidTypeKind
	FloatTypeKind     TypeKind = C.LLVMFloatTypeKind
	DoubleTypeKind    TypeKind = C.LLVMDoubleTypeKind
	X86_FP80TypeKind  TypeKind = C.LLVMX86_FP80TypeKind
	FP128TypeKind     TypeKind = C.LLVMFP128TypeKind
	PPC_FP128TypeKind TypeKind = C.LLVMPPC_FP128TypeKind
	LabelTypeKind     TypeKind = C.LLVMLabelTypeKind
	IntegerTypeKind   TypeKind = C.LLVMIntegerTypeKind
	FunctionTypeKind  TypeKind = C.LLVMFunctionTypeKind
	StructTypeKind    TypeKind = C.LLVMStructTypeKind
	ArrayTypeKind     TypeKind = C.LLVMArrayTypeKind
	PointerTypeKind   TypeKind = C.LLVMPointerTypeKind
	VectorTypeKind    TypeKind = C.LLVMVectorTypeKind
	MetadataTypeKind  TypeKind = C.LLVMMetadataTypeKind
	TokenTypeKind     TypeKind = C.LLVMTokenTypeKind
)

//-------------------------------------------------------------------------
// llvm.Linkage
//-------------------------------------------------------------------------

const (
	ExternalLinkage            Linkage = C.LLVMExternalLinkage
	AvailableExternallyLinkage Linkage = C.LLVMAvailableExternallyLinkage
	LinkOnceAnyLinkage         Linkage = C.LLVMLinkOnceAnyLinkage
	LinkOnceODRLinkage         Linkage = C.LLVMLinkOnceODRLinkage
	WeakAnyLinkage             Linkage = C.LLVMWeakAnyLinkage
	WeakODRLinkage             Linkage = C.LLVMWeakODRLinkage
	AppendingLinkage           Linkage = C.LLVMAppendingLinkage
	InternalLinkage            Linkage = C.LLVMInternalLinkage
	PrivateLinkage             Linkage = C.LLVMPrivateLinkage
	ExternalWeakLinkage        Linkage = C.LLVMExternalWeakLinkage
	CommonLinkage              Linkage = C.LLVMCommonLinkage
)

//-------------------------------------------------------------------------
// llvm.Visibility
//-------------------------------------------------------------------------

const (
	DefaultVisibility   Visibility = C.LLVMDefaultVisibility
	HiddenVisibility    Visibility = C.LLVMHiddenVisibility
	ProtectedVisibility Visibility = C.LLVMProtectedVisibility
)

//-------------------------------------------------------------------------
// llvm.CallConv
//-------------------------------------------------------------------------

const (
	CCallConv           CallConv = C.LLVMCCallConv
	FastCallConv        CallConv = C.LLVMFastCallConv
	ColdCallConv        CallConv = C.LLVMColdCallConv
	X86StdcallCallConv  CallConv = C.LLVMX86StdcallCallConv
	X86FastcallCallConv CallConv = C.LLVMX86FastcallCallConv
)

//-------------------------------------------------------------------------
// llvm.IntPredicate
//-------------------------------------------------------------------------

const (
	IntEQ  IntPredicate = C.LLVMIntEQ
	IntNE  IntPredicate = C.LLVMIntNE
	IntUGT IntPredicate = C.LLVMIntUGT
	IntUGE IntPredicate = C.LLVMIntUGE
	IntULT IntPredicate = C.LLVMIntULT
	IntULE IntPredicate = C.LLVMIntULE
	IntSGT IntPredicate = C.LLVMIntSGT
	IntSGE IntPredicate = C.LLVMIntSGE
	IntSLT IntPredicate = C.LLVMIntSLT
	IntSLE IntPredicate = C.LLVMIntSLE
)

//-------------------------------------------------------------------------
// llvm.FloatPredicate
//-------------------------------------------------------------------------

const (
	FloatPredicateFalse FloatPredicate = C.LLVMRealPredicateFalse
	FloatOEQ            FloatPredicate = C.LLVMRealOEQ
	FloatOGT            FloatPredicate = C.LLVMRealOGT
	FloatOGE            FloatPredicate = C.LLVMRealOGE
	FloatOLT            FloatPredicate = C.LLVMRealOLT
	FloatOLE            FloatPredicate = C.LLVMRealOLE
	FloatONE            FloatPredicate = C.LLVMRealONE
	FloatORD            FloatPredicate = C.LLVMRealORD
	FloatUNO            FloatPredicate = C.LLVMRealUNO
	FloatUEQ            FloatPredicate = C.LLVMRealUEQ
	FloatUGT            FloatPredicate = C.LLVMRealUGT
	FloatUGE            FloatPredicate = C.LLVMRealUGE
	FloatULT            FloatPredicate = C.LLVMRealULT
	FloatULE            FloatPredicate = C.LLVMRealULE
	FloatUNE            FloatPredicate = C.LLVMRealUNE
	FloatPredicateTrue  FloatPredicate = C.LLVMRealPredicateTrue
)

//-------------------------------------------------------------------------
// llvm.LandingPadClause
//-------------------------------------------------------------------------

const (
	LandingPadCatch  LandingPadClause = C.LLVMLandingPadCatch
	LandingPadFilter LandingPadClause = C.LLVMLandingPadFilter
)

//-------------------------------------------------------------------------
// llvm.InlineAsmDialect
//-------------------------------------------------------------------------

const (
	InlineAsmDialectATT   InlineAsmDialect = C.LLVMInlineAsmDialectATT
	InlineAsmDialectIntel InlineAsmDialect = C.LLVMInlineAsmDialectIntel
)

//-------------------------------------------------------------------------
// llvm.Context
//-------------------------------------------------------------------------

func NewContext() Context    { return Context{C.TangoLLVMContextCreate()} }
func GlobalContext() Context { return Context{C.TangoLLVMGetGlobalContext()} }
func (c Context) Dispose()   { C.TangoLLVMContextDispose(c.C) }

func (c Context) MDKindID(name string) (id int) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	id = int(C.TangoLLVMGetMDKindIDInContext(c.C, cname, C.unsigned(len(name))))
	return
}

func MDKindID(name string) (id int) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	id = int(C.TangoLLVMGetMDKindID(cname, C.unsigned(len(name))))
	return
}

//-------------------------------------------------------------------------
// llvm.Attribute
//-------------------------------------------------------------------------

func AttributeKindID(name string) (id uint) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	id = uint(C.TangoLLVMGetEnumAttributeKindForName(cname, C.size_t(len(name))))
	return
}

func (c Context) CreateEnumAttribute(kind uint, val uint64) (a Attribute) {
	a.C = C.TangoLLVMCreateEnumAttribute(c.C, C.unsigned(kind), C.uint64_t(val))
	return
}

func (a Attribute) GetEnumKind() (id int) {
	id = int(C.TangoLLVMGetEnumAttributeKind(a.C))
	return
}

func (a Attribute) GetEnumValue() (val uint64) {
	val = uint64(C.TangoLLVMGetEnumAttributeValue(a.C))
	return
}

func (c Context) CreateStringAttribute(kind string, val string) (a Attribute) {
	ckind := C.CString(kind)
	defer C.free(unsafe.Pointer(ckind))
	cval := C.CString(val)
	defer C.free(unsafe.Pointer(cval))
	a.C = C.TangoLLVMCreateStringAttribute(c.C,
		ckind, C.unsigned(len(kind)),
		cval, C.unsigned(len(val)))
	return
}

func (a Attribute) GetStringKind() string {
	length := C.unsigned(0)
	ckind := C.TangoLLVMGetStringAttributeKind(a.C, &length)
	return C.GoStringN(ckind, C.int(length))
}

func (a Attribute) GetStringValue() string {
	length := C.unsigned(0)
	ckind := C.TangoLLVMGetStringAttributeValue(a.C, &length)
	return C.GoStringN(ckind, C.int(length))
}

func (a Attribute) IsEnum() bool {
	return C.TangoLLVMIsEnumAttribute(a.C) != 0
}

func (a Attribute) IsString() bool {
	return C.TangoLLVMIsStringAttribute(a.C) != 0
}

//-------------------------------------------------------------------------
// llvm.Module
//-------------------------------------------------------------------------

// Create and destroy modules.
// See llvm::Module::Module.
func NewModule(name string) (m Module) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	m.C = C.TangoLLVMModuleCreateWithName(cname)
	return
}

func (c Context) NewModule(name string) (m Module) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	m.C = C.TangoLLVMModuleCreateWithNameInContext(cname, c.C)
	return
}

// See llvm::Module::~Module
func (m Module) Dispose() { C.TangoLLVMDisposeModule(m.C) }

// Data layout. See Module::getDataLayout.
func (m Module) DataLayout() string {
	clayout := C.TangoLLVMGetDataLayout(m.C)
	return C.GoString(clayout)
}

func (m Module) SetDataLayout(layout string) {
	clayout := C.CString(layout)
	defer C.free(unsafe.Pointer(clayout))
	C.TangoLLVMSetDataLayout(m.C, clayout)
}

// Target triple. See Module::getTargetTriple.
func (m Module) Target() string {
	ctarget := C.TangoLLVMGetTarget(m.C)
	return C.GoString(ctarget)
}
func (m Module) SetTarget(target string) {
	ctarget := C.CString(target)
	defer C.free(unsafe.Pointer(ctarget))
	C.TangoLLVMSetTarget(m.C, ctarget)
}

func (m Module) GetTypeByName(name string) (t Type) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	t.C = C.TangoLLVMGetTypeByName(m.C, cname)
	return
}

// See Module::dump.
func (m Module) Dump() {
	C.TangoLLVMDumpModule(m.C)
}

func (m Module) String() string {
	cir := C.TangoLLVMPrintModuleToString(m.C)
	defer C.free(unsafe.Pointer(cir))
	ir := C.GoString(cir)
	return ir
}

// See Module::setModuleInlineAsm.
func (m Module) SetInlineAsm(asm string) {
	casm := C.CString(asm)
	defer C.free(unsafe.Pointer(casm))
	C.TangoLLVMSetModuleInlineAsm(m.C, casm)
}

func (m Module) AddNamedMetadataOperand(name string, operand Metadata) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	C.TangoLLVMAddNamedMetadataOperand2(m.C, cname, operand.C)
}

func (m Module) Context() (c Context) {
	c.C = C.TangoLLVMGetModuleContext(m.C)
	return
}

//-------------------------------------------------------------------------
// llvm.Type
//-------------------------------------------------------------------------

// LLVM types conform to the following hierarchy:
//
//   types:
//     integer type
//     real type
//     function type
//     sequence types:
//       array type
//       pointer type
//       vector type
//     void type
//     label type
//     opaque type

// See llvm::LLVMTypeKind::getTypeID.
func (t Type) TypeKind() TypeKind { return TypeKind(C.TangoLLVMGetTypeKind(t.C)) }

// See llvm::LLVMType::getContext.
func (t Type) Context() (c Context) {
	c.C = C.TangoLLVMGetTypeContext(t.C)
	return
}

// Operations on integer types
func (c Context) Int1Type() (t Type)  { t.C = C.TangoLLVMInt1TypeInContext(c.C); return }
func (c Context) Int8Type() (t Type)  { t.C = C.TangoLLVMInt8TypeInContext(c.C); return }
func (c Context) Int16Type() (t Type) { t.C = C.TangoLLVMInt16TypeInContext(c.C); return }
func (c Context) Int32Type() (t Type) { t.C = C.TangoLLVMInt32TypeInContext(c.C); return }
func (c Context) Int64Type() (t Type) { t.C = C.TangoLLVMInt64TypeInContext(c.C); return }
func (c Context) IntType(numbits int) (t Type) {
	t.C = C.TangoLLVMIntTypeInContext(c.C, C.unsigned(numbits))
	return
}

func Int1Type() (t Type)  { t.C = C.TangoLLVMInt1Type(); return }
func Int8Type() (t Type)  { t.C = C.TangoLLVMInt8Type(); return }
func Int16Type() (t Type) { t.C = C.TangoLLVMInt16Type(); return }
func Int32Type() (t Type) { t.C = C.TangoLLVMInt32Type(); return }
func Int64Type() (t Type) { t.C = C.TangoLLVMInt64Type(); return }

func IntType(numbits int) (t Type) {
	t.C = C.TangoLLVMIntType(C.unsigned(numbits))
	return
}

func (t Type) IntTypeWidth() int {
	return int(C.TangoLLVMGetIntTypeWidth(t.C))
}

// Operations on real types
func (c Context) FloatType() (t Type)    { t.C = C.TangoLLVMFloatTypeInContext(c.C); return }
func (c Context) DoubleType() (t Type)   { t.C = C.TangoLLVMDoubleTypeInContext(c.C); return }
func (c Context) X86FP80Type() (t Type)  { t.C = C.TangoLLVMX86FP80TypeInContext(c.C); return }
func (c Context) FP128Type() (t Type)    { t.C = C.TangoLLVMFP128TypeInContext(c.C); return }
func (c Context) PPCFP128Type() (t Type) { t.C = C.TangoLLVMPPCFP128TypeInContext(c.C); return }

// Operations on function types
func FunctionType(returnType Type, paramTypes []Type, isVarArg bool) (t Type) {
	var pt *C.LLVMTypeRef
	var ptlen C.unsigned
	if len(paramTypes) > 0 {
		pt = llvmTypeRefPtr(&paramTypes[0])
		ptlen = C.unsigned(len(paramTypes))
	}
	t.C = C.TangoLLVMFunctionType(returnType.C,
		pt,
		ptlen,
		boolToLLVMBool(isVarArg))
	return
}

func (t Type) IsFunctionVarArg() bool { return C.TangoLLVMIsFunctionVarArg(t.C) != 0 }
func (t Type) ReturnType() (rt Type)  { rt.C = C.TangoLLVMGetReturnType(t.C); return }
func (t Type) ParamTypesCount() int   { return int(C.TangoLLVMCountParamTypes(t.C)) }
func (t Type) ParamTypes() []Type {
	count := t.ParamTypesCount()
	if count > 0 {
		out := make([]Type, count)
		C.TangoLLVMGetParamTypes(t.C, llvmTypeRefPtr(&out[0]))
		return out
	}
	return nil
}

// Operations on struct types
func (c Context) StructType(elementTypes []Type, packed bool) (t Type) {
	var pt *C.LLVMTypeRef
	var ptlen C.unsigned
	if len(elementTypes) > 0 {
		pt = llvmTypeRefPtr(&elementTypes[0])
		ptlen = C.unsigned(len(elementTypes))
	}
	t.C = C.TangoLLVMStructTypeInContext(c.C,
		pt,
		ptlen,
		boolToLLVMBool(packed))
	return
}

func (c Context) GetTypeByName(name string) (t Type) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	t.C = C.TangoLLVMGetTypeByName2(c.C, cname)
	return
}

func (c Context) StructCreateNamed(name string) (t Type) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	t.C = C.TangoLLVMStructCreateNamed(c.C, cname)
	return
}

func (t Type) StructName() string {
	return C.GoString(C.TangoLLVMGetStructName(t.C))
}

func (t Type) StructSetBody(elementTypes []Type, packed bool) {
	var pt *C.LLVMTypeRef
	var ptlen C.unsigned
	if len(elementTypes) > 0 {
		pt = llvmTypeRefPtr(&elementTypes[0])
		ptlen = C.unsigned(len(elementTypes))
	}
	C.TangoLLVMStructSetBody(t.C, pt, ptlen, boolToLLVMBool(packed))
}

func (t Type) IsStructPacked() bool         { return C.TangoLLVMIsPackedStruct(t.C) != 0 }
func (t Type) StructElementTypesCount() int { return int(C.TangoLLVMCountStructElementTypes(t.C)) }
func (t Type) StructElementTypes() []Type {
	out := make([]Type, t.StructElementTypesCount())
	if len(out) > 0 {
		C.TangoLLVMGetStructElementTypes(t.C, llvmTypeRefPtr(&out[0]))
	}
	return out
}

// Operations on array, pointer, and vector types (sequence types)
func (t Type) Subtypes() (ret []Type) {
	ret = make([]Type, C.TangoLLVMGetNumContainedTypes(t.C))
	if len(ret) == 0 {
		return
	}
	C.TangoLLVMGetSubtypes(t.C, llvmTypeRefPtr(&ret[0]))
	return
}

func ArrayType(elementType Type, elementCount int) (t Type) {
	t.C = C.TangoLLVMArrayType(elementType.C, C.unsigned(elementCount))
	return
}
func PointerType(elementType Type, addressSpace int) (t Type) {
	t.C = C.TangoLLVMPointerType(elementType.C, C.unsigned(addressSpace))
	return
}
func VectorType(elementType Type, elementCount int) (t Type) {
	t.C = C.TangoLLVMVectorType(elementType.C, C.unsigned(elementCount))
	return
}

func (t Type) ElementType() (rt Type)   { rt.C = C.TangoLLVMGetElementType(t.C); return }
func (t Type) ArrayLength() int         { return int(C.TangoLLVMGetArrayLength(t.C)) }
func (t Type) PointerAddressSpace() int { return int(C.TangoLLVMGetPointerAddressSpace(t.C)) }
func (t Type) VectorSize() int          { return int(C.TangoLLVMGetVectorSize(t.C)) }

// Operations on other types
func (c Context) VoidType() (t Type)  { t.C = C.TangoLLVMVoidTypeInContext(c.C); return }
func (c Context) LabelType() (t Type) { t.C = C.TangoLLVMLabelTypeInContext(c.C); return }
func (c Context) TokenType() (t Type) { t.C = C.TangoLLVMTokenTypeInContext(c.C); return }
func (c Context) PointerType(addressSpace int) (t Type) {
	t.C = C.TangoLLVMPointerTypeInContext(c.C, C.unsigned(addressSpace))
	return
}

//-------------------------------------------------------------------------
// llvm.Value
//-------------------------------------------------------------------------

// Operations on all values
func (v Value) Type() (t Type)                { t.C = C.TangoLLVMTypeOf(v.C); return }
func (v Value) FunctionType() (rt Type)       { rt.C = C.TangoLLVMGetFunctionType(v.C); return }
func (v Value) GlobalValueType() (rt Type)    { rt.C = C.TangoLLVMGlobalGetValueType(v.C); return }
func (v Value) CalledFunctionType() (rt Type) { rt.C = C.TangoLLVMGetCalledFunctionType(v.C); return }
func (v Value) Name() string                  { return C.GoString(C.TangoLLVMGetValueName(v.C)) }
func (v Value) SetName(name string) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	C.TangoLLVMSetValueName(v.C, cname)
}
func (v Value) Dump()                       { C.TangoLLVMDumpValue(v.C) }
func (v Value) ReplaceAllUsesWith(nv Value) { C.TangoLLVMReplaceAllUsesWith(v.C, nv.C) }
func (v Value) HasMetadata() bool           { return C.TangoLLVMHasMetadata(v.C) != 0 }
func (v Value) Metadata(kind int) (rv Value) {
	rv.C = C.TangoLLVMGetMetadata(v.C, C.unsigned(kind))
	return
}

// The value must be an instruction
func (v Value) SetMetadata(kind int, node Metadata) {
	C.TangoLLVMSetMetadata2(v.C, C.unsigned(kind), node.C)
}

// The value must be a global
func (v Value) GlobalSetMetadata(kind int, node Metadata) {
	C.TangoLLVMGlobalSetMetadata(v.C, C.unsigned(kind), node.C)
}

func (v Value) GetElementAsConstant(index int) (rv Value) {
	rv.C = C.TangoLLVMGetElementAsConstant(v.C, C.unsigned(index))
	return
}

func (v Value) ValueKind() ValueKind {
	return ValueKind(C.TangoLLVMGetValueKind(v.C))
}

// Conversion functions.
// Return the input value if it is an instance of the specified class, otherwise NULL.
// See llvm::dyn_cast_or_null<>.
func (v Value) IsAArgument() (rv Value)   { rv.C = C.TangoLLVMIsAArgument(v.C); return }
func (v Value) IsABasicBlock() (rv Value) { rv.C = C.TangoLLVMIsABasicBlock(v.C); return }
func (v Value) IsAInlineAsm() (rv Value)  { rv.C = C.TangoLLVMIsAInlineAsm(v.C); return }
func (v Value) IsAUser() (rv Value)       { rv.C = C.TangoLLVMIsAUser(v.C); return }
func (v Value) IsAConstant() (rv Value)   { rv.C = C.TangoLLVMIsAConstant(v.C); return }
func (v Value) IsAConstantAggregateZero() (rv Value) {
	rv.C = C.TangoLLVMIsAConstantAggregateZero(v.C)
	return
}

func (v Value) IsAConstantArray() (rv Value) { rv.C = C.TangoLLVMIsAConstantArray(v.C); return }
func (v Value) IsAConstantDataSequential() (rv Value) {
	rv.C = C.TangoLLVMIsAConstantDataSequential(v.C)
	return
}
func (v Value) IsAConstantDataArray() (rv Value) { rv.C = C.TangoLLVMIsAConstantDataArray(v.C); return }
func (v Value) IsAConstantDataVector() (rv Value) {
	rv.C = C.TangoLLVMIsAConstantDataVector(v.C)
	return
}
func (v Value) IsAConstantExpr() (rv Value) { rv.C = C.TangoLLVMIsAConstantExpr(v.C); return }
func (v Value) IsAConstantFP() (rv Value)   { rv.C = C.TangoLLVMIsAConstantFP(v.C); return }
func (v Value) IsAConstantInt() (rv Value)  { rv.C = C.TangoLLVMIsAConstantInt(v.C); return }
func (v Value) IsAConstantPointerNull() (rv Value) {
	rv.C = C.TangoLLVMIsAConstantPointerNull(v.C)
	return
}
func (v Value) IsAConstantStruct() (rv Value)   { rv.C = C.TangoLLVMIsAConstantStruct(v.C); return }
func (v Value) IsAConstantVector() (rv Value)   { rv.C = C.TangoLLVMIsAConstantVector(v.C); return }
func (v Value) IsAGlobalValue() (rv Value)      { rv.C = C.TangoLLVMIsAGlobalValue(v.C); return }
func (v Value) IsAFunction() (rv Value)         { rv.C = C.TangoLLVMIsAFunction(v.C); return }
func (v Value) IsAGlobalAlias() (rv Value)      { rv.C = C.TangoLLVMIsAGlobalAlias(v.C); return }
func (v Value) IsAGlobalVariable() (rv Value)   { rv.C = C.TangoLLVMIsAGlobalVariable(v.C); return }
func (v Value) IsAUndefValue() (rv Value)       { rv.C = C.TangoLLVMIsAUndefValue(v.C); return }
func (v Value) IsAInstruction() (rv Value)      { rv.C = C.TangoLLVMIsAInstruction(v.C); return }
func (v Value) IsABinaryOperator() (rv Value)   { rv.C = C.TangoLLVMIsABinaryOperator(v.C); return }
func (v Value) IsACallInst() (rv Value)         { rv.C = C.TangoLLVMIsACallInst(v.C); return }
func (v Value) IsAIntrinsicInst() (rv Value)    { rv.C = C.TangoLLVMIsAIntrinsicInst(v.C); return }
func (v Value) IsADbgInfoIntrinsic() (rv Value) { rv.C = C.TangoLLVMIsADbgInfoIntrinsic(v.C); return }
func (v Value) IsADbgDeclareInst() (rv Value)   { rv.C = C.TangoLLVMIsADbgDeclareInst(v.C); return }
func (v Value) IsAMemIntrinsic() (rv Value)     { rv.C = C.TangoLLVMIsAMemIntrinsic(v.C); return }
func (v Value) IsAMemCpyInst() (rv Value)       { rv.C = C.TangoLLVMIsAMemCpyInst(v.C); return }
func (v Value) IsAMemMoveInst() (rv Value)      { rv.C = C.TangoLLVMIsAMemMoveInst(v.C); return }
func (v Value) IsAMemSetInst() (rv Value)       { rv.C = C.TangoLLVMIsAMemSetInst(v.C); return }
func (v Value) IsACmpInst() (rv Value)          { rv.C = C.TangoLLVMIsACmpInst(v.C); return }
func (v Value) IsAFCmpInst() (rv Value)         { rv.C = C.TangoLLVMIsAFCmpInst(v.C); return }
func (v Value) IsAICmpInst() (rv Value)         { rv.C = C.TangoLLVMIsAICmpInst(v.C); return }
func (v Value) IsAExtractElementInst() (rv Value) {
	rv.C = C.TangoLLVMIsAExtractElementInst(v.C)
	return
}
func (v Value) IsAGetElementPtrInst() (rv Value) { rv.C = C.TangoLLVMIsAGetElementPtrInst(v.C); return }
func (v Value) IsAInsertElementInst() (rv Value) { rv.C = C.TangoLLVMIsAInsertElementInst(v.C); return }
func (v Value) IsAInsertValueInst() (rv Value)   { rv.C = C.TangoLLVMIsAInsertValueInst(v.C); return }
func (v Value) IsAPHINode() (rv Value)           { rv.C = C.TangoLLVMIsAPHINode(v.C); return }
func (v Value) IsASelectInst() (rv Value)        { rv.C = C.TangoLLVMIsASelectInst(v.C); return }
func (v Value) IsAShuffleVectorInst() (rv Value) { rv.C = C.TangoLLVMIsAShuffleVectorInst(v.C); return }
func (v Value) IsAStoreInst() (rv Value)         { rv.C = C.TangoLLVMIsAStoreInst(v.C); return }
func (v Value) IsABranchInst() (rv Value)        { rv.C = C.TangoLLVMIsABranchInst(v.C); return }
func (v Value) IsAInvokeInst() (rv Value)        { rv.C = C.TangoLLVMIsAInvokeInst(v.C); return }
func (v Value) IsAReturnInst() (rv Value)        { rv.C = C.TangoLLVMIsAReturnInst(v.C); return }
func (v Value) IsASwitchInst() (rv Value)        { rv.C = C.TangoLLVMIsASwitchInst(v.C); return }
func (v Value) IsAUnreachableInst() (rv Value)   { rv.C = C.TangoLLVMIsAUnreachableInst(v.C); return }
func (v Value) IsAUnaryInstruction() (rv Value)  { rv.C = C.TangoLLVMIsAUnaryInstruction(v.C); return }
func (v Value) IsAAllocaInst() (rv Value)        { rv.C = C.TangoLLVMIsAAllocaInst(v.C); return }
func (v Value) IsACastInst() (rv Value)          { rv.C = C.TangoLLVMIsACastInst(v.C); return }
func (v Value) IsABitCastInst() (rv Value)       { rv.C = C.TangoLLVMIsABitCastInst(v.C); return }
func (v Value) IsAFPExtInst() (rv Value)         { rv.C = C.TangoLLVMIsAFPExtInst(v.C); return }
func (v Value) IsAFPToSIInst() (rv Value)        { rv.C = C.TangoLLVMIsAFPToSIInst(v.C); return }
func (v Value) IsAFPToUIInst() (rv Value)        { rv.C = C.TangoLLVMIsAFPToUIInst(v.C); return }
func (v Value) IsAFPTruncInst() (rv Value)       { rv.C = C.TangoLLVMIsAFPTruncInst(v.C); return }
func (v Value) IsAIntToPtrInst() (rv Value)      { rv.C = C.TangoLLVMIsAIntToPtrInst(v.C); return }
func (v Value) IsAPtrToIntInst() (rv Value)      { rv.C = C.TangoLLVMIsAPtrToIntInst(v.C); return }
func (v Value) IsASExtInst() (rv Value)          { rv.C = C.TangoLLVMIsASExtInst(v.C); return }
func (v Value) IsASIToFPInst() (rv Value)        { rv.C = C.TangoLLVMIsASIToFPInst(v.C); return }
func (v Value) IsATruncInst() (rv Value)         { rv.C = C.TangoLLVMIsATruncInst(v.C); return }
func (v Value) IsAUIToFPInst() (rv Value)        { rv.C = C.TangoLLVMIsAUIToFPInst(v.C); return }
func (v Value) IsAZExtInst() (rv Value)          { rv.C = C.TangoLLVMIsAZExtInst(v.C); return }
func (v Value) IsAExtractValueInst() (rv Value)  { rv.C = C.TangoLLVMIsAExtractValueInst(v.C); return }
func (v Value) IsALoadInst() (rv Value)          { rv.C = C.TangoLLVMIsALoadInst(v.C); return }
func (v Value) IsAVAArgInst() (rv Value)         { rv.C = C.TangoLLVMIsAVAArgInst(v.C); return }

// Operations on Uses
func (v Value) FirstUse() (u Use)  { u.C = C.TangoLLVMGetFirstUse(v.C); return }
func (u Use) NextUse() (ru Use)    { ru.C = C.TangoLLVMGetNextUse(u.C); return }
func (u Use) User() (v Value)      { v.C = C.TangoLLVMGetUser(u.C); return }
func (u Use) UsedValue() (v Value) { v.C = C.TangoLLVMGetUsedValue(u.C); return }

// Operations on Users
func (v Value) Operand(i int) (rv Value)   { rv.C = C.TangoLLVMGetOperand(v.C, C.unsigned(i)); return }
func (v Value) SetOperand(i int, op Value) { C.TangoLLVMSetOperand(v.C, C.unsigned(i), op.C) }
func (v Value) OperandsCount() int         { return int(C.TangoLLVMGetNumOperands(v.C)) }

// Operations on constants of any type
func ConstNull(t Type) (v Value)        { v.C = C.TangoLLVMConstNull(t.C); return }
func ConstAllOnes(t Type) (v Value)     { v.C = C.TangoLLVMConstAllOnes(t.C); return }
func Undef(t Type) (v Value)            { v.C = C.TangoLLVMGetUndef(t.C); return }
func (v Value) IsConstant() bool        { return C.TangoLLVMIsConstant(v.C) != 0 }
func (v Value) IsNull() bool            { return C.TangoLLVMIsNull(v.C) != 0 }
func (v Value) IsUndef() bool           { return C.TangoLLVMIsUndef(v.C) != 0 }
func ConstPointerNull(t Type) (v Value) { v.C = C.TangoLLVMConstPointerNull(t.C); return }

// Operations on metadata
func MetadataAsValue(c Context, md Metadata) (v Value) {
	v.C = C.TangoLLVMMetadataAsValue(c.C, md.C)
	return
}

func ValueAsMetadata(v Value) (md Metadata) {
	md.C = C.TangoLLVMValueAsMetadata(v.C)
	return
}

func (c Context) MDString(str string) (md Metadata) {
	cstr := C.CString(str)
	defer C.free(unsafe.Pointer(cstr))
	md.C = C.TangoLLVMMDString2(c.C, cstr, C.unsigned(len(str)))
	return
}

func (c Context) MDNode(mds []Metadata) (md Metadata) {
	ptr, nvals := llvmMetadataRefs(mds)
	md.C = C.TangoLLVMMDNode2(c.C, ptr, nvals)
	return
}
func (v Value) ConstantAsMetadata() (md Metadata) {
	md.C = C.TangoLLVMConstantAsMetadata(v.C)
	return
}

// Operations on scalar constants
func ConstInt(t Type, n uint64, signExtend bool) (v Value) {
	v.C = C.TangoLLVMConstInt(t.C,
		C.ulonglong(n),
		boolToLLVMBool(signExtend))
	return
}

func ConstFloat(t Type, n float64) (v Value) {
	v.C = C.TangoLLVMConstReal(t.C, C.double(n))
	return
}
func ConstFloatFromString(t Type, str string) (v Value) {
	cstr := C.CString(str)
	defer C.free(unsafe.Pointer(cstr))
	v.C = C.TangoLLVMConstRealOfString(t.C, cstr)
	return
}
func (t Type) PrintTypeToString() string {
	return C.GoString(C.TangoLLVMPrintTypeToString(t.C))
}
func (v Value) ZExtValue() uint64 { return uint64(C.TangoLLVMConstIntGetZExtValue(v.C)) }
func (v Value) SExtValue() int64  { return int64(C.TangoLLVMConstIntGetSExtValue(v.C)) }
func (v Value) DoubleValue() (result float64, inexact bool) {
	var losesInfo C.LLVMBool
	doubleResult := C.TangoLLVMConstRealGetDouble(v.C, &losesInfo)
	return float64(doubleResult), losesInfo != 0
}

// Operations on composite constants
func (c Context) ConstString(str string, addnull bool) (v Value) {
	cstr := C.CString(str)
	defer C.free(unsafe.Pointer(cstr))
	v.C = C.TangoLLVMConstStringInContext(c.C, cstr,
		C.unsigned(len(str)), boolToLLVMBool(!addnull))
	return
}
func (c Context) ConstStruct(constVals []Value, packed bool) (v Value) {
	ptr, nvals := llvmValueRefs(constVals)
	v.C = C.TangoLLVMConstStructInContext(c.C, ptr, nvals,
		boolToLLVMBool(packed))
	return
}
func ConstNamedStruct(t Type, constVals []Value) (v Value) {
	ptr, nvals := llvmValueRefs(constVals)
	v.C = C.TangoLLVMConstNamedStruct(t.C, ptr, nvals)
	return
}
func ConstAggregateZero(t Type) (v Value) {
	v.C = C.TangoLLVMConstAggregateZero(t.C)
	return
}
func ConstString(str string, addnull bool) (v Value) {
	cstr := C.CString(str)
	defer C.free(unsafe.Pointer(cstr))
	v.C = C.TangoLLVMConstString(cstr,
		C.unsigned(len(str)), boolToLLVMBool(!addnull))
	return
}
func ConstArray(t Type, constVals []Value) (v Value) {
	ptr, nvals := llvmValueRefs(constVals)
	v.C = C.TangoLLVMConstArray(t.C, ptr, nvals)
	return
}

// Constant expressions
func (v Value) Opcode() Opcode            { return Opcode(C.TangoLLVMGetConstOpcode(v.C)) }
func (v Value) InstructionOpcode() Opcode { return Opcode(C.TangoLLVMGetInstructionOpcode(v.C)) }

// Operations on global variables, functions, and aliases (globals)
func (v Value) GlobalParent() (m Module) { m.C = C.TangoLLVMGetGlobalParent(v.C); return }
func (v Value) IsDeclaration() bool      { return C.TangoLLVMIsDeclaration(v.C) != 0 }
func (v Value) Linkage() Linkage         { return Linkage(C.TangoLLVMGetLinkage(v.C)) }
func (v Value) SetLinkage(l Linkage)     { C.TangoLLVMSetLinkage(v.C, C.LLVMLinkage(l)) }
func (v Value) Section() string          { return C.GoString(C.TangoLLVMGetSection(v.C)) }
func (v Value) SetSection(str string) {
	cstr := C.CString(str)
	defer C.free(unsafe.Pointer(cstr))
	C.TangoLLVMSetSection(v.C, cstr)
}
func (v Value) Visibility() Visibility      { return Visibility(C.TangoLLVMGetVisibility(v.C)) }
func (v Value) SetVisibility(vi Visibility) { C.TangoLLVMSetVisibility(v.C, C.LLVMVisibility(vi)) }
func (v Value) Alignment() int              { return int(C.TangoLLVMGetAlignment(v.C)) }
func (v Value) SetAlignment(a int)          { C.TangoLLVMSetAlignment(v.C, C.unsigned(a)) }
func (v Value) SetUnnamedAddr(ua bool)      { C.TangoLLVMSetUnnamedAddr(v.C, boolToLLVMBool(ua)) }

// Operations on global variables
func AddGlobal(m Module, t Type, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMAddGlobal(m.C, t.C, cname)
	return
}

func (m Module) NamedGlobal(name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMGetNamedGlobal(m.C, cname)
	return
}

func (m Module) FirstGlobal() (v Value)   { v.C = C.TangoLLVMGetFirstGlobal(m.C); return }
func (m Module) LastGlobal() (v Value)    { v.C = C.TangoLLVMGetLastGlobal(m.C); return }
func NextGlobal(v Value) (rv Value)       { rv.C = C.TangoLLVMGetNextGlobal(v.C); return }
func PrevGlobal(v Value) (rv Value)       { rv.C = C.TangoLLVMGetPreviousGlobal(v.C); return }
func (v Value) EraseFromParentAsGlobal()  { C.TangoLLVMDeleteGlobal(v.C) }
func (v Value) Initializer() (rv Value)   { rv.C = C.TangoLLVMGetInitializer(v.C); return }
func (v Value) SetInitializer(cv Value)   { C.TangoLLVMSetInitializer(v.C, cv.C) }
func (v Value) IsThreadLocal() bool       { return C.TangoLLVMIsThreadLocal(v.C) != 0 }
func (v Value) SetThreadLocal(tl bool)    { C.TangoLLVMSetThreadLocal(v.C, boolToLLVMBool(tl)) }
func (v Value) IsGlobalConstant() bool    { return C.TangoLLVMIsGlobalConstant(v.C) != 0 }
func (v Value) SetGlobalConstant(gc bool) { C.TangoLLVMSetGlobalConstant(v.C, boolToLLVMBool(gc)) }
func (v Value) IsVolatile() bool          { return C.TangoLLVMGetVolatile(v.C) != 0 }
func (v Value) SetVolatile(volatile bool) { C.TangoLLVMSetVolatile(v.C, boolToLLVMBool(volatile)) }
func (v Value) Ordering() AtomicOrdering  { return AtomicOrdering(C.TangoLLVMGetOrdering(v.C)) }
func (v Value) SetOrdering(ordering AtomicOrdering) {
	C.TangoLLVMSetOrdering(v.C, C.LLVMAtomicOrdering(ordering))
}
func (v Value) IsAtomicSingleThread() bool { return C.TangoLLVMIsAtomicSingleThread(v.C) != 0 }
func (v Value) SetAtomicSingleThread(singleThread bool) {
	C.TangoLLVMSetAtomicSingleThread(v.C, boolToLLVMBool(singleThread))
}
func (v Value) CmpXchgSuccessOrdering() AtomicOrdering {
	return AtomicOrdering(C.TangoLLVMGetCmpXchgSuccessOrdering(v.C))
}
func (v Value) SetCmpXchgSuccessOrdering(ordering AtomicOrdering) {
	C.TangoLLVMSetCmpXchgSuccessOrdering(v.C, C.LLVMAtomicOrdering(ordering))
}
func (v Value) CmpXchgFailureOrdering() AtomicOrdering {
	return AtomicOrdering(C.TangoLLVMGetCmpXchgFailureOrdering(v.C))
}
func (v Value) SetCmpXchgFailureOrdering(ordering AtomicOrdering) {
	C.TangoLLVMSetCmpXchgFailureOrdering(v.C, C.LLVMAtomicOrdering(ordering))
}

// Operations on functions
func AddFunction(m Module, name string, ft Type) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMAddFunction(m.C, cname, ft.C)
	return
}

func (m Module) NamedFunction(name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMGetNamedFunction(m.C, cname)
	return
}

func (m Module) FirstFunction() (v Value)  { v.C = C.TangoLLVMGetFirstFunction(m.C); return }
func (m Module) LastFunction() (v Value)   { v.C = C.TangoLLVMGetLastFunction(m.C); return }
func NextFunction(v Value) (rv Value)      { rv.C = C.TangoLLVMGetNextFunction(v.C); return }
func PrevFunction(v Value) (rv Value)      { rv.C = C.TangoLLVMGetPreviousFunction(v.C); return }
func (v Value) EraseFromParentAsFunction() { C.TangoLLVMDeleteFunction(v.C) }
func (v Value) IntrinsicID() int           { return int(C.TangoLLVMGetIntrinsicID(v.C)) }
func (v Value) FunctionCallConv() CallConv {
	return CallConv(C.LLVMCallConv(C.TangoLLVMGetFunctionCallConv(v.C)))
}
func (v Value) SetFunctionCallConv(cc CallConv) { C.TangoLLVMSetFunctionCallConv(v.C, C.unsigned(cc)) }
func (v Value) GC() string                      { return C.GoString(C.TangoLLVMGetGC(v.C)) }
func (v Value) SetGC(name string) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	C.TangoLLVMSetGC(v.C, cname)
}
func (v Value) AddAttributeAtIndex(i int, a Attribute) {
	C.TangoLLVMAddAttributeAtIndex(v.C, C.LLVMAttributeIndex(i), a.C)
}
func (v Value) AddFunctionAttr(a Attribute) {
	v.AddAttributeAtIndex(C.LLVMAttributeFunctionIndex, a)
}
func (v Value) GetEnumAttributeAtIndex(i int, kind uint) (a Attribute) {
	a.C = C.TangoLLVMGetEnumAttributeAtIndex(v.C, C.LLVMAttributeIndex(i), C.unsigned(kind))
	return
}
func (v Value) GetEnumFunctionAttribute(kind uint) Attribute {
	return v.GetEnumAttributeAtIndex(C.LLVMAttributeFunctionIndex, kind)
}
func (v Value) GetStringAttributeAtIndex(i int, kind string) (a Attribute) {
	ckind := C.CString(kind)
	defer C.free(unsafe.Pointer(ckind))
	a.C = C.TangoLLVMGetStringAttributeAtIndex(v.C, C.LLVMAttributeIndex(i),
		ckind, C.unsigned(len(kind)))
	return
}
func (v Value) RemoveEnumAttributeAtIndex(i int, kind uint) {
	C.TangoLLVMRemoveEnumAttributeAtIndex(v.C, C.LLVMAttributeIndex(i), C.unsigned(kind))
}
func (v Value) RemoveEnumFunctionAttribute(kind uint) {
	v.RemoveEnumAttributeAtIndex(C.LLVMAttributeFunctionIndex, kind)
}
func (v Value) RemoveStringAttributeAtIndex(i int, kind string) {
	ckind := C.CString(kind)
	defer C.free(unsafe.Pointer(ckind))
	C.TangoLLVMRemoveStringAttributeAtIndex(v.C, C.LLVMAttributeIndex(i),
		ckind, C.unsigned(len(kind)))
}
func (v Value) AddTargetDependentFunctionAttr(attr, value string) {
	cattr := C.CString(attr)
	defer C.free(unsafe.Pointer(cattr))
	cvalue := C.CString(value)
	defer C.free(unsafe.Pointer(cvalue))
	C.TangoLLVMAddTargetDependentFunctionAttr(v.C, cattr, cvalue)
}
func (v Value) SetPersonality(p Value) {
	C.TangoLLVMSetPersonalityFn(v.C, p.C)
}
func (v Value) RemoveFunctionBody() {
	C.TangoLLVMRemoveFunctionBody(v.C)
}

// Operations on parameters
func (v Value) ParamsCount() int { return int(C.TangoLLVMCountParams(v.C)) }
func (v Value) Params() []Value {
	out := make([]Value, v.ParamsCount())
	if len(out) > 0 {
		C.TangoLLVMGetParams(v.C, llvmValueRefPtr(&out[0]))
	}
	return out
}
func (v Value) Param(i int) (rv Value)      { rv.C = C.TangoLLVMGetParam(v.C, C.unsigned(i)); return }
func (v Value) ParamParent() (rv Value)     { rv.C = C.TangoLLVMGetParamParent(v.C); return }
func (v Value) FirstParam() (rv Value)      { rv.C = C.TangoLLVMGetFirstParam(v.C); return }
func (v Value) LastParam() (rv Value)       { rv.C = C.TangoLLVMGetLastParam(v.C); return }
func NextParam(v Value) (rv Value)          { rv.C = C.TangoLLVMGetNextParam(v.C); return }
func PrevParam(v Value) (rv Value)          { rv.C = C.TangoLLVMGetPreviousParam(v.C); return }
func (v Value) SetParamAlignment(align int) { C.TangoLLVMSetParamAlignment(v.C, C.unsigned(align)) }

// Operations on basic blocks
func (bb BasicBlock) AsValue() (v Value)      { v.C = C.TangoLLVMBasicBlockAsValue(bb.C); return }
func (v Value) IsBasicBlock() bool            { return C.TangoLLVMValueIsBasicBlock(v.C) != 0 }
func (v Value) AsBasicBlock() (bb BasicBlock) { bb.C = C.TangoLLVMValueAsBasicBlock(v.C); return }
func (bb BasicBlock) Parent() (v Value)       { v.C = C.TangoLLVMGetBasicBlockParent(bb.C); return }
func (v Value) BasicBlocksCount() int         { return int(C.TangoLLVMCountBasicBlocks(v.C)) }
func (v Value) BasicBlocks() []BasicBlock {
	out := make([]BasicBlock, v.BasicBlocksCount())
	C.TangoLLVMGetBasicBlocks(v.C, llvmBasicBlockRefPtr(&out[0]))
	return out
}
func (v Value) FirstBasicBlock() (bb BasicBlock) {
	bb.C = C.TangoLLVMGetFirstBasicBlock(v.C)
	return
}
func (v Value) LastBasicBlock() (bb BasicBlock) {
	bb.C = C.TangoLLVMGetLastBasicBlock(v.C)
	return
}
func NextBasicBlock(bb BasicBlock) (rbb BasicBlock) {
	rbb.C = C.TangoLLVMGetNextBasicBlock(bb.C)
	return
}
func PrevBasicBlock(bb BasicBlock) (rbb BasicBlock) {
	rbb.C = C.TangoLLVMGetPreviousBasicBlock(bb.C)
	return
}
func (v Value) EntryBasicBlock() (bb BasicBlock) {
	bb.C = C.TangoLLVMGetEntryBasicBlock(v.C)
	return
}
func (c Context) AddBasicBlock(f Value, name string) (bb BasicBlock) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	bb.C = C.TangoLLVMAppendBasicBlockInContext(c.C, f.C, cname)
	return
}
func (c Context) InsertBasicBlock(ref BasicBlock, name string) (bb BasicBlock) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	bb.C = C.TangoLLVMInsertBasicBlockInContext(c.C, ref.C, cname)
	return
}
func AddBasicBlock(f Value, name string) (bb BasicBlock) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	bb.C = C.TangoLLVMAppendBasicBlock(f.C, cname)
	return
}
func InsertBasicBlock(ref BasicBlock, name string) (bb BasicBlock) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	bb.C = C.TangoLLVMInsertBasicBlock(ref.C, cname)
	return
}
func (bb BasicBlock) EraseFromParent()          { C.TangoLLVMDeleteBasicBlock(bb.C) }
func (bb BasicBlock) MoveBefore(pos BasicBlock) { C.TangoLLVMMoveBasicBlockBefore(bb.C, pos.C) }
func (bb BasicBlock) MoveAfter(pos BasicBlock)  { C.TangoLLVMMoveBasicBlockAfter(bb.C, pos.C) }
func (bb BasicBlock) Name() string {
	cname := C.TangoLLVMGetBasicBlockName(bb.C)
	name := C.GoString(cname)
	return name
}

// Operations on instructions
func (v Value) EraseFromParentAsInstruction()  { C.TangoLLVMInstructionEraseFromParent(v.C) }
func (v Value) RemoveFromParentAsInstruction() { C.TangoLLVMInstructionRemoveFromParent(v.C) }
func (v Value) InstructionParent() (bb BasicBlock) {
	bb.C = C.TangoLLVMGetInstructionParent(v.C)
	return
}
func (v Value) InstructionDebugLoc() (md Metadata) {
	md.C = C.TangoLLVMInstructionGetDebugLoc(v.C)
	return
}
func (v Value) InstructionSetDebugLoc(md Metadata) { C.TangoLLVMInstructionSetDebugLoc(v.C, md.C) }
func (bb BasicBlock) FirstInstruction() (v Value)  { v.C = C.TangoLLVMGetFirstInstruction(bb.C); return }
func (bb BasicBlock) LastInstruction() (v Value)   { v.C = C.TangoLLVMGetLastInstruction(bb.C); return }
func NextInstruction(v Value) (rv Value)           { rv.C = C.TangoLLVMGetNextInstruction(v.C); return }
func PrevInstruction(v Value) (rv Value)           { rv.C = C.TangoLLVMGetPreviousInstruction(v.C); return }

// Operations on call sites
func (v Value) SetInstructionCallConv(cc CallConv) {
	C.TangoLLVMSetInstructionCallConv(v.C, C.unsigned(cc))
}
func (v Value) InstructionCallConv() CallConv {
	return CallConv(C.LLVMCallConv(C.TangoLLVMGetInstructionCallConv(v.C)))
}
func (v Value) AddCallSiteAttribute(i int, a Attribute) {
	C.TangoLLVMAddCallSiteAttribute(v.C, C.LLVMAttributeIndex(i), a.C)
}
func (v Value) AddCallSiteFnAttribute(a Attribute) {
	C.TangoLLVMAddCallSiteFnAttribute(v.C, a.C)
}
func (v Value) SetInstrParamAlignment(i int, align int) {
	C.TangoLLVMSetInstrParamAlignment(v.C, C.unsigned(i), C.unsigned(align))
}
func (v Value) CalledValue() (rv Value) {
	rv.C = C.TangoLLVMGetCalledValue(v.C)
	return
}

// Operations on call instructions (only)
func (v Value) IsTailCall() bool    { return C.TangoLLVMIsTailCall(v.C) != 0 }
func (v Value) SetTailCall(is bool) { C.TangoLLVMSetTailCall(v.C, boolToLLVMBool(is)) }

// Operations on phi nodes
func (v Value) AddIncoming(vals []Value, blocks []BasicBlock) {
	ptr, nvals := llvmValueRefs(vals)
	C.TangoLLVMAddIncoming(v.C, ptr, llvmBasicBlockRefPtr(&blocks[0]), nvals)
}
func (v Value) IncomingCount() int { return int(C.TangoLLVMCountIncoming(v.C)) }
func (v Value) IncomingValue(i int) (rv Value) {
	rv.C = C.TangoLLVMGetIncomingValue(v.C, C.unsigned(i))
	return
}
func (v Value) IncomingBlock(i int) (bb BasicBlock) {
	bb.C = C.TangoLLVMGetIncomingBlock(v.C, C.unsigned(i))
	return
}

// Operations on inline assembly
func InlineAsm(t Type, asmString, constraints string, hasSideEffects, isAlignStack bool, dialect InlineAsmDialect, canThrow bool) (rv Value) {
	casm := C.CString(asmString)
	defer C.free(unsafe.Pointer(casm))
	cconstraints := C.CString(constraints)
	defer C.free(unsafe.Pointer(cconstraints))
	rv.C = C.TangoLLVMGoGetInlineAsm(t.C, casm, C.size_t(len(asmString)), cconstraints, C.size_t(len(constraints)), boolToLLVMBool(hasSideEffects), boolToLLVMBool(isAlignStack), C.LLVMInlineAsmDialect(dialect), boolToLLVMBool(canThrow))
	return
}

// Operations on aggregates
func (v Value) Indices() []uint32 {
	num := C.TangoLLVMGetNumIndices(v.C)
	indicesPtr := C.TangoLLVMGetIndices(v.C)
	// https://github.com/golang/go/wiki/cgo#turning-c-arrays-into-go-slices
	rawIndices := (*[1 << 20]C.uint)(unsafe.Pointer(indicesPtr))[:num:num]
	indices := make([]uint32, num)
	for i := range indices {
		indices[i] = uint32(rawIndices[i])
	}
	return indices
}

// Operations on comparisons
func (v Value) IntPredicate() IntPredicate { return IntPredicate(C.TangoLLVMGetICmpPredicate(v.C)) }
func (v Value) FloatPredicate() FloatPredicate {
	return FloatPredicate(C.TangoLLVMGetFCmpPredicate(v.C))
}

//-------------------------------------------------------------------------
// llvm.Builder
//-------------------------------------------------------------------------

// An instruction builder represents a point within a basic block, and is the
// exclusive means of building instructions using the C interface.

func (c Context) NewBuilder() (b Builder) { b.C = C.TangoLLVMCreateBuilderInContext(c.C); return }
func NewBuilder() (b Builder)             { b.C = C.TangoLLVMCreateBuilder(); return }
func (b Builder) SetInsertPoint(block BasicBlock, instr Value) {
	C.TangoLLVMPositionBuilder(b.C, block.C, instr.C)
}
func (b Builder) SetInsertPointBefore(instr Value)     { C.TangoLLVMPositionBuilderBefore(b.C, instr.C) }
func (b Builder) SetInsertPointAtEnd(block BasicBlock) { C.TangoLLVMPositionBuilderAtEnd(b.C, block.C) }
func (b Builder) GetInsertBlock() (bb BasicBlock)      { bb.C = C.TangoLLVMGetInsertBlock(b.C); return }
func (b Builder) ClearInsertionPoint()                 { C.TangoLLVMClearInsertionPosition(b.C) }
func (b Builder) Insert(instr Value)                   { C.TangoLLVMInsertIntoBuilder(b.C, instr.C) }
func (b Builder) InsertWithName(instr Value, name string) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	C.TangoLLVMInsertIntoBuilderWithName(b.C, instr.C, cname)
}
func (b Builder) Dispose() { C.TangoLLVMDisposeBuilder(b.C) }

// Metadata
type DebugLoc struct {
	Line, Col uint
	Scope     Metadata
	InlinedAt Metadata
}

func (b Builder) SetCurrentDebugLocation(line, col uint, scope, inlinedAt Metadata) {
	C.TangoLLVMGoSetCurrentDebugLocation(b.C, C.unsigned(line), C.unsigned(col), scope.C, inlinedAt.C)
}

func (b Builder) SetCurrentDebugLocation2(loc Metadata) {
	C.TangoLLVMSetCurrentDebugLocation2(b.C, loc.C)
}

// Get current debug location. Please do not call this function until setting debug location with SetCurrentDebugLocation()
func (b Builder) GetCurrentDebugLocation() (loc DebugLoc) {
	md := C.TangoLLVMGoGetCurrentDebugLocation(b.C)
	loc.Line = uint(md.Line)
	loc.Col = uint(md.Col)
	loc.Scope = Metadata{C: md.Scope}
	loc.InlinedAt = Metadata{C: md.InlinedAt}
	return
}
func (b Builder) SetInstDebugLocation(v Value) { C.TangoLLVMSetInstDebugLocation(b.C, v.C) }

// Terminators
func (b Builder) CreateRetVoid() (rv Value)    { rv.C = C.TangoLLVMBuildRetVoid(b.C); return }
func (b Builder) CreateRet(v Value) (rv Value) { rv.C = C.TangoLLVMBuildRet(b.C, v.C); return }
func (b Builder) CreateAggregateRet(vs []Value) (rv Value) {
	ptr, nvals := llvmValueRefs(vs)
	rv.C = C.TangoLLVMBuildAggregateRet(b.C, ptr, nvals)
	return
}
func (b Builder) CreateBr(bb BasicBlock) (rv Value) { rv.C = C.TangoLLVMBuildBr(b.C, bb.C); return }
func (b Builder) CreateCondBr(ifv Value, thenb, elseb BasicBlock) (rv Value) {
	rv.C = C.TangoLLVMBuildCondBr(b.C, ifv.C, thenb.C, elseb.C)
	return
}
func (b Builder) CreateSwitch(v Value, elseb BasicBlock, numCases int) (rv Value) {
	rv.C = C.TangoLLVMBuildSwitch(b.C, v.C, elseb.C, C.unsigned(numCases))
	return
}
func (b Builder) CreateIndirectBr(addr Value, numDests int) (rv Value) {
	rv.C = C.TangoLLVMBuildIndirectBr(b.C, addr.C, C.unsigned(numDests))
	return
}

func (b Builder) CreateUnreachable() (rv Value) { rv.C = C.TangoLLVMBuildUnreachable(b.C); return }

// Add a case to the switch instruction
func (v Value) AddCase(on Value, dest BasicBlock) { C.TangoLLVMAddCase(v.C, on.C, dest.C) }

// Add a destination to the indirectbr instruction
func (v Value) AddDest(dest BasicBlock) { C.TangoLLVMAddDestination(v.C, dest.C) }

// Arithmetic
func (b Builder) CreateAdd(lhs, rhs Value, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildAdd(b.C, lhs.C, rhs.C, cname)
	return
}
func (b Builder) CreateNSWAdd(lhs, rhs Value, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildNSWAdd(b.C, lhs.C, rhs.C, cname)
	return
}
func (b Builder) CreateNUWAdd(lhs, rhs Value, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildNUWAdd(b.C, lhs.C, rhs.C, cname)
	return
}
func (b Builder) CreateFAdd(lhs, rhs Value, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildFAdd(b.C, lhs.C, rhs.C, cname)
	return
}
func (b Builder) CreateSub(lhs, rhs Value, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildSub(b.C, lhs.C, rhs.C, cname)
	return
}
func (b Builder) CreateNSWSub(lhs, rhs Value, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildNSWSub(b.C, lhs.C, rhs.C, cname)
	return
}
func (b Builder) CreateNUWSub(lhs, rhs Value, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildNUWSub(b.C, lhs.C, rhs.C, cname)
	return
}
func (b Builder) CreateFSub(lhs, rhs Value, name string) (v Value) {
	cname := C.CString(name)
	v.C = C.TangoLLVMBuildFSub(b.C, lhs.C, rhs.C, cname)
	C.free(unsafe.Pointer(cname))
	return
}
func (b Builder) CreateMul(lhs, rhs Value, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildMul(b.C, lhs.C, rhs.C, cname)
	return
}
func (b Builder) CreateNSWMul(lhs, rhs Value, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildNSWMul(b.C, lhs.C, rhs.C, cname)
	return
}
func (b Builder) CreateNUWMul(lhs, rhs Value, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildNUWMul(b.C, lhs.C, rhs.C, cname)
	return
}
func (b Builder) CreateFMul(lhs, rhs Value, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildFMul(b.C, lhs.C, rhs.C, cname)
	return
}
func (b Builder) CreateUDiv(lhs, rhs Value, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildUDiv(b.C, lhs.C, rhs.C, cname)
	return
}
func (b Builder) CreateSDiv(lhs, rhs Value, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildSDiv(b.C, lhs.C, rhs.C, cname)
	return
}
func (b Builder) CreateExactSDiv(lhs, rhs Value, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildExactSDiv(b.C, lhs.C, rhs.C, cname)
	return
}
func (b Builder) CreateFDiv(lhs, rhs Value, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildFDiv(b.C, lhs.C, rhs.C, cname)
	return
}
func (b Builder) CreateURem(lhs, rhs Value, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildURem(b.C, lhs.C, rhs.C, cname)
	return
}
func (b Builder) CreateSRem(lhs, rhs Value, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildSRem(b.C, lhs.C, rhs.C, cname)
	return
}
func (b Builder) CreateFRem(lhs, rhs Value, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildFRem(b.C, lhs.C, rhs.C, cname)
	return
}
func (b Builder) CreateShl(lhs, rhs Value, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildShl(b.C, lhs.C, rhs.C, cname)
	return
}
func (b Builder) CreateLShr(lhs, rhs Value, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildLShr(b.C, lhs.C, rhs.C, cname)
	return
}
func (b Builder) CreateAShr(lhs, rhs Value, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildAShr(b.C, lhs.C, rhs.C, cname)
	return
}
func (b Builder) CreateAnd(lhs, rhs Value, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildAnd(b.C, lhs.C, rhs.C, cname)
	return
}
func (b Builder) CreateOr(lhs, rhs Value, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildOr(b.C, lhs.C, rhs.C, cname)
	return
}
func (b Builder) CreateXor(lhs, rhs Value, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildXor(b.C, lhs.C, rhs.C, cname)
	return
}
func (b Builder) CreateBinOp(op Opcode, lhs, rhs Value, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildBinOp(b.C, C.LLVMOpcode(op), lhs.C, rhs.C, cname)
	return
}
func (b Builder) CreateNeg(v Value, name string) (rv Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	rv.C = C.TangoLLVMBuildNeg(b.C, v.C, cname)
	return
}
func (b Builder) CreateNSWNeg(v Value, name string) (rv Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	rv.C = C.TangoLLVMBuildNSWNeg(b.C, v.C, cname)
	return
}
func (b Builder) CreateNUWNeg(v Value, name string) (rv Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	rv.C = C.TangoLLVMBuildNUWNeg(b.C, v.C, cname)
	return
}
func (b Builder) CreateFNeg(v Value, name string) (rv Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	rv.C = C.TangoLLVMBuildFNeg(b.C, v.C, cname)
	return
}
func (b Builder) CreateNot(v Value, name string) (rv Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	rv.C = C.TangoLLVMBuildNot(b.C, v.C, cname)
	return
}

// Memory

func (b Builder) CreateMalloc(t Type, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildMalloc(b.C, t.C, cname)
	return
}
func (b Builder) CreateArrayMalloc(t Type, val Value, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildArrayMalloc(b.C, t.C, val.C, cname)
	return
}
func (b Builder) CreateAlloca(t Type, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildAlloca(b.C, t.C, cname)
	return
}
func (b Builder) CreateArrayAlloca(t Type, val Value, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildArrayAlloca(b.C, t.C, val.C, cname)
	return
}
func (b Builder) CreateFree(p Value) (v Value) {
	v.C = C.TangoLLVMBuildFree(b.C, p.C)
	return
}
func (b Builder) CreateLoad(t Type, p Value, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildLoad2(b.C, t.C, p.C, cname)
	return
}
func (b Builder) CreateStore(val Value, p Value) (v Value) {
	v.C = C.TangoLLVMBuildStore(b.C, val.C, p.C)
	return
}
func (b Builder) CreateGEP(t Type, p Value, indices []Value, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	ptr, nvals := llvmValueRefs(indices)
	v.C = C.TangoLLVMBuildGEP2(b.C, t.C, p.C, ptr, nvals, cname)
	return
}

func (b Builder) CreateStructGEP(p Value, i int, name string) (v Value) {
	// cname := C.CString(name)
	// defer C.free(unsafe.Pointer(cname))
	// v.C = C.TangoLLVMBuildStructGEP(b.C, p.C, C.unsigned(i), cname)
	return
}
func (b Builder) CreateGlobalString(str, name string) (v Value) {
	cstr := C.CString(str)
	defer C.free(unsafe.Pointer(cstr))
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildGlobalString(b.C, cstr, cname)
	return
}
func (b Builder) CreateGlobalStringPtr(str, name string) (v Value) {
	cstr := C.CString(str)
	defer C.free(unsafe.Pointer(cstr))
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildGlobalStringPtr(b.C, cstr, cname)
	return
}
func (b Builder) CreateAtomicRMW(op AtomicRMWBinOp, ptr, val Value, ordering AtomicOrdering, singleThread bool) (v Value) {
	v.C = C.TangoLLVMBuildAtomicRMW(b.C, C.LLVMAtomicRMWBinOp(op), ptr.C, val.C, C.LLVMAtomicOrdering(ordering), boolToLLVMBool(singleThread))
	return
}
func (b Builder) CreateAtomicCmpXchg(ptr, cmp, newVal Value, successOrdering, failureOrdering AtomicOrdering, singleThread bool) (v Value) {
	v.C = C.TangoLLVMBuildAtomicCmpXchg(b.C, ptr.C, cmp.C, newVal.C, C.LLVMAtomicOrdering(successOrdering), C.LLVMAtomicOrdering(failureOrdering), boolToLLVMBool(singleThread))
	return
}

// MemIntrinsic
func (b Builder) CreateMemSet(ptr Value, val Value, len Value, align int) (v Value) {
	v.C = C.TangoLLVMBuildMemSet(b.C, ptr.C, val.C, len.C, C.unsigned(align))
	return
}
func (b Builder) CreateMemCpy(dst Value, dstAlign int, src Value, srcAlign int, size Value) (v Value) {
	v.C = C.TangoLLVMBuildMemCpy(b.C, dst.C, C.unsigned(dstAlign), src.C, C.unsigned(srcAlign), size.C)
	return
}
func (b Builder) CreateMemCpyInline(dst Value, dstAlign int, src Value, srcAlign int, size Value) (v Value) {
	v.C = C.TangoLLVMBuildMemCpyInline(b.C, dst.C, C.unsigned(dstAlign), src.C, C.unsigned(srcAlign), size.C)
	return
}
func (b Builder) CreateMemMove(dst Value, dstAlign int, src Value, srcAlign int, size Value) (v Value) {
	v.C = C.TangoLLVMBuildMemMove(b.C, dst.C, C.unsigned(dstAlign), src.C, C.unsigned(srcAlign), size.C)
	return
}

// Casts
func (b Builder) CreateTrunc(val Value, t Type, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildTrunc(b.C, val.C, t.C, cname)
	return
}
func (b Builder) CreateZExt(val Value, t Type, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildZExt(b.C, val.C, t.C, cname)
	return
}
func (b Builder) CreateSExt(val Value, t Type, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildSExt(b.C, val.C, t.C, cname)
	return
}
func (b Builder) CreateFPToUI(val Value, t Type, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildFPToUI(b.C, val.C, t.C, cname)
	return
}
func (b Builder) CreateFPToSI(val Value, t Type, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildFPToSI(b.C, val.C, t.C, cname)
	return
}
func (b Builder) CreateUIToFP(val Value, t Type, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildUIToFP(b.C, val.C, t.C, cname)
	return
}
func (b Builder) CreateSIToFP(val Value, t Type, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildSIToFP(b.C, val.C, t.C, cname)
	return
}
func (b Builder) CreateFPTrunc(val Value, t Type, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildFPTrunc(b.C, val.C, t.C, cname)
	return
}
func (b Builder) CreateFPExt(val Value, t Type, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildFPExt(b.C, val.C, t.C, cname)
	return
}
func (b Builder) CreatePtrToInt(val Value, t Type, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildPtrToInt(b.C, val.C, t.C, cname)
	return
}
func (b Builder) CreateIntToPtr(val Value, t Type, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildIntToPtr(b.C, val.C, t.C, cname)
	return
}
func (b Builder) CreateBitCast(val Value, t Type, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildBitCast(b.C, val.C, t.C, cname)
	return
}
func (b Builder) CreateZExtOrBitCast(val Value, t Type, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildZExtOrBitCast(b.C, val.C, t.C, cname)
	return
}
func (b Builder) CreateSExtOrBitCast(val Value, t Type, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildSExtOrBitCast(b.C, val.C, t.C, cname)
	return
}
func (b Builder) CreateTruncOrBitCast(val Value, t Type, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildTruncOrBitCast(b.C, val.C, t.C, cname)
	return
}
func (b Builder) CreateCast(val Value, op Opcode, t Type, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildCast(b.C, C.LLVMOpcode(op), val.C, t.C, cname)
	return
} //
func (b Builder) CreatePointerCast(val Value, t Type, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildPointerCast(b.C, val.C, t.C, cname)
	return
}
func (b Builder) CreateIntCast(val Value, t Type, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildIntCast(b.C, val.C, t.C, cname)
	return
}
func (b Builder) CreateFPCast(val Value, t Type, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildFPCast(b.C, val.C, t.C, cname)
	return
}

// Comparisons
func (b Builder) CreateICmp(pred IntPredicate, lhs, rhs Value, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildICmp(b.C, C.LLVMIntPredicate(pred), lhs.C, rhs.C, cname)
	return
}
func (b Builder) CreateFCmp(pred FloatPredicate, lhs, rhs Value, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildFCmp(b.C, C.LLVMRealPredicate(pred), lhs.C, rhs.C, cname)
	return
}

// Miscellaneous instructions
func (b Builder) CreatePHI(t Type, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildPhi(b.C, t.C, cname)
	return
}
func (b Builder) CreateCall(t Type, fn Value, args []Value, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	ptr, nvals := llvmValueRefs(args)
	v.C = C.TangoLLVMBuildCall2(b.C, t.C, fn.C, ptr, nvals, cname)
	return
}

func LookupIntrinsicID(name string) int {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	return int(C.TangoLLVMLookupIntrinsicID(cname, C.size_t(len(name))))
}

func (b Builder) CreateIntrinsic(id int, types []Type, args []Value, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	ptr, nvals := llvmValueRefs(args)
	tptr, ntypes := llvmTypeRefs(types)
	v.C = C.TangoLLVMBuildIntrinsic(b.C, C.unsigned(id), tptr, ntypes, ptr, nvals, cname)
	return
}

func (b Builder) CreateSelect(ifv, thenv, elsev Value, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildSelect(b.C, ifv.C, thenv.C, elsev.C, cname)
	return
}

func (b Builder) CreateVAArg(list Value, t Type, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildVAArg(b.C, list.C, t.C, cname)
	return
}
func (b Builder) CreateExtractElement(vec, i Value, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildExtractElement(b.C, vec.C, i.C, cname)
	return
}
func (b Builder) CreateInsertElement(vec, elt, i Value, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildInsertElement(b.C, vec.C, elt.C, i.C, cname)
	return
}
func (b Builder) CreateShuffleVector(v1, v2, mask Value, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildShuffleVector(b.C, v1.C, v2.C, mask.C, cname)
	return
}
func (b Builder) CreateExtractValue(agg Value, i int, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildExtractValue(b.C, agg.C, C.unsigned(i), cname)
	return
}
func (b Builder) CreateInsertValue(agg, elt Value, i int, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildInsertValue(b.C, agg.C, elt.C, C.unsigned(i), cname)
	return
}

func (b Builder) CreateIsNull(val Value, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildIsNull(b.C, val.C, cname)
	return
}
func (b Builder) CreateIsNotNull(val Value, name string) (v Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	v.C = C.TangoLLVMBuildIsNotNull(b.C, val.C, cname)
	return
}
func (b Builder) CreatePtrDiff(lhs, rhs Value, name string) (v Value) {
	// cname := C.CString(name)
	// defer C.free(unsafe.Pointer(cname))
	// v.C = C.TangoLLVMBuildPtrDiff(b.C, lhs.C, rhs.C, cname)
	return
}

func (b Builder) CreateLandingPad(t Type, nclauses int, name string) (l Value) {
	cname := C.CString(name)
	defer C.free(unsafe.Pointer(cname))
	l.C = C.TangoLLVMBuildLandingPad(b.C, t.C, nil, C.unsigned(nclauses), cname)
	return l
}

func (l Value) AddClause(v Value) {
	C.TangoLLVMAddClause(l.C, v.C)
}

func (l Value) SetCleanup(cleanup bool) {
	C.TangoLLVMSetCleanup(l.C, boolToLLVMBool(cleanup))
}

func (b Builder) CreateResume(ex Value) (v Value) {
	v.C = C.TangoLLVMBuildResume(b.C, ex.C)
	return
}

func (b Builder) CreateGEPByOffset(base Value, elemType Type, offset uint64) (v Value) {
	v.C = C.TangoLLVMBuildGEPByOffset(b.C, base.C, elemType.C, C.uint64_t(offset))
	return
}
