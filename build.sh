#! /bin/bash
# Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2025 ByteDance Ltd. and/or its affiliates.

set -e

DIR=$(pushd $(dirname $BASH_SOURCE[0]) &> /dev/null && pwd && popd &> /dev/null)

mkdir -p ${DIR}/llvm-build-release

pushd ${DIR}/llvm-build-release &> /dev/null

# Disable assertion for better compilation performance
ASSERTION=OFF

cmake -G Ninja \
     -DCMAKE_BUILD_TYPE=Release \
     -DLLVM_ENABLE_ASSERTIONS=$ASSERTION \
     -DLLVM_TARGETS_TO_BUILD=X86 \
     -DLLVM_BUILD_LLVM_DYLIB=ON \
     -DLLVM_LINK_LLVM_DYLIB=ON \
     -DCMAKE_INSTALL_PREFIX=${DIR}/llvm-bin-release \
     ../llvm

cmake --build . -j 24

ninja install

popd &> /dev/null
