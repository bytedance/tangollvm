//===- llvm_dep.go - creates LLVM dependency ------------------------------===//
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
// This file ensures that the LLVM libraries are built before using the
// bindings.
//
//===----------------------------------------------------------------------===//

//go:build goexperiment.tangollvm && !byollvm && linux && amd64 && cgo && !compiler_bootstrap

package llvm

var _ run_build_sh
