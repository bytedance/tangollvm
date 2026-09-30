#! /bin/bash
# Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2025 ByteDance Ltd. and/or its affiliates.

set -e

DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

mkdir -p "${DIR}/llvm-build-release"

pushd "${DIR}/llvm-build-release" > /dev/null

# Keep assertions enabled when validating the customized backend.
ASSERTION=ON

cmake -G Ninja \
     -DCMAKE_BUILD_TYPE=Release \
     -DLLVM_ENABLE_ASSERTIONS=$ASSERTION \
     -DLLVM_TARGETS_TO_BUILD=X86 \
     -DLLVM_BUILD_LLVM_DYLIB=ON \
     -DLLVM_LINK_LLVM_DYLIB=ON \
     "-DCMAKE_INSTALL_PREFIX=${DIR}/llvm-bin-release" \
     ../llvm

cmake --build . -j 24

ninja install
cp -p "${DIR}/LICENSE.TXT" "${DIR}/llvm-bin-release/LICENSE.TXT"
cp -p "${DIR}/LICENSE_UPSTREAM.TXT" "${DIR}/llvm-bin-release/LICENSE_UPSTREAM.TXT"
cp -p "${DIR}/NOTICE" "${DIR}/llvm-bin-release/NOTICE"
cp -p "${DIR}/third-party/go/LICENSE" "${DIR}/llvm-bin-release/LICENSE.go"
cp -p "${DIR}/third-party/go/LICENSE-MIT" "${DIR}/llvm-bin-release/LICENSE.go-MIT"
cp -p "${DIR}/third-party/gollvm/LICENSE" "${DIR}/llvm-bin-release/LICENSE.gollvm"

popd > /dev/null
