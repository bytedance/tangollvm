//===- analysis.go - Bindings for analysis --------------------------------===//
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
// This file defines bindings for the analysis component.
//
//===----------------------------------------------------------------------===//

//go:build goexperiment.tangollvm && linux && amd64 && cgo && !compiler_bootstrap

package llvm

/*
#include "tangollvm.h"
#include <stdlib.h>
*/
import "C"
import "errors"

type VerifierFailureAction C.LLVMVerifierFailureAction

const (
	// verifier will print to stderr and abort()
	AbortProcessAction VerifierFailureAction = C.LLVMAbortProcessAction
	// verifier will print to stderr and return 1
	PrintMessageAction VerifierFailureAction = C.LLVMPrintMessageAction
	// verifier will just return 1
	ReturnStatusAction VerifierFailureAction = C.LLVMReturnStatusAction
)

// Verifies that a module is valid, taking the specified action if not.
// Optionally returns a human-readable description of any invalid constructs.
func VerifyModule(m Module, a VerifierFailureAction) error {
	var cmsg *C.char
	broken := C.TangoLLVMVerifyModule(m.C, C.LLVMVerifierFailureAction(a), &cmsg)

	// C++'s verifyModule means isModuleBroken, so it returns false if
	// there are no errors
	if broken != 0 {
		err := errors.New(C.GoString(cmsg))
		C.TangoLLVMDisposeMessage(cmsg)
		return err
	}
	return nil
}
