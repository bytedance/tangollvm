// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2026 ByteDance Ltd. and/or its affiliates.

//go:build goexperiment.tangollvm && linux && amd64 && cgo && !compiler_bootstrap

package llvm

/*
#include "tangollvm.h"
#include <stdlib.h>
*/
import "C"
import (
	"fmt"
	"path/filepath"
	"unsafe"
)

func errorCodeToError(err int) error {
	switch err {
	case 0:
		return nil
	case 1:
		return fmt.Errorf("dlopen cannot open the selected LLVM library")
	case 2:

		return fmt.Errorf("dlsym cannot resolve symbol TangoLLVMCreateTangoLLVMEnvironment")
	case 3, -3:
		return fmt.Errorf("cannot create TangoLLVMEnv")
	default:
		return fmt.Errorf("unknown error code: %d", err)
	}
}

func initTangoLLVMEnv(libname string) int {
	cname := C.CString(libname)
	defer C.free(unsafe.Pointer(cname))
	return int(C.InitTangoLLVMEnv(cname))
}

func InitTangoLLVMEnv(libname string) error {
	// Resolve bare library names in the selected SDK first. A system LLVM
	// library must not shadow the paired TangoLLVM API or TANGOLLVM_SDK_ROOT.
	if filepath.Base(libname) == libname {
		libname = filepath.Join(DefaultLibraryPath(), libname)
	}
	if err := errorCodeToError(initTangoLLVMEnv(libname)); err != nil {
		return fmt.Errorf("load TangoLLVM library %q: %w", libname, err)
	}
	return nil
}
