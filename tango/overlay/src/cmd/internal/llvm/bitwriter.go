//===- bitwriter.go - Bindings for bitwriter ------------------------------===//
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
// This file defines bindings for the bitwriter component.
//
//===----------------------------------------------------------------------===//

//go:build goexperiment.tangollvm && linux && amd64 && cgo && !compiler_bootstrap

package llvm

/*
#include "tangollvm.h"
#include <stdlib.h>
*/
import "C"
import "os"
import "errors"

var writeBitcodeToFileErr = errors.New("Failed to write bitcode to file")

func WriteBitcodeToFile(m Module, file *os.File) error {
	fail := C.TangoLLVMWriteBitcodeToFD(m.C, C.int(file.Fd()), C.int(0), C.int(0))
	if fail != 0 {
		return writeBitcodeToFileErr
	}
	return nil
}
