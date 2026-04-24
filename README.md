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

In addition, the repository provides a few helper scripts for building and packaging a minimal TangoLLVM distribution:

- `build.sh`: builds and installs a release LLVM build into `llvm-bin-release/`.

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

This project is under the Apache License v2.0 with LLVM Exceptions. See `LICENSE.TXT`.

## Contributing

We welcome feedback and contributions, including issue reports, documentation improvements, and Go/LLVM integration patches.

Please see [CONTRIBUTING.md](CONTRIBUTING.md) for contribution guidelines.

For the original LLVM README from the upstream base of this branch, see
[README_UPSTREAM.md](README_UPSTREAM.md).
