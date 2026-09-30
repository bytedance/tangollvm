//===- target.go - Bindings for target ------------------------------------===//
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
// This file defines bindings for the target component.
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
	"errors"
	"unsafe"
)

type (
	TargetData struct {
		C C.LLVMTargetDataRef
	}
	Target struct {
		C C.LLVMTargetRef
	}
	TargetMachine struct {
		C C.LLVMTargetMachineRef
	}
	ByteOrdering    C.enum_LLVMByteOrdering
	RelocMode       C.LLVMRelocMode
	CodeGenOptLevel C.LLVMCodeGenOptLevel
	CodeGenFileType C.LLVMCodeGenFileType
	CodeModel       C.LLVMCodeModel
)

const (
	BigEndian    ByteOrdering = C.LLVMBigEndian
	LittleEndian ByteOrdering = C.LLVMLittleEndian
)

const (
	RelocDefault      RelocMode = C.LLVMRelocDefault
	RelocStatic       RelocMode = C.LLVMRelocStatic
	RelocPIC          RelocMode = C.LLVMRelocPIC
	RelocDynamicNoPic RelocMode = C.LLVMRelocDynamicNoPic
)

const (
	CodeGenLevelNone       CodeGenOptLevel = C.LLVMCodeGenLevelNone
	CodeGenLevelLess       CodeGenOptLevel = C.LLVMCodeGenLevelLess
	CodeGenLevelDefault    CodeGenOptLevel = C.LLVMCodeGenLevelDefault
	CodeGenLevelAggressive CodeGenOptLevel = C.LLVMCodeGenLevelAggressive
)

const (
	CodeModelDefault    CodeModel = C.LLVMCodeModelDefault
	CodeModelJITDefault CodeModel = C.LLVMCodeModelJITDefault
	CodeModelTiny       CodeModel = C.LLVMCodeModelTiny
	CodeModelSmall      CodeModel = C.LLVMCodeModelSmall
	CodeModelKernel     CodeModel = C.LLVMCodeModelKernel
	CodeModelMedium     CodeModel = C.LLVMCodeModelMedium
	CodeModelLarge      CodeModel = C.LLVMCodeModelLarge
)

const (
	AssemblyFile CodeGenFileType = C.LLVMAssemblyFile
	ObjectFile   CodeGenFileType = C.LLVMObjectFile
)

func InitializeAll() { C.TangoLLVMInitializeAll() }

//-------------------------------------------------------------------------
// llvm.TargetData
//-------------------------------------------------------------------------

// Converts target data to a target layout string. The string must be disposed
// with LLVMDisposeMessage.
// See the constructor llvm::TargetData::TargetData.
func (td TargetData) String() (s string) {
	cmsg := C.TangoLLVMCopyStringRepOfTargetData(td.C)
	s = C.GoString(cmsg)
	C.TangoLLVMDisposeMessage(cmsg)
	return
}

// Returns the byte order of a target, either BigEndian or LittleEndian.
// See the method llvm::TargetData::isLittleEndian.
func (td TargetData) ByteOrder() ByteOrdering { return ByteOrdering(C.TangoLLVMByteOrder(td.C)) }

// Returns the pointer size in bytes for a target.
// See the method llvm::TargetData::getPointerSize.
func (td TargetData) PointerSize() int { return int(C.TangoLLVMPointerSize(td.C)) }

// Returns the integer type that is the same size as a pointer on a target.
// See the method llvm::TargetData::getIntPtrType.
func (td TargetData) IntPtrType() (t Type) { t.C = C.TangoLLVMIntPtrType(td.C); return }

// Computes the size of a type in bytes for a target.
// See the method llvm::TargetData::getTypeSizeInBits.
func (td TargetData) TypeSizeInBits(t Type) uint64 {
	return uint64(C.TangoLLVMSizeOfTypeInBits(td.C, t.C))
}

// Computes the storage size of a type in bytes for a target.
// See the method llvm::TargetData::getTypeStoreSize.
func (td TargetData) TypeStoreSize(t Type) uint64 {
	return uint64(C.TangoLLVMStoreSizeOfType(td.C, t.C))
}

// Computes the ABI size of a type in bytes for a target.
// See the method llvm::TargetData::getTypeAllocSize.
func (td TargetData) TypeAllocSize(t Type) uint64 {
	return uint64(C.TangoLLVMABISizeOfType(td.C, t.C))
}

