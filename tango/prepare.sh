#!/usr/bin/env bash
# Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Copyright (c) 2026 ByteDance Ltd. and/or its affiliates.

# Prepare a separate Tango tree using Bash and standard Linux tools.
set -euo pipefail
export LC_ALL=C
repo=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)
tango= output= staging= scratch=
source_only=false
fail() { echo "TangoLLVM preparation failed: $*" >&2; exit 1; }
cleanup() {
    [[ -z "$staging" ]] || rm -rf -- "$staging"
    [[ -z "$scratch" ]] || rm -rf -- "$scratch"
    return 0
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
while (( $# )); do
    case "$1" in
        --tango|--output)
            (( $# >= 2 )) || fail "missing value for $1"
            case "$1" in --tango) tango=$2;; --output) output=$2;; esac
            shift 2 ;;
        --source-only) source_only=true; shift ;;
        --help|-h)
            echo 'Usage: prepare.sh --tango TANGO --output NEW_DIRECTORY [--source-only]'
            exit 0 ;;
        *) fail "unknown argument: $1" ;;
    esac
done
[[ -n "$tango" && -n "$output" ]] || fail '--tango and --output are required'
[[ ! -e "$output" && ! -L "$output" ]] || fail "output already exists: $output"
tango=$(realpath -e -- "$tango")
output=$(realpath -m -- "$output")
case "$output/" in "$tango/"*|"$repo/"*) fail 'output must be outside both source repositories';; esac
[[ -d "$tango" ]] || fail "missing Tango directory: $tango"
# Snapshot the manifest before parsing so its digest identifies the data used.
scratch=$(mktemp -d)
cp -- "$repo/tango/manifest.tsv" "$scratch/manifest.tsv"
digest() {
    if [[ -f "$1" ]]; then
        local sum
        sum=$(sha256sum < "$1")
        REPLY=${sum:0:64}
    else
        REPLY=-
    fi
}
digest "$scratch/manifest.tsv"; manifest_hash=$REPLY
valid_path() {
    case "$1" in ''|/*|*/|*//*|.|..|./*|../*|*/./*|*/../*|*/.|*/..) return 1;; esac
}
inside() {
    local resolved
    resolved=$(realpath -m -- "$1")
    [[ "$resolved" == "$2/"* ]]
}
declare -a overlays=()
declare -A overlay_hash=()
version= tango_base= integration_source=
while IFS=$'\t' read -r kind name hash extra || [[ -n "$kind" ]]; do
    [[ -z "$extra" ]] || fail 'invalid manifest columns'
    case "$kind" in
        version)
            [[ -z "$version" && "$name" == 1 && -z "$hash" ]] || fail 'unsupported manifest version'
            version=$name ;;
        tango_base|integration_source)
            [[ "$name" =~ ^[0-9a-f]{40}$ && -z "$hash" ]] || fail "invalid $kind"
            [[ -z "${!kind}" ]] || fail "duplicate $kind"
            printf -v "$kind" '%s' "$name" ;;
        overlay)
            valid_path "$name" || fail "invalid or duplicate manifest path: $name"
            [[ "$hash" =~ ^[0-9a-f]{64}$ || "$hash" == - ]] || fail "invalid overlay hash: $name"
            [[ ! -v 'overlay_hash[$name]' ]] || fail "invalid or duplicate overlay path: $name"
            overlays+=("$name"); overlay_hash["$name"]=$hash ;;
        *) fail "invalid manifest record: $kind" ;;
    esac
done < "$scratch/manifest.tsv"
[[ "$version" == 1 ]] || fail 'missing manifest version'
for name in "${overlays[@]}"; do
    src="$tango/$name"
    [[ ! -L "$src" && ( ! -e "$src" || -f "$src" ) ]] || fail "not a regular source file: $name"
    inside "$src" "$tango" || fail "source path escapes the Tango checkout: $name"
    digest "$src"
    [[ "$REPLY" == "${overlay_hash[$name]}" ]] || fail "incompatible Tango source: $name"
    src="$repo/tango/overlay/$name"
    [[ ! -L "$src" ]] && inside "$src" "$repo/tango/overlay" || fail "unsupported overlay source: $name"
    [[ -f "$src" ]] || fail "missing overlay source: $name"
