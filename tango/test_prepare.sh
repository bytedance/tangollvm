#!/usr/bin/env bash
# Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 ByteDance Ltd. and/or its affiliates.

# Run the actual shell entry point against small, isolated Git/SDK fixtures.
set -euo pipefail
script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
tmp=$(mktemp -d)
trap 'rm -rf -- "$tmp"' EXIT
count=0
real_cp=$(command -v cp)
real_mkdir=$(command -v mkdir)
fail() { echo "FAIL: $*" >&2; exit 1; }
sha() { sha256sum < "$1" | cut -c1-64; }
setup() {
    count=$((count+1))
    root="$tmp/case $count"
    go="$root/go"; repo="$root/llvm"; out="$root/output with spaces"
    mkdir -p "$go/src" "$repo/tango/overlay/src"
    cp "$script_dir/prepare.sh" "$repo/tango/prepare.sh"
    printf 'original\n' > "$go/src/shared.go"
    printf 'overlay\n' > "$repo/tango/overlay/src/shared.go"
    printf 'new\n' > "$repo/tango/overlay/src/new.go"
    expected=$(sha "$go/src/shared.go")
    printf 'version\t1\noverlay\tsrc/shared.go\t%s\noverlay\tsrc/new.go\t-\n' "$expected" > "$repo/tango/manifest.tsv"
    git -C "$go" init -q
    git -C "$go" add .
    git -C "$go" -c user.name=Test -c user.email=test@example.com commit -qm base
    git -C "$repo" init -q
    git -C "$repo" -c user.name=Test -c user.email=test@example.com commit --allow-empty -qm base
}
run() { "$repo/tango/prepare.sh" --tango "$go" --output "$out" "$@" > "$root/log" 2>&1; }
reject() {
    local pattern=$1; shift
    if run "$@"; then fail "unexpected success: $pattern"; fi
    grep -qF "$pattern" "$root/log" || { cat "$root/log"; fail "missing diagnostic: $pattern"; }
    [[ ! -e "$out" ]] || fail 'failed preparation published output'
    [[ -z $(find "$root" -maxdepth 1 -name '.tangollvm-*' -print) ]] || fail 'leaked staging directory'
}
sdk() {
    mkdir -p "$repo/llvm-bin-release/"{bin,lib,include/llvm-c}
    local name
    for name in opt llc llvm-link; do
        printf '#!/bin/sh\nexit 0\n' > "$repo/llvm-bin-release/bin/$name"
        chmod +x "$repo/llvm-bin-release/bin/$name"
    done
    printf library > "$repo/llvm-bin-release/lib/libLLVM.so.19.1"
    ln -s libLLVM.so.19.1 "$repo/llvm-bin-release/lib/libLLVM.so"
    printf header > "$repo/llvm-bin-release/include/llvm-c/TangoLLVM.h"
    printf license > "$repo/LICENSE.TXT"
    printf 'upstream LLVM license' > "$repo/LICENSE_UPSTREAM.TXT"
    printf 'source notices' > "$repo/NOTICE"
    mkdir -p "$repo/third-party/gollvm"
    printf 'gollvm license' > "$repo/third-party/gollvm/LICENSE"
    mkdir -p "$repo/third-party/go"
    printf 'Go BSD license' > "$repo/third-party/go/LICENSE"
    printf 'Go MIT license' > "$repo/third-party/go/LICENSE-MIT"
}
setup; run --source-only
[[ $(cat "$go/src/shared.go") == original && $(cat "$out/src/shared.go") == overlay && -f "$out/src/new.go" && ! -e "$out/.git" ]] || fail copy
record="$out/tangollvm-source.json"
grep -qF '"source_only":true' "$record"
grep -qF "\"src/shared.go\":\"$expected\"" "$record"
grep -qF "\"manifest_sha256\":\"$(sha "$repo/tango/manifest.tsv")\"" "$record"