// Computes the ABI alignment of a type in bytes for a target.
// See the method llvm::TargetData::getABITypeAlignment.
func (td TargetData) ABITypeAlignment(t Type) int {
	return int(C.TangoLLVMABIAlignmentOfType(td.C, t.C))
}

// Computes the call frame alignment of a type in bytes for a target.
// See the method llvm::TargetData::getCallFrameTypeAlignment.
func (td TargetData) CallFrameTypeAlignment(t Type) int {
	return int(C.TangoLLVMCallFrameAlignmentOfType(td.C, t.C))
}

// Computes the preferred alignment of a type in bytes for a target.
// See the method llvm::TargetData::getPrefTypeAlignment.
func (td TargetData) PrefTypeAlignment(t Type) int {
	return int(C.TangoLLVMPreferredAlignmentOfType(td.C, t.C))
}

// Computes the preferred alignment of a global variable in bytes for a target.
// See the method llvm::TargetData::getPreferredAlignment.
func (td TargetData) PreferredAlignment(g Value) int {
	return int(C.TangoLLVMPreferredAlignmentOfGlobal(td.C, g.C))
}

// Computes the structure element that contains the byte offset for a target.
// See the method llvm::StructLayout::getElementContainingOffset.
func (td TargetData) ElementContainingOffset(t Type, offset uint64) int {
	return int(C.TangoLLVMElementAtOffset(td.C, t.C, C.ulonglong(offset)))
}

// Computes the byte offset of the indexed struct element for a target.
// See the method llvm::StructLayout::getElementOffset.
func (td TargetData) ElementOffset(t Type, element int) uint64 {
	return uint64(C.TangoLLVMOffsetOfElement(td.C, t.C, C.unsigned(element)))
}

// Deallocates a TargetData.
// See the destructor llvm::TargetData::~TargetData.
func (td TargetData) Dispose() { C.TangoLLVMDisposeTargetData(td.C) }

//-------------------------------------------------------------------------
// llvm.Target
//-------------------------------------------------------------------------

func GetTargetFromTriple(triple string) (t Target, err error) {
	var errstr *C.char
	ctriple := C.CString(triple)
	defer C.free(unsafe.Pointer(ctriple))
	fail := C.TangoLLVMGetTargetFromTriple(ctriple, &t.C, &errstr)
	if fail != 0 {
		err = errors.New(C.GoString(errstr))
		C.free(unsafe.Pointer(errstr))
	}
	return
}

func (t Target) Name() string {
	return C.GoString(C.TangoLLVMGetTargetName(t.C))
}

func (t Target) Description() string {
	return C.GoString(C.TangoLLVMGetTargetDescription(t.C))
}

//-------------------------------------------------------------------------
// llvm.TargetMachine
//-------------------------------------------------------------------------

// CreateTargetMachine creates a new TargetMachine.
func (t Target) CreateTargetMachine(Triple string, CPU string, Features string,
	Level CodeGenOptLevel, Reloc RelocMode,
	CodeModel CodeModel) (tm TargetMachine) {
	cTriple := C.CString(Triple)
	defer C.free(unsafe.Pointer(cTriple))
	cCPU := C.CString(CPU)
	defer C.free(unsafe.Pointer(cCPU))
	cFeatures := C.CString(Features)
	defer C.free(unsafe.Pointer(cFeatures))
	tm.C = C.TangoLLVMCreateTargetMachine(t.C, cTriple, cCPU, cFeatures,
		C.LLVMCodeGenOptLevel(Level),
		C.LLVMRelocMode(Reloc),
		C.LLVMCodeModel(CodeModel))
	return
}

// CreateTargetData returns a new TargetData describing the TargetMachine's
// data layout. The returned TargetData is owned by the caller, who is
// responsible for disposing of it by calling the TargetData.Dispose method.
func (tm TargetMachine) CreateTargetData() TargetData {
	return TargetData{C.TangoLLVMCreateTargetDataLayout(tm.C)}
}

// Dispose releases resources related to the TargetMachine.
func (tm TargetMachine) Dispose() {
	C.TangoLLVMDisposeTargetMachine(tm.C)
}
