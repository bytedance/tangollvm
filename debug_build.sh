#! /bin/bash
# Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2025 ByteDance Ltd. and/or its affiliates.

set -e

DIR=$(pushd $(dirname $BASH_SOURCE[0]) &> /dev/null && pwd && popd &> /dev/null)

mkdir -p ${DIR}/llvm-build-debug

pushd ${DIR}/llvm-build-debug &> /dev/null

# Disable assertion for better compilation performance
ASSERTION=ON

cmake -G Ninja \
     -DCMAKE_BUILD_TYPE=Debug \
     -DLLVM_ENABLE_STATS=ON \
     -DLLVM_ENABLE_ASSERTIONS=$ASSERTION \
     -DLLVM_TARGETS_TO_BUILD=X86 \
     -DLLVM_BUILD_LLVM_C_DYLIB=OFF \
     -DLLVM_BUILD_LLVM_DYLIB=ON \
     -DLLVM_LINK_LLVM_DYLIB=ON \
     -DCMAKE_INSTALL_PREFIX=${DIR}/llvm-bin-debug \
     ../llvm

cmake --build . -j 24

ninja install
cp -p "${DIR}/LICENSE.TXT" "${DIR}/llvm-bin-debug/LICENSE.TXT"
cp -p "${DIR}/LICENSE_UPSTREAM.TXT" "${DIR}/llvm-bin-debug/LICENSE_UPSTREAM.TXT"
cp -p "${DIR}/NOTICE" "${DIR}/llvm-bin-debug/NOTICE"
cp -p "${DIR}/third-party/gollvm/LICENSE" "${DIR}/llvm-bin-debug/LICENSE.gollvm"
cp -p "${DIR}/third-party/go/LICENSE" "${DIR}/llvm-bin-debug/LICENSE.go"
cp -p "${DIR}/third-party/go/LICENSE-MIT" "${DIR}/llvm-bin-debug/LICENSE.go-MIT"

popd &> /dev/null
