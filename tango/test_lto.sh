#!/usr/bin/env bash
# Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 ByteDance Ltd. and/or its affiliates.

# Build and run two independent full-LTO executables in one Go invocation.
set -euo pipefail
if [[ $# != 2 || $1 != --go ]]; then
    echo "Usage: $0 --go /path/to/prepared/bin/go" >&2
    exit 2
fi
go=$(realpath -- "$2")
[[ -x "$go" ]] || { echo "Not an executable Go tool: $go" >&2; exit 1; }
export GOENV=off GOFLAGS= GOTOOLCHAIN=local GOEXPERIMENT=tangollvm
unset GOROOT
if [[ -n ${TANGOLLVM_SDK_ROOT:-} ]]; then
    TANGOLLVM_SDK_ROOT=$(realpath -- "$TANGOLLVM_SDK_ROOT")
    export TANGOLLVM_SDK_ROOT
fi
root=$(mktemp -d)
trap 'rm -rf -- "$root"' EXIT
root=$(realpath -- "$root")
export GOCACHE="$root/cache"
for name in one two; do
    mkdir -p "$root/project/cmd/$name"
    printf 'package main\nimport "fmt"\nfunc main() { fmt.Println("%s") }\n' "$name" > "$root/project/cmd/$name/main.go"
done
printf 'module test.example/multi\n\ngo 1.26\n' > "$root/project/go.mod"
cd "$root/project"
"$go" build -a -p 8 \
    '-gcflags=all=-beast=llvm=3,optimize=3,inline=1,full-lto=1' \
    -o "$root/bin/" ./...
for name in one two; do
    output=$("$root/bin/$name")
    [[ "$output" == "$name" ]] || { echo "$name: unexpected output: $output" >&2; exit 1; }
    echo "PASS: $name builds and runs independently"
done
