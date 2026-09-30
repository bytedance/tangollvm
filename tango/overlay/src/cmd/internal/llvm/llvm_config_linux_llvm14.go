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

//go:build goexperiment.tangollvm && !byollvm && linux && amd64 && cgo && !compiler_bootstrap

package llvm

// // Note: disabling deprecated warnings as a transition to new function
// // signatures. We should fix this in LLVM 15 or so, when support for
// // LLVM 13 is dropped and the old function signatures aren't supported
// // anymore.
// // Examples are: LLVMConstGEP2, LLVMAddAlias2, ...
// #cgo CFLAGS: -Wno-deprecated-declarations
//
// // Locate the copied SDK when testing or rebuilding bindings directly.
// // The bootstrap entry also supplies its explicit SDK include directory.
// #cgo CPPFLAGS: "-I${SRCDIR}/../../../../pkg/tangollvm/include"
// #cgo CPPFLAGS: -D_GNU_SOURCE -D__STDC_CONSTANT_MACROS -D__STDC_FORMAT_MACROS -D__STDC_LIMIT_MACROS
// #cgo CXXFLAGS: -std=c++17
// #cgo LDFLAGS: -ldl
import "C"

type run_build_sh int
