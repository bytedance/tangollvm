// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 ByteDance Ltd. and/or its affiliates.

//===- lib/CodeGen/AsmPrinter/GoGCProgWriter.cpp - GoGCProgWriter implementation ----------------===//

#include "GoGCProgWriter.h"
#include "GoAuxDataHandler.h"

void llvm::MCGoGCProgWriter::lit(uint8_t B) {
  Index++;
}

void llvm::MCGoGCProgWriter::flushLit() {
  // ...
}

void llvm::MCGoGCProgWriter::repeat(int64_t N, int64_t C) {
  if (N == 0 || C == 0) {
    return;
  }
  flushLit();
  // ...
  Index += N * C;
}

void llvm::MCGoGCProgWriter::append(const uint8_t *Data, int64_t N) {
  flushLit();
  // ...
  Index += N;
}

void llvm::MCGoGCProgWriter::ptr(int64_t Index) {
  zeroUtil(Index);
  lit(1);
}

void llvm::MCGoGCProgWriter::zeroUtil(int64_t Index) {
  int64_t Skip = Index - this->Index;
  if (Skip == 0) {
    return;
  }
  if (Skip < 32) {
    for (int64_t I = 0; I < Skip; I++) {
      lit(0);
    }
    return;
  }
  lit(0);
  flushLit();
  repeat(1, Skip - 1);
}

bool llvm::MCGoGCProgWriter::shouldRepeat(int64_t N, int64_t C) {
  return C > 1 && C * N > 32;
}

void llvm::MCGoGCProgWriter::end() {
  // ..
}

void llvm::MCGoGCProgWriter::emit(Type *T, int64_t TypeSize, const DataLayout& DL, int64_t Offset) {
  if (!MCGoAuxData::hasPointerTy(T)) {
    return;
  }
  unsigned PtrSize = DL.getPointerSize();
  if (TypeSize == PtrSize) {
    ptr(Offset / PtrSize);
    return;
  }

  switch (T->getTypeID()) {
  case Type::ArrayTyID: {
    int64_t NumElem = T->getArrayNumElements();
    if (NumElem == 0) {
      report_fatal_error("MCGoGCProgWriter::emit, empty array");
    }
    Type* ET = T->getArrayElementType();
    while (ET->isArrayTy()) {
      NumElem *= ET->getArrayNumElements();
      ET = ET->getArrayElementType();
    }

    int64_t ETSize = DL.getTypeAllocSize(ET).getFixedValue();
    if (!shouldRepeat(ETSize/ PtrSize, NumElem)) {
      for (int64_t i = 0; i < NumElem; i++) {
        emit(ET, ETSize, DL, Offset + i * ETSize);
      }
      return;
    }
    emit(ET, ETSize, DL, Offset);
    zeroUtil((Offset + ETSize)/ PtrSize);
    repeat(ETSize/ PtrSize, NumElem - 1);
    return;
  }
  case Type::StructTyID:{
    StructType* ST = dyn_cast<StructType>(T);
    StringRef STName = ST->getName();
    if (STName.starts_with("string$") ||STName.starts_with("slice$")) {
      ptr(Offset / PtrSize);
      return;
    }
    if (STName.starts_with("interface$")) {
      ptr(Offset / PtrSize + 1);
      return;
    }
    const StructLayout* SL = DL.getStructLayout(ST);
    for (unsigned i = 0; i < ST->getNumElements(); i++) {
      Type* ET = ST->getElementType(i);
      emit(ET, DL.getTypeAllocSize(ET).getFixedValue(), DL,
           Offset + SL->getElementOffset(i));
    }
    return;
  }
  default: {
    report_fatal_error("MCGoGCProgWriter::emit, unsupported Type");
  }
  }
}
