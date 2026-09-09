; Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
; See https://llvm.org/LICENSE.txt for license information.
; SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
;
; Copyright (c) 2026 ByteDance Ltd. and/or its affiliates.

; RUN: llc -stop-after=finalize-isel -mattr=-avx %s -o - | FileCheck %s --check-prefix=SSE
; RUN: llc -stop-after=finalize-isel -mattr=+avx %s -o - | FileCheck %s --check-prefix=AVX
;
; Duff entry offsets are part of the TangoLLVM runtime assembly contract.
; Baseline amd64 must use SSE entries unless AVX is explicitly enabled.
; SSE: CALL64pcrel32 @runtime.duffzero + 184, csr_64_allregs_nodi,
; SSE: CALL64pcrel32 @runtime.duffcopy + 448, csr_64_allregs_nodisixmm0,
; AVX: CALL64pcrel32 @runtime.duffcopy + 1153, csr_64_allregs_nodisi_nosse,

target triple = "x86_64-unknown-linux-goabi"

declare void @llvm.memset.p0.i64(ptr nocapture writeonly, i8, i64, i1 immarg)
declare void @llvm.memcpy.p0.p0.i64(ptr noalias nocapture writeonly, ptr noalias nocapture readonly, i64, i1 immarg)

define cc 113 void @zero(ptr %p) {
  call void @llvm.memset.p0.i64(ptr %p, i8 0, i64 512, i1 false)
  ret void
}

define cc 113 void @copy(ptr %p, ptr %q) {
  call void @llvm.memcpy.p0.p0.i64(ptr %p, ptr %q, i64 512, i1 false)
  ret void
}