setup; printf changed > "$go/src/shared.go"; reject 'incompatible Tango source' --source-only
setup; printf collision > "$go/src/new.go"; reject 'incompatible Tango source' --source-only
setup; sdk; run
[[ -L "$out/pkg/tangollvm/lib/libLLVM.so" && -f "$out/pkg/tangollvm/LICENSE.TXT" && -f "$out/pkg/tangollvm/LICENSE.gollvm" ]] || fail sdk
cmp "$repo/LICENSE.TXT" "$out/pkg/tangollvm/LICENSE.TXT"
cmp "$repo/LICENSE_UPSTREAM.TXT" "$out/pkg/tangollvm/LICENSE_UPSTREAM.TXT"
cmp "$repo/NOTICE" "$out/pkg/tangollvm/NOTICE"
cmp "$repo/third-party/go/LICENSE" "$out/pkg/tangollvm/LICENSE.go"
cmp "$repo/third-party/go/LICENSE-MIT" "$out/pkg/tangollvm/LICENSE.go-MIT"
 grep -qF "\"lib/libLLVM.so\":\"$(sha "$out/pkg/tangollvm/lib/libLLVM.so")\"" "$out/tangollvm-source.json"
setup; sdk; rm "$repo/llvm-bin-release/bin/llvm-link"; reject 'incomplete LLVM SDK'
setup; sdk; chmod -x "$repo/llvm-bin-release/bin/llvm-link"; reject 'not executable'
setup; reject 'incomplete LLVM SDK'
for link in /tmp missing ../../../outside; do
    setup; sdk; ln -s "$link" "$repo/llvm-bin-release/lib/extra"; reject 'SDK symlink'
done
setup; mv "$repo/tango/overlay/src/new.go" "$root/external"; ln -s "$root/external" "$repo/tango/overlay/src/new.go"; reject 'unsupported overlay source' --source-only
setup; mv "$repo/tango/overlay" "$root/external"; ln -s "$root/external" "$repo/tango/overlay"; reject 'unsupported overlay source' --source-only
setup; mv "$go/src/shared.go" "$root/external"; ln -s "$root/external" "$go/src/shared.go"; reject 'regular source' --source-only
setup; ln -s absent "$go/dangling"; git -C "$go" add dangling; reject 'unsupported source file' --source-only
setup; mkdir "$go/extra"; touch "$go/extra/tracked"; git -C "$go" add extra; mv "$go/extra" "$root/external"; ln -s "$root/external" "$go/extra"; reject 'source path escapes' --source-only
setup; mkdir "$out"; echo keep > "$out/keep"; if run --source-only; then fail 'accepted existing output'; fi; [[ $(cat "$out/keep") == keep ]]
setup; out="$go/build"; reject 'outside' --source-only
setup; out="$repo/build"; reject 'outside' --source-only
for path in ../escape /absolute src/shared.go src/./bad; do
    setup; printf 'overlay\t%s\t-\n' "$path" >> "$repo/tango/manifest.tsv"; reject 'invalid or duplicate' --source-only
done
# Different Tango branches may change files outside the overlay destinations.
setup
printf 'version\t1\noverlay\tsrc/new.go\t-\n' > "$repo/tango/manifest.tsv"
git -C "$go" checkout -qb another-tango-branch
printf 'branch-specific interface\n' > "$go/src/shared.go"
git -C "$go" add src/shared.go
git -C "$go" -c user.name=Test -c user.email=test@example.com commit -qm branch
run --source-only
cmp "$go/src/shared.go" "$out/src/shared.go"
grep -qF "\"tango_revision\":\"$(git -C "$go" rev-parse HEAD)\"" "$out/tangollvm-source.json"
grep -qF "\"src/shared.go\":\"$(sha "$go/src/shared.go")\"" "$out/tangollvm-source.json"
setup; printf 'version\t2\n' > "$repo/tango/manifest.tsv"; reject 'unsupported manifest version' --source-only
setup; printf 'overlay\tsrc/x\t-\textra\n' >> "$repo/tango/manifest.tsv"; reject 'invalid manifest columns' --source-only
# The build/prepare path must never invoke Python or jq.
setup; mkdir "$root/no-python"
for name in python python3 jq; do printf '#!/bin/sh\nexit 99\n' > "$root/no-python/$name"; chmod +x "$root/no-python/$name"; done
PATH="$root/no-python:$PATH" run --source-only
# Git filenames with quotes, backslashes and newlines remain valid JSON keys.
setup; special=$'quoted"back\\line\nfile'; printf special > "$go/$special"; git -C "$go" add .; run --source-only
grep -qF 'quoted\"back\\line\u000afile' "$out/tangollvm-source.json"
# Edits after the overlay copy must not alter recorded hashes or manifest hash.
setup; mkdir "$root/hooks"; manifest_hash=$(sha "$repo/tango/manifest.tsv")
cat > "$root/hooks/cp" <<EOF
#!/bin/bash
"$real_cp" "\$@" || exit
if [[ "\$*" == *'/overlay/src/new.go'* ]]; then
    echo changed > "$repo/tango/overlay/src/new.go"
    echo bad > "$repo/tango/manifest.tsv"
