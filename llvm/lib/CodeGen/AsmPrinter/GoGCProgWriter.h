//===- GoGCProgWriter.h - Go GCProg support ---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 ByteDance Ltd. and/or its affiliates.
//===----------------------------------------------------------------------===//
//
// This file contains the declaration of the GoGCProgWriter to support the
// generation of Go GCProg. ref: /go/src/cmd/internal/gcprog/gcprog.go
// NOTE: we only compute the bitIndex of the current Type.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CODEGEN_ASMPRINTER_GOGCPROGWRITER_H
#define LLVM_CODEGEN_ASMPRINTER_GOGCPROGWRITER_H

#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Type.h"
#include <cstdint>

namespace llvm {

class MCGoGCProgWriter {
public:
  // the number of bits written to the bit stream.
  uint64_t Index = 0;

  // lit adds a single literal bit to w.
  void lit(uint8_t B);

  // flushlit flushes any pending literal bits.
  void flushLit();

  // Repeat emits an instruction to repeat the description
  // of the last n words c times (including the initial description, c+1 times in total).
  void repeat(int64_t N, int64_t C);

  // Append emits the given GC program into the current output.
  // The caller asserts that the program emits n bits (describes n words),
  // and Append panics if that is not true.
  void append(const uint8_t *Data, int64_t N);

  // Ptr emits a 1 into the bit stream at the given bit index.
  // that is, it records that the index'th word in the object memory is a pointer.
  // Any bits between the current index and the new index
  // are set to zero, meaning the corresponding words are scalars.
  void ptr(int64_t Index);

  // ZeroUntil adds zeros to the bit stream until reaching the given index;
  // that is, it records that the words from the most recent pointer until
  // the index'th word are scalars.
  // ZeroUntil is usually called in preparation for a call to Repeat, Append, or End.
  void zeroUtil(int64_t Index);

  // ShouldRepeat reports whether it would be worthwhile to
  // use a Repeat to describe c elements of n bits each,
  // compared to just emitting c copies of the n-bit description.
  bool shouldRepeat(int64_t N, int64_t C);

  // End marks the end of the program, writing any remaining bytes.
  void end();

  void emit(Type *T, int64_t TypeSize, const DataLayout& DL, int64_t Offset);
};

} // end namespace llvm

#endif // LLVM_CODEGEN_ASMPRINTER_GOGCPROGWRITER_H
