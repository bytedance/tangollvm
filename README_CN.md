<div align="center">

# TangoLLVM

[English](README.md)

面向 Tango Go 工具链增强的 LLVM 发行版。

</div>

## 简介

TangoLLVM 是基于上游 [LLVM 项目](https://llvm.org/) 的修改版，在此基础上增加了额外改动，以更好地支持使用 LLVM 来编译 Go 代码。

它主要服务于 **[Tango](https://github.com/bytedance/tango)**：字节跳动增强版 Go 发行版（在社区标准 Go SDK 之上的超集）。在 Tango 中，LLVM 后端是一个可选的编译期后端（在 Go 侧启用），生成的二进制文件在运行时 **不依赖** LLVM。

本仓库聚焦于该集成方案中的 LLVM 侧工作。除上游 LLVM 单仓（monorepo）的内容外，还包含若干面向 Go 的增强，例如：

- Go ABIInternal 调用约定以及相关的 CodeGen 支持。
- Go runtime 调用模式（例如栈增长 / morestack、写屏障辅助函数等）。
- 与 Go GC / statepoint 相关的工具与 pass。
- 一小部分给 Go 前端使用的 C API（见 `llvm/include/llvm-c/TangoLLVM.h`）。

从整体上看，端到端的 TangoLLVM 流水线是：

- （在 Tango Go 编译器中）前端将 Go Generic SSA lower 到 LLVM IR。
- TangoLLVM 在遵守 Go ABIInternal 约定的前提下，将 LLVM IR 编译为 ELF/MachO 的可重定位目标文件。
- patch/merge 步骤将上述结果与 Go 工具链的其它产物合并。

后续规划：本仓库计划会增加 Go Bolt 相关支持。

如果你关注完整的 Tango 工具链以及端到端使用方式，请参考 [Tango 仓库与其文档](https://github.com/bytedance/tango)。

## 仓库结构

本仓库沿用上游 `llvm-project` 的单仓结构（例如 `llvm/`、`clang/`、`lld/`、`compiler-rt/` 等）。

此外，仓库还提供了一些辅助脚本，用于构建与打包一个精简的 TangoLLVM 发行版：

- `build.sh`：构建并安装一个 release 版本到 `llvm-bin-release/`。

## 从源码构建

### 前置依赖

- C++17 工具链（Clang 或 GCC）
- CMake >= 3.20
- Ninja

### 快速构建（推荐）

仓库提供了开箱即用的构建脚本：

```bash
./build.sh
```

默认配置为：

- Release 构建，并开启断言（assertions），
- 构建 `X86` 目标，
- 安装到 `./llvm-bin-release/`。

### 自定义构建

你也可以使用标准的 LLVM CMake 选项进行构建：

```bash
mkdir -p build && cd build
cmake -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLVM_ENABLE_ASSERTIONS=ON \
  -DLLVM_TARGETS_TO_BUILD="X86" \
  ../llvm
ninja
```

更多细节请参考 LLVM 上游文档：

- https://llvm.org/docs/GettingStarted.html

## 许可证

本项目使用 Apache License v2.0 with LLVM Exceptions，见 `LICENSE.TXT`。

## 贡献

欢迎反馈与贡献，包括（但不限于）问题报告、文档改进，以及 Go/LLVM 集成相关的补丁。

贡献指南请见 [CONTRIBUTING.md](CONTRIBUTING.md)。

本分支所基于的 LLVM 上游原始 README 保留在
[README_UPSTREAM.md](README_UPSTREAM.md)。
