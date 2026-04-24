# Contributing to TangoLLVM

We are happy to accept patches and contributions to TangoLLVM.

TangoLLVM is a fork of the upstream LLVM project with additional support for compiling Go code with LLVM. When contributing, please keep in mind that we aim to stay close to upstream LLVM and keep TangoLLVM‑specific changes clear and easy to maintain.

## Contributor License Agreement

Contributions to this project must be accompanied by a Contributor License Agreement (CLA).
You or your employer retain the copyright to your contribution; the CLA gives
the project permission to use and redistribute it.

When you open a pull request at <https://github.com/bytedance/tangollvm>, the
automated CLA check will guide you through the signing process. You generally
only need to submit the CLA once.

## Changes Accepted

Please file an issue before doing substantial work. This helps avoid duplicate effort and provides a place to discuss design trade‑offs.

In general:

- Keep TangoLLVM‑specific behavior explicit (via attributes, metadata, flags, or well‑documented conventions) so that rebasing onto upstream LLVM remains straightforward.
- Avoid large refactors that are not required for the feature/fix.

## Code Style

Follow LLVM’s coding standards:

- https://llvm.org/docs/CodingStandards.html

This repository contains `.clang-format`. Please run `clang-format` (or `git clang-format`) on C/C++ changes.

## Testing

Run the most relevant LLVM tests for the area you touched. Typical examples:

- `ninja check-llvm`

If your change impacts Go/TangoLLVM integration paths, please also include a minimal reproducer and, when possible, a targeted regression test under `llvm/test/`.

## Issues

We use the issue tracker of the public mirror to track public bugs.

When filing an issue, please include:

- the TangoLLVM/LLVM version (commit hash),
- your OS and architecture,
- build configuration (CMake flags, compiler, linker),
- a minimal reproducer and expected vs. actual behavior,
- if the issue is observed via the Tango Go toolchain, also include the Tango version and how the LLVM backend was enabled.
