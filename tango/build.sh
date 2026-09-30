#!/usr/bin/env bash
# Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 ByteDance Ltd. and/or its affiliates.

set -euo pipefail
repo=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
: "${TANGO_ROOT:?Set TANGO_ROOT to the Tango source checkout}"
: "${TANGOLLVM_BUILD_ROOT:?Set TANGOLLVM_BUILD_ROOT to a new directory outside both repositories}"
if [[ $(uname -s) != Linux || $(uname -m) != x86_64 || ${GOOS:-linux} != linux || ${GOARCH:-amd64} != amd64 ]]; then
    echo 'TangoLLVM requires a native Linux/amd64 build.' >&2
    exit 1
fi
if [[ ${CGO_ENABLED:-1} != 1 ]]; then
    echo 'TangoLLVM requires CGO_ENABLED=1.' >&2
    exit 1
fi
"$repo/tango/prepare.sh" --tango "$TANGO_ROOT" --output "$TANGOLLVM_BUILD_ROOT"
build_root=$(cd "$TANGOLLVM_BUILD_ROOT" && pwd)
# Prefer paired SDK headers over pre-existing user include directories.
export TANGOLLVM_SDK_ROOT="$build_root/pkg/tangollvm"
export CGO_CPPFLAGS="\"-I$TANGOLLVM_SDK_ROOT/include\"${CGO_CPPFLAGS:+ $CGO_CPPFLAGS}"
export CGO_ENABLED=1 BUILD_TANGOLLVM=1 BUILDING_TANGOLLVM_TOOLS=1
# Append last so a preceding notangollvm does not silently disable this build.
export GOEXPERIMENT="${GOEXPERIMENT:+$GOEXPERIMENT,}tangollvm"
unset GOROOT
cd "$build_root/src"
exec ./make.bash "$@"