fi
EOF
chmod +x "$root/hooks/cp"; PATH="$root/hooks:$PATH" run --source-only
grep -qF "\"manifest_sha256\":\"$manifest_hash\"" "$out/tangollvm-source.json"
grep -qF "\"src/new.go\":\"$(sha "$out/src/new.go")\"" "$out/tangollvm-source.json"
# Validate again after SDK copying, before publishing.
setup; sdk; mkdir "$root/hooks"
cat > "$root/hooks/cp" <<EOF
#!/bin/bash
"$real_cp" "\$@" || exit
if [[ "\$*" == *'/llvm-bin-release/.'* ]]; then
    dest="\${@: -1}"
    ln -s /tmp "\$dest/external"
fi
EOF
chmod +x "$root/hooks/cp"; PATH="$root/hooks:$PATH" reject 'SDK symlink'
# A concurrent creator owns its directory; mkdir must not overwrite it.
setup; mkdir "$root/hooks"
cat > "$root/hooks/mkdir" <<EOF
#!/bin/bash
if [[ "\$*" == '-- $out' ]]; then
    "$real_mkdir" -- "$out"
    echo keep > "$out/keep"
fi
exec "$real_mkdir" "\$@"
EOF
chmod +x "$root/hooks/mkdir"
if PATH="$root/hooks:$PATH" run --source-only; then fail 'replaced concurrent output'; fi
[[ $(cat "$out/keep") == keep ]] || fail 'removed concurrent output'
# Recheck overlay bases after the source copy.
real_tar=$(command -v tar)
setup; mkdir "$root/hooks"
cat > "$root/hooks/tar" <<EOF
#!/bin/bash
"$real_tar" "\$@" || exit
if [[ "\$*" == *'-xpf'* ]]; then echo changed > "\$2/src/shared.go"; fi
EOF
chmod +x "$root/hooks/tar"
PATH="$root/hooks:$PATH" reject 'source changed while copying' --source-only
# Exercise build.sh -> prepare.sh -> make.bash with Python and jq unavailable.
# Use cc to verify header selection, including a conflicting user include path.
setup; sdk
mkdir -p "$root/foreign include/llvm-c"
printf '#error selected foreign TangoLLVM header\n' > "$root/foreign include/llvm-c/TangoLLVM.h"
printf '#define SELECTED_SDK_HEADER 1\n' > "$repo/llvm-bin-release/include/llvm-c/TangoLLVM.h"
cp "$script_dir/build.sh" "$repo/tango/build.sh"
cat > "$go/src/make.bash" <<'EOF'
#!/bin/bash
set -eu
[[ "$BUILD_TANGOLLVM" == 1 && "$BUILDING_TANGOLLVM_TOOLS" == 1 ]]
[[ "$GOEXPERIMENT" == *tangollvm && -f "$TANGOLLVM_SDK_ROOT/include/llvm-c/TangoLLVM.h" ]]
[[ -z "${GOROOT:-}" ]]
# These flags contain only the controlled fixture paths and PRESERVED_FLAG.
# Interpret their quotes as cgo does, then run a real C preprocessor.
eval "set -- $CGO_CPPFLAGS"
cc "$@" -E -P -x c - <<'SOURCE'
#include <llvm-c/TangoLLVM.h>
#if !SELECTED_SDK_HEADER || PRESERVED_FLAG != 42
#error missing SDK header or user flags
#endif
SOURCE
printf 'build entry passed\n'
EOF
chmod +x "$go/src/make.bash"; git -C "$go" add .
mkdir "$root/no-python"
for name in python python3 jq; do printf '#!/bin/sh\nexit 99\n' > "$root/no-python/$name"; chmod +x "$root/no-python/$name"; done
PATH="$root/no-python:$PATH" TANGO_ROOT="$go" TANGOLLVM_BUILD_ROOT="$out" \
    GOOS=linux GOARCH=amd64 CGO_ENABLED=1 GOROOT=ignored GOEXPERIMENT=notangollvm \
    CGO_CPPFLAGS="\"-I$root/foreign include\" -DPRESERVED_FLAG=42" \
    "$repo/tango/build.sh" > "$root/log" 2>&1 || { cat "$root/log"; fail "build entry/header selection"; }
grep -qF 'build entry passed' "$root/log"
echo "PASS: $count shell preparation cases"
