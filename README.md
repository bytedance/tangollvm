<div align="center">

# TangoLLVM

[中文版](README_CN.md)

An LLVM distribution enhanced for the Tango Go toolchain.

</div>

## Introduction

TangoLLVM is a fork of the upstream [LLVM project](https://llvm.org/) with additional changes to better support compiling Go code with LLVM.

It is primarily used by **[Tango](https://github.com/bytedance/tango)**, ByteDance’s enhanced distribution of Go (a superset of the community‑standard Go SDK). In Tango, the LLVM backend is an opt‑in compile‑time backend (enabled on the Go side) and does **not** introduce a runtime dependency on LLVM for the produced binaries.

This repository focuses on the LLVM side of that integration. In addition to the upstream LLVM monorepo contents, it contains Go‑oriented enhancements such as:

- Go ABIInternal calling convention and related CodeGen support.
- Go runtime call patterns (e.g. stack growth / morestack, write barrier helpers).
- Go GC / statepoint related utilities and passes.
- A small C API surface (see `llvm/include/llvm-c/TangoLLVM.h`) used by the Go frontend.

At a high level, the end‑to‑end TangoLLVM pipeline is:

- A frontend (in the Tango Go compiler) lowers Go Generic SSA to LLVM IR.
- TangoLLVM compiles LLVM IR to an ELF/MachO relocatable object while honoring Go’s ABIInternal conventions.
- A patch/merge step combines the result with the rest of the Go toolchain outputs.

Future work: this repository is expected to add Go Bolt related support.

If you are looking for the full Tango toolchain and end‑to‑end usage, please refer to [Tango’s repository and documentation](https://github.com/bytedance/tango).

## Repository Layout

This repository follows the upstream `llvm-project` monorepo layout (e.g. `llvm/`, `clang/`, `lld/`, `compiler-rt/`, etc.).

In addition, the repository provides scripts for building LLVM and preparing a separate TangoLLVM toolchain:

- `build.sh`: builds and installs a release LLVM build into `llvm-bin-release/`.
- `tango/prepare.sh`: checks overlay destinations and copies the bindings and complete SDK into a new build directory.
- `tango/build.sh`: prepares that directory and builds the TangoLLVM toolchain.

## Build from Source

### Prerequisites

- A C++17 toolchain (Clang or GCC)
- CMake >= 3.20
- Ninja

### Quick Build (recommended)

The repository provides a ready‑to‑use build script:

```bash
./build.sh
```

By default it:

- builds a Release configuration with assertions enabled,
- builds the `X86` target,
- installs to `./llvm-bin-release/`.

### Build the TangoLLVM toolchain

After `./build.sh` installs the LLVM SDK, run:

```bash
TANGO_ROOT=/path/to/tango \
TANGOLLVM_BUILD_ROOT=/path/to/new-tango-llvm-build \
GOROOT_BOOTSTRAP=/path/to/bootstrap-go \
./tango/build.sh
```

This integration currently supports native Linux/amd64. The output directory must
be new and outside both source repositories. Preparation uses Bash and standard
Linux tools, copies the full SDK, and leaves both source checkouts unchanged.
Python and jq are not needed by the preparation entry point.
See [tango/README.md](tango/README.md) for dependencies, application compilation,
source ownership, and validation commands.

### Custom Build

You can also build with standard LLVM CMake options:

```bash
mkdir -p build && cd build
cmake -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_ENABLE_ASSERTIONS=ON \
  -DLLVM_TARGETS_TO_BUILD="X86" \
  ../llvm
ninja
```

For more details, see LLVM’s upstream documentation:

- https://llvm.org/docs/GettingStarted.html

## License

Unless otherwise noted, ByteDance contributions use
Apache-2.0 WITH LLVM-exception; see
[LICENSE.TXT](LICENSE.TXT). TangoLLVM is based on
[LLVM 19](https://github.com/llvm/llvm-project/tree/release/19.x).
The complete original LLVM license, including historical terms, is preserved
in [LICENSE_UPSTREAM.TXT](LICENSE_UPSTREAM.TXT). Upstream and third-party code retains
its original licenses. [NOTICE](NOTICE) lists the go-llvm, gollvm and Go source
files and their license locations. SDKs retain these notices and licenses.

## Contributing

We welcome feedback and contributions, including issue reports, documentation improvements, and Go/LLVM integration patches.

Please see [CONTRIBUTING.md](CONTRIBUTING.md) for contribution guidelines.

For the original LLVM README from the upstream base of this branch, see
[README_UPSTREAM.md](README_UPSTREAM.md).
