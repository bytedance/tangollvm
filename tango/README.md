# TangoLLVM source and SDK integration

[Tango](https://github.com/bytedance/tango) maintains the Go-to-LLVM IR translator
(`src/cmd/compile/internal/ssagen/llvm.go`) and its tests. Parts of that
translator were adapted from TinyGo; its source and license are retained in
Tango. Tango also owns compiler/linker dispatch, experiment isolation, Go object
patching, SDK lookup and disabled-backend stubs. The translator follows Go's
SSA, types and ABI interfaces. Its build
constraints exclude it from normal builds, which need no LLVM headers, libraries
or tools.

This repository owns the LLVM backend, bindings, generated C bridge and their
licenses. The copy manifest lists the bindings, their notices and license, and a binding test:
15 files in total. The translator, its test, and runtime Duff assembly are
copied from Tango, never overlaid.

## Build

Requirements: native Linux/amd64, Git, Bash 4.3 or newer, GNU coreutils, GNU tar,
findutils, a C/C++ compiler, CMake, Ninja, and a bootstrap Go toolchain compatible
with Tango.

```sh
# In the LLVM repository: build the customized SDK, including generated headers.
./build.sh

# Use absolute paths. BUILD_ROOT must be new and outside both repositories.
TANGO_ROOT=/path/to/tango \
TANGOLLVM_BUILD_ROOT=/path/to/tango-llvm-build \
GOROOT_BOOTSTRAP=/path/to/bootstrap-go \
./tango/build.sh
```

The convenience entry point in Tango delegates to the same build implementation:

```sh
TANGOLLVM_REPO=/path/to/llvm \
TANGOLLVM_BUILD_ROOT=/path/to/tango-llvm-build \
GOROOT_BOOTSTRAP=/path/to/bootstrap-go \
/path/to/tango/src/make.tangollvm.bash
```

Preparation copies tracked files from the Tango working tree to a new build
snapshot, checks overlay destinations, and copies the LLVM-owned files
into it. Existing files at those destinations are rejected. Neither source
checkout is modified. Untracked Tango files are not build inputs; commit new
source files first. Unrelated tracked edits are copied from the working tree.

The SDK is copied to `BUILD_ROOT/pkg/tangollvm`, including generated headers,
tools, libraries, and the LLVM, gollvm and Go license texts. Existing output directories are rejected,
not deleted or reused. Use a fresh output directory for each build. This also
avoids stale objects when runtime layouts or LLVM APIs change.

The SDK must include executable `opt`, `llc` and `llvm-link` tools. Its symlinks
must be relative, resolve inside the SDK and have existing targets, so the copy
can be moved independently. Preparation validates both the installed SDK and
its copy. Source paths cannot escape the Tango checkout or the LLVM overlay.

`tangollvm-source.json` records both Git revisions, copied Tango input hashes,
overlay hashes, and selected SDK file hashes. Git revisions alone do not describe uncommitted changes;
retain this manifest and the generated snapshot with local validation results.
The manifest hash describes the bytes parsed for that preparation; overlay
hashes describe the copied files even if their originals are edited meanwhile.
When changing a release compiler or an external SDK, use a fresh `GOCACHE`: Go
release tool version strings do not identify local rebuilds or SDK changes.
The source-only mode prepares a reviewable snapshot without requiring an SDK:

```sh
./tango/prepare.sh --tango /path/to/tango \
  --output /path/to/source-review --source-only
```

Source-only mode does not install an SDK or build the toolchain.

## Use and test

Run application commands from the application's Go module directory. Select a
fresh cache when switching to a rebuilt toolchain or SDK:

```sh
cd /path/to/application
export GOCACHE="$(mktemp -d)"
env -u GOROOT GOEXPERIMENT=tangollvm \
  /path/to/tango-llvm-build/bin/go build \
  -gcflags='all=-beast=llvm=3,optimize=3,inline=1' ./...
```

The SDK defaults to `GOROOT/pkg/tangollvm`. `TANGOLLVM_SDK_ROOT` can select
another SDK at application compilation time. The LLVM compiler tools load LLVM;
the generated Go applications do not require LLVM at runtime.

The generated toolchain also retains the standard compiler and linker; select
`GOEXPERIMENT=notangollvm` to use them. To build plain Tango, run its regular
`src/make.bash` without the LLVM environment variables. Enabling the LLVM
experiment requires the prepared sources and separate LLVM tools.

```sh
# Requires a C compiler (cc) for the conflicting-header regression.
/path/to/llvm/tango/test_prepare.sh
cd /path/to/tango-llvm-build
env -u GOROOT ./test/llvm/test.sh 3 3 1
env -u GOROOT GOEXPERIMENT=notangollvm ./bin/go test cmd/dist
env -u GOROOT GOEXPERIMENT=notangollvm ./bin/go test cmd/internal/llvm \
  cmd/compile/internal/ssagen
env -u GOROOT GOEXPERIMENT=tangollvm ./bin/go test cmd/internal/llvm \
  cmd/compile/internal/ssagen cmd/dist
/path/to/llvm/tango/test_lto.sh --go /path/to/tango-llvm-build/bin/go
```

The bindings locate headers in the copied SDK relative to their source directory,
so direct package tests do not need the bootstrap process's environment. The
bootstrap entry supplies its explicit header path for its intermediate trees.

For full LTO, use a fresh compilation of all packages so their exported LLVM
bitcode is present in the same temporary work directory:

```sh
cd /path/to/application
env -u GOROOT GOEXPERIMENT=tangollvm \
  /path/to/tango-llvm-build/bin/go build -a \
  -gcflags='all=-beast=llvm=3,optimize=3,inline=1,full-lto=1' ./...
```

The `-a` flag is required for this work-directory-based LTO pipeline. Cached Go
archives alone do not contain its exported bitcode side files. LTO stages stop
at the first failure and retain the original tool error.
Each link selects bitcode from its own main archive and resolved dependencies,
and uses a separate intermediate directory. Multiple executable targets in one
`go build ./...` invocation do not merge each other's `main` packages. The
integration test above builds and runs two targets in one invocation.

## Maintain the boundary

`overlay/` stores complete source files that are copied only when LLVM is
requested. Its files are never committed back into Tango. Keep the self-developed translator,
flow and Go object handling in Tango. LLVM-derived bindings, the C bridge and
LLVM backend implementation belong here. Calling LLVM APIs does not by itself
make a Go compiler source file LLVM-owned.

`manifest.tsv` lists the overlay destinations and their expected original
state. It does not pin Tango-owned files to a particular branch or content hash.
Different Tango branches can be prepared without updating interface hashes;
validate compiler and ABI compatibility by building and running the regression
tests for each branch. Preparation alone does not establish compatibility.
The optional source revision records are historical references, not branch
requirements. Actual input revisions and hashes remain recorded in
`tangollvm-source.json` for traceability.

The initial source is Tango integration `2aa05be13a`. Bindings deleted in that
commit were recovered from its parent, constrained by `goexperiment.tangollvm`,
and adapted to SDK-relative tool lookup. The Tango translator is adapted to the current
`go.preallocated` signature, using explicit caller-frame addresses so
optimizers do not treat caller-owned return slots as unobservable local storage. Tango owns the amd64 Duff compatibility assembly used by this backend.
Regenerate it in Tango's `src/runtime` directory with `go run mkduff.go`.
The generated `duff_amd64.s` is guarded by `goexperiment.tangollvm` and remains
under the Tango tree's Go BSD license. There is no separate Duff generator or
runtime overlay in this repository.

LLVM-derived Go bindings include extensions from TinyGo's
[go-llvm](https://github.com/tinygo-org/go-llvm) and retain the LLVM
Apache-2.0 WITH LLVM-exception license.
ByteDance-owned LLVM adapter code uses the same license, regardless of its
`.go`, `.c`, or `.h` extension. Copying these files into Tango does not change
their license.

`GoGCPrinter.cpp` includes code adapted from
[gollvm's `passes/GC.cpp`](https://go.googlesource.com/gollvm/+/605d1b6368b72e7bc15f66fac1f33f754537a090/passes/GC.cpp).
`GoStatepoints.cpp` is adapted from gollvm's `passes/GoStatepoints.cpp` and
retains its historical NCSA notice. The pointer traversal code in
`GoAuxDataHandler.cpp`, `GoStatepoints.cpp`, and `TangoLLVM.h::hasPointerTy`
is adapted from [gollvm's `passes/Util.cpp`](https://go.googlesource.com/gollvm/+/605d1b6368b72e7bc15f66fac1f33f754537a090/passes/Util.cpp).
Those files retain The Go Authors' copyright notice and require the BSD
3-Clause terms in `third-party/gollvm/LICENSE`.

`GoGCProgWriter.h` and `GoGCProgWriter.cpp` contain portions adapted from Go's
`src/cmd/internal/gcprog/gcprog.go`, covered by the BSD 3-Clause license in
`third-party/go/LICENSE`. The Go stack-splitting code in `X86FrameLowering.cpp`
is adapted from `src/cmd/internal/obj/x86/obj6.go`, covered by the MIT license
and original copyright notices in `third-party/go/LICENSE-MIT`.

The project grant is in `LICENSE.TXT`; the complete original LLVM license
is retained in `LICENSE_UPSTREAM.TXT`. The root `NOTICE` identifies the sources
and licenses of the mixed files; the overlay directory has its own `NOTICE`
listing the derived bindings.

Preparation copies the project and upstream licenses, `NOTICE`,
`LICENSE.gollvm`, `LICENSE.go` and `LICENSE.go-MIT` into the SDK. Retain them when
distributing the SDK or LLVM binaries.

The TangoLLVM preparation, build, and regression scripts use Bash and need no
Python or jq. Upstream LLVM test tools retain their own dependencies.

The tab-separated manifest starts with `version<TAB>1`, optional source revision
records, then `overlay<TAB>path<TAB>base_sha256` records. A base hash of `-` requires the overlay
destination to be absent. Paths must be normalized and relative; manifest paths
cannot contain tabs or newlines. This file is data and is never sourced as shell
code. Provenance remains JSON in `tangollvm-source.json`.