done
sdk_files=(bin/opt bin/llc bin/llvm-link lib/libLLVM.so include/llvm-c/TangoLLVM.h)
validate_sdk() {
    local sdk=$1 name link target root
    for name in "${sdk_files[@]}"; do
        [[ -f "$sdk/$name" ]] || fail "incomplete LLVM SDK: $sdk/$name; run build.sh first"
    done
    for name in opt llc llvm-link; do
        [[ -x "$sdk/bin/$name" ]] || fail "LLVM tool is not executable: $name"
    done
    root=$(realpath -e -- "$sdk")
    find "$sdk" -type l -print0 > "$scratch/sdk-links"
    while IFS= read -r -d '' link; do
        target=$(readlink -- "$link")
        [[ "$target" != /* && -e "$link" ]] && inside "$link" "$root" || fail "non-relocatable LLVM SDK symlink: $link"
    done < "$scratch/sdk-links"
}
sdk="$repo/llvm-bin-release"
$source_only || validate_sdk "$sdk"
git -C "$tango" ls-files -z > "$scratch/tracked"
: > "$scratch/files"
declare -A checked_dirs=()
while IFS= read -r -d '' name; do
    valid_path "$name" || fail "invalid tracked path: $name"
    src="$tango/$name"
    [[ ! -L "$src" ]] || fail "unsupported source file: $name"
    dir=${name%/*}; [[ "$dir" != "$name" ]] || dir=.
    if [[ ! -v 'checked_dirs[$dir]' ]]; then
        resolved=$(realpath -m -- "$tango/$dir")
        [[ "$resolved" == "$tango" || "$resolved" == "$tango/"* ]] || fail "source path escapes the Tango checkout: $name"
        checked_dirs["$dir"]=1
    fi
    # Deleted tracked files are intentionally absent in the input snapshot.
    [[ -e "$src" ]] || continue
    [[ -f "$src" ]] || fail "unsupported source file: $name"
    printf '%s\0' "$name" >> "$scratch/files"
done < "$scratch/tracked"
mkdir -p -- "${output%/*}"
staging=$(mktemp -d "${output%/*}/.tangollvm-XXXXXXXX")
tar -C "$tango" --null --verbatim-files-from -T "$scratch/files" -cf - | tar -C "$staging" -xpf -
# Hash the copied inputs before applying the overlay, in batches rather than
# spawning a checksum process for each file in the Go source tree.
(cd "$staging" && xargs -0 -r sha256sum --zero -- < "$scratch/files") > "$scratch/input-hashes"
for name in "${overlays[@]}"; do
    digest "$staging/$name"
    [[ "$REPLY" == "${overlay_hash[$name]}" ]] || fail "Tango source changed while copying: $name"
    mkdir -p -- "$(dirname "$staging/$name")"
    cp -p -- "$repo/tango/overlay/$name" "$staging/$name"
done
if ! $source_only; then
    dest="$staging/pkg/tangollvm"
    [[ ! -e "$dest" && ! -L "$dest" ]] || fail 'SDK destination already exists in Tango inputs'
    mkdir -p -- "${dest%/*}"
    mkdir -- "$dest"
    cp -a -- "$sdk/." "$dest/"
    cp -p -- "$repo/LICENSE.TXT" "$dest/LICENSE.TXT"
    cp -p -- "$repo/LICENSE_UPSTREAM.TXT" "$dest/LICENSE_UPSTREAM.TXT"
    cp -p -- "$repo/NOTICE" "$dest/NOTICE"
    cp -p -- "$repo/third-party/gollvm/LICENSE" "$dest/LICENSE.gollvm"
    cp -p -- "$repo/third-party/go/LICENSE" "$dest/LICENSE.go"
    cp -p -- "$repo/third-party/go/LICENSE-MIT" "$dest/LICENSE.go-MIT"
    validate_sdk "$dest"
fi
# JSON output remains compatible with existing provenance readers. Escape all
# control bytes as well as quotes/backslashes, including unusual Git filenames.
json_quote() {
    local s=$1 c escaped i
    s=${s//\\/\\\\}; s=${s//\"/\\\"}
    if [[ "$s" == *[[:cntrl:]]* ]]; then
        for ((i=1; i<32; i++)); do
            printf -v c '%b' "\\$(printf '%03o' "$i")"
            printf -v escaped '\\u%04x' "$i"
            s=${s//"$c"/"$escaped"}
        done
    fi
    REPLY="\"$s\""
}
write_hashes() {
    local root=$1 name sep=
    shift
    printf '{'
    for name in "$@"; do
        json_quote "$name"; printf '%s%s:' "$sep" "$REPLY"
        digest "$root/$name"; [[ "$REPLY" != - ]] || fail "missing copied file: $name"
        printf '"%s"' "$REPLY"; sep=,
    done
    printf '}'
}
tango_revision=$(git -C "$tango" rev-parse HEAD)
llvm_revision=$(git -C "$repo" rev-parse HEAD)
{
    printf '{"tango_revision":"%s","llvm_revision":"%s","manifest_sha256":"%s",' "$tango_revision" "$llvm_revision" "$manifest_hash"
    printf '"source_only":%s,"tango_input_sha256":{' "$source_only"
    sep=
    while IFS= read -r -d '' record; do
        hash=${record:0:64}; name=${record:66}
        json_quote "$name"; printf '%s%s:"%s"' "$sep" "$REPLY" "$hash"; sep=,
    done < "$scratch/input-hashes"
    printf '},"overlay_sha256":'; write_hashes "$staging" "${overlays[@]}"
    printf ',"sdk_sha256":'
    if $source_only; then printf '{}'; else write_hashes "$staging/pkg/tangollvm" "${sdk_files[@]}"; fi
    printf '}\n'
} > "$staging/tangollvm-source.json"
# Claim the output with mkdir so even a concurrently created directory is kept.
mkdir -- "$output" || fail "output already exists: $output"
shopt -s dotglob nullglob
if ! mv -- "$staging"/* "$output/"; then
    rm -rf -- "$output"
    fail 'could not publish prepared tree'
fi
printf '%s\n' "$output"
