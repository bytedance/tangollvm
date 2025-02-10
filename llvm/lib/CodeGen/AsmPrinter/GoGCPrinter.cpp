// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 ByteDance Ltd. and/or its affiliates.

//===--- GoGC.cpp -----------------------------------------------------------===//
//
// Copyright 2018 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.
//
//===----------------------------------------------------------------------===//
//
// GC strategy for Go runtime.
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/Twine.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/GCMetadataPrinter.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/StackMaps.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/GCStrategy.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCObjectFileInfo.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/Module.h"
#include "llvm/Target/TargetLoweringObjectFile.h"
#include <vector>

using namespace llvm;

namespace {

class GoGC : public GCStrategy {
public:
  GoGC() {
    UseStatepoints = true;
    UsesMetadata = true; // using GCMetadataPrinter to emit stack maps

    // TODO: write barrier?
  }

  std::optional<bool> isGCManagedPointer(const Type *Ty) const override {
    return isa<PointerType>(Ty);
  }
};

GCRegistry::Add<GoGC> X("go", "Go garbage collector.");

class GoGCPrinter : public GCMetadataPrinter {
public:
  bool emitStackMaps(StackMaps &SM, AsmPrinter &AP) override;
};

GCMetadataPrinterRegistry::Add<GoGCPrinter>
  X2("go", "Go garbage collector.");

} // namespace

static void
convertBitVectorToBytesVector(uint32_t &Size, BitVector &BV, std::vector<uint8_t> &Bytes) {
  if (BV.none())
    return;
  int last = BV.find_last();
  Size = last + 1;
  Bytes.reserve(last/8 + 1);
  int i;
  for (i = 0; i <= last; i += 8) {
    uint8_t b = 0;
    for (int j = 0; j < 8 && i+j <= last; j++)
      b |= BV[i+j] ? 1<<j : 0;
    Bytes.push_back(b);
  }
  Bytes.resize(last/8 + 1);
}

// Returns the bitmap (in a vector of bytes) encoding live
// locations.
static void
computeBitVector(Function * Fn, uint64_t StackSize, bool requireFP,
                 const StackMaps::LocationVec &CSLocs,
                 uint32_t &ArgsSize, std::vector<uint8_t> &ArgsMap,
                 uint32_t &LocalsSize, std::vector<uint8_t> &LocalsMap) {
  // TODO: this function is silly -- BitVector internally has
  // a bitmap storage, but it is private. We basically recompute
  // it. Can we do better?

  const int PtrSize = 8; // TODO: get from target info
  BitVector ArgsBV;
  BitVector LocalsBV;
  BitVector *BV = &ArgsBV;

  BitVector LocationSeen;
  // TODO: stack grows down and stack slot = 8
  const uint64_t SlotSize = 8;
  for (unsigned i = 0, n = CSLocs.size(); i < n; i++) {
    const auto &Loc = CSLocs[i];
    switch (Loc.Type) {
    case StackMaps::Location::Direct:
    case StackMaps::Location::Indirect: {
      // TODO: verify that the base register is SP.

      assert(Loc.Offset >= 0 && "replace FP to SP");
      assert(Loc.Offset % PtrSize == 0 && "must be ptr");

      int64_t LocIdx = Loc.Offset / PtrSize;
      if (LocIdx >= LocationSeen.size()) {
        LocationSeen.resize(LocIdx + 1);
      }
      if (LocationSeen.test(LocIdx)) {
        continue;
      }
      LocationSeen.set(LocIdx);

      unsigned Idx;
      bool IsLocal;
      if (Loc.Offset >= (int64_t)StackSize) {
        // The computed StackSize always contains BP.
        // But we may eliminated push RBP if BP is not required.
        if (requireFP) {
          assert((Loc.Offset >= int64_t(StackSize + SlotSize)) && "invariant");
          Idx = (Loc.Offset - StackSize - SlotSize) / PtrSize;
        } else {
          Idx = (Loc.Offset - StackSize) / PtrSize;
        }
        BV = &ArgsBV;
        IsLocal = false;
      } else {
        assert(requireFP && "no-frame should be frameless");
        // Why need to subtract two SlotSize?
        // 1). Loc.Offset is SP based. See replaceFramePtrWithStackPtr in X86RegisterInfo.cpp
        // Subtract one SlotSize for FP.
        // TODO: StackSize must be constant after frame-setup.
        // 2). Stack grows downside in X86. The offset of the bottom most local is -8.
        // The offset in stack map is relative to the top most local.
        // The runtime scans from top to bottom (from lower address to higher address).
        // We must encode -8 as 0;
        // See convertToLocalBytesVector.
        // Example: suppose that the 1st, 3rd and 4th slots contain pointers.
        // FP Offset: -8, -24, -32
        // Idx: 0, 2, 3
        // Bits: 4
        // Reverse Idx: 3, 1, 0
        // Encoded Offset: 24, 8, 0
        // Runtiem: -32 + 24, -32 + 8, -32 + 0
        uint64_t BPSize = SlotSize;
        Idx = (StackSize - BPSize - Loc.Offset - SlotSize) / PtrSize;
        BV = &LocalsBV;
        IsLocal = true;
        assert(Idx < StackSize/PtrSize && "cannot be negative");
      }

#if 0
      dbgs() << "Loc.Offset: " << Loc.Offset << "\n";
      dbgs() << "StackSize: " << StackSize << "\n";
      dbgs() << "FPOffset: " << int64_t(StackSize - SlotSize - Loc.Offset - SlotSize) << "\n";
      dbgs() << "Idx: " << Idx << "\n";
#endif

      // If a slot is following a "Constant" location, it is an aggregate type
      // and that constant encodes the pointer bitmap.
      if (i+1 < n &&
          CSLocs[i+1].Type == StackMaps::Location::Constant) {
        // Make sure it is a local/arg slot, not a spill of in-register value.
        assert(Loc.Type == StackMaps::Location::Direct);
        i++;
        for (; i < n && CSLocs[i].Type == StackMaps::Location::Constant; i++) {
          if (Idx + 32 >= BV->size())
            BV->resize(Idx + 32);
          uint32_t Bits = CSLocs[i].Offset;
          // dbgs() << "ArgBits: " << Bits << "\n";
          for (unsigned j = 0; j < 32; j++)
            if ((Bits>>j)&1) {
              if (IsLocal) {
                // dbgs() << "Local Set: " << Idx << " - " << j << "\n";
                assert(Idx >= j && "stack grows down");
                BV->set(Idx - j);
              } else {
                // dbgs() << "Args Set: " << Idx << " + " << j << "\n";
                BV->set(Idx + j);
              }
            }
          Idx += IsLocal ? -32 : 32;
        }
        i--; // compensate the increment in outer loop
        break;
      }

      // Another case of aggregate type is a spill of a vector of pointers.
      if (Loc.Size > PtrSize) {
        assert(Loc.Type == StackMaps::Location::Indirect);
        for (unsigned j = 0; j < Loc.Size / PtrSize; j++) {
          if (Idx + j >= BV->size())
            BV->resize(Idx + j + 1);
          BV->set(Idx + j);
        }
        break;
      }

      // A single pointer slot.
      assert(Loc.Size == PtrSize);
      if (Idx >= BV->size())
        // This can happen if Loc is an arg slot (for byVal aggregate type).
        BV->resize(Idx + 1);
      BV->set(Idx);
      break;
    }
    default:
      break;
    }
  }

  convertBitVectorToBytesVector(ArgsSize, ArgsBV, ArgsMap);
  convertBitVectorToBytesVector(LocalsSize, LocalsBV, LocalsMap);
}

static void convertToLocalBytesVector(uint32_t MaxMapSize, uint32_t MaxByteSize, std::vector<uint8_t> &Map) {
  std::vector<uint8_t> New(MaxByteSize, 0);
  for (uint32_t i = 0; i < MaxMapSize; i++) {
    uint32_t ByteIndex = i / 8;
    if (ByteIndex >= Map.size()) {
      break;
    }
    uint32_t BitIndex = i % 8;
    if ((Map[ByteIndex] & (1 << BitIndex)) != 0) {
      uint32_t NI = MaxMapSize - i - 1;
      ByteIndex = NI / 8;
      BitIndex = NI % 8;
      uint8_t B = New[ByteIndex];
      B = B | (1 << BitIndex);
      New[ByteIndex] = B;
    }
  }
  Map = New;
}

static void
emitStackMaps(uint32_t MaxMapSize, std::vector<std::vector<uint8_t>> MapTable, MCStreamer &OS, bool isLocal) {
  const int PtrSize = 8; // TODO: get from target info
  OS.emitIntValue(MapTable.size(), 4); // number of maps
  OS.emitIntValue(MaxMapSize, 4); // number of bits in each map
  uint32_t MaxBytes = (MaxMapSize + (PtrSize - 1)) / PtrSize;
#if 0
  dbgs() << "Before TableSize: " << MapTable.size() << "\n";
  dbgs() << "Before MaxMapSize: " << MaxMapSize << "\n";
  dbgs() << "Before MaxBytes: " << MaxBytes << "\n";
  for (auto &Map : MapTable) {
    uint32_t Index = 0;
    for (uint8_t Byte : Map) {
      Index++;
      dbgs() << int(Byte) << " ";
    }
    for (; Index < MaxBytes; Index++) {
      dbgs() << 0 << " ";
    }
    dbgs() << "\n";
  }
#endif
  for (auto &Map : MapTable) {
    uint32_t Index = 0;
    if (isLocal) {
      convertToLocalBytesVector(MaxMapSize, MaxBytes, Map);
    }
    for (uint8_t Byte : Map) {
      Index++;
      OS.emitIntValue(Byte, 1);
    }
    for (; Index < MaxBytes; Index++) {
      OS.emitIntValue(0, 1); // fill with zero bytes
    }
  }
#if 0
  dbgs() << "After MaxMapSize: " << MaxMapSize << "\n";
  dbgs() << "After MaxBytes: " << MaxBytes << "\n";
  for (auto &Map : MapTable) {
    uint32_t Index = 0;
    for (uint8_t Byte : Map) {
      Index++;
      dbgs() << int(Byte) << " ";
    }
    for (; Index < MaxBytes; Index++) {
      dbgs() << 0 << " ";
    }
    dbgs() << "\n";
  }
#endif
}

static bool hasPointer(Type *T) {
  switch (T->getTypeID()) {
  case Type::PointerTyID:
    return true;
  case Type::ArrayTyID:
    return hasPointer(T->getArrayElementType());
  case Type::FixedVectorTyID:
  case Type::ScalableVectorTyID:
    return hasPointer(T->getScalarType());
  case Type::StructTyID: {
    for (unsigned i = 0, e = T->getStructNumElements(); i < e; ++i)
      if (hasPointer(T->getStructElementType(i)))
        return true;
    return false;
  }
  default:
    return false;
  }
}

template <typename F>
static void
visitPointers(Type *T, const DataLayout &DL, int64_t BaseOffset, const F& Func) {
  if (!hasPointer(T))
    return;

  Type *Int32Ty = Type::getInt32Ty(T->getContext());
  switch (T->getTypeID()) {
  case Type::PointerTyID:
    Func(BaseOffset);
    break;;
  case Type::ArrayTyID: {
    Type *ET = T->getArrayElementType();
    for (unsigned i = 0, n = T->getArrayNumElements(); i < n; ++i) {
      Value *ivals[2] = { ConstantInt::get(Int32Ty, 0),
                          ConstantInt::get(Int32Ty, i) };
      ArrayRef<Value*> Idx = ArrayRef(ivals, 2);
      uint64_t Offset = DL.getIndexedOffsetInType(T, Idx);
      visitPointers(ET, DL, BaseOffset+Offset, Func);
    }
    break;
  }
  case Type::FixedVectorTyID: {
    auto *VT = llvm::cast<llvm::FixedVectorType>(T);
    Type *ET = T->getScalarType();
    for (unsigned i = 0, n = VT->getNumElements(); i < n; ++i) {
      Value *ivals[2] = { ConstantInt::get(Int32Ty, 0),
                          ConstantInt::get(Int32Ty, i) };
      ArrayRef<Value*> Idx = ArrayRef(ivals, 2);
      uint64_t Offset = DL.getIndexedOffsetInType(T, Idx);
      visitPointers(ET, DL, BaseOffset+Offset, Func);
    }
    break;
  }
  case Type::StructTyID: {
    for (unsigned i = 0, n = T->getStructNumElements(); i < n; ++i) {
      Type *ET = T->getStructElementType(i);
      if (!hasPointer(ET))
        continue;
      Value *ivals[2] = { ConstantInt::get(Int32Ty, 0),
                          ConstantInt::get(Int32Ty, i) };
      ArrayRef<Value*> Idx = ArrayRef(ivals, 2);
      uint64_t Offset = DL.getIndexedOffsetInType(T, Idx);
      visitPointers(ET, DL, BaseOffset+Offset, Func);
    }
    break;
  }
  case Type::ScalableVectorTyID:
    assert(false && "not expecting ScalableVectorTyID");
  default:
    break;
  }
}

static void emitEntryStackMap(Function *Fn, AsmPrinter &AP, uint32_t &MaxSize, std::vector<uint8_t> &Map) {
  int64_t MaxIndex = -1;
  const DataLayout &DL = AP.getDataLayout();
  const unsigned PtrSize = DL.getPointerSize();
  auto consumeOffset = [&](int64_t Offset){
    int64_t Index = Offset /= PtrSize;
    if (Index > MaxIndex)
      MaxIndex = Index;
    int64_t ByteIndex = Index / PtrSize;
    int64_t BitIndex = Index % PtrSize;
    if (ByteIndex >= (int64_t)Map.size()) {
      Map.resize(ByteIndex + 1, 0);
    }
    Map[ByteIndex] = Map[ByteIndex] | (1 << BitIndex);
  };

  if (auto n = Fn->getMetadata("go.stack.param")) {
    unsigned numOp = n->getNumOperands();
    if (numOp > 0) {
      assert((numOp % 2 == 0) && "invariant");
      for (unsigned i = 0; i < numOp; i += 2) {
        // (type, offset)
        Type *Ty = dyn_cast<ConstantAsMetadata>(n->getOperand(i + 0))->getValue()->getType();
        assert(Ty != nullptr);
        int64_t Offset = dyn_cast<ConstantAsMetadata>(n->getOperand(i + 1))->getValue()->getUniqueInteger().getSExtValue();

        visitPointers(Ty, DL, Offset, consumeOffset);
      }
    }
  }

  if (auto n = Fn->getMetadata("go.caller.args.area")) {
    unsigned numOp = n->getNumOperands();
    if (numOp > 2) {
      assert(((numOp - 2) % 4 == 0) && "must be 4 quadruple array");
      for (unsigned i = 2; i < numOp; i += 4) {
        // (regidx, offset, size, ptr)
        int64_t IsPtr = dyn_cast<ConstantAsMetadata>(n->getOperand(i + 3))->getValue() ->getUniqueInteger().getSExtValue();
        if (IsPtr == 0) {
          continue;
        }
        int64_t Offset = dyn_cast<ConstantAsMetadata>(n->getOperand(i + 1))->getValue()->getUniqueInteger().getSExtValue();
        int64_t Size = dyn_cast<ConstantAsMetadata>(n->getOperand(i + 2))->getValue()->getUniqueInteger().getSExtValue();
        assert(Size == 8 && "amd64 must be 8 bytes ptr");
        assert(Offset >= 0 && Offset % 8 == 0 && "must be 8 byte aligned");
        Offset -= 8; // Offset is relative to FP. We need to subtract slot for PC. See llvm.go
        consumeOffset(Offset);
      }
    }
  }

  if (MaxIndex + 1 > MaxSize)
    MaxSize = MaxIndex + 1;
}

static void
emitCallsiteEntries(StackMaps &SM, AsmPrinter &AP) {
  auto &CSInfos = SM.getCSInfos();
  if (CSInfos.empty())
    return;

  MCStreamer &OS = *AP.OutStreamer;
  MCContext &OutContext = OS.getContext();

  const Module * M = AP.MMI->getModule();
  const DataLayout &DL = AP.getDataLayout();

  auto CSI = CSInfos.begin();
  uint64_t MaxID = 0;
  for (auto const &FR : SM.getFnInfos()) {
    const size_t RecordCount = FR.second.RecordCount;
    std::vector<std::vector<uint8_t>> ArgsTable;
    std::vector<std::vector<uint8_t>> LocalsTable;
    ArgsTable.resize(RecordCount + 1);
    LocalsTable.resize(RecordCount + 1);
    uint32_t MaxArgsSize = 0, MaxLocalsSize = 0;
    uint64_t BeginID = MaxID;

    auto FuncName = FR.first->getName();
    // All Go Globals are external linkages
    assert(FuncName.size() > 0 && "must not be empty");
    if (FuncName[0] == DL.getGlobalPrefix()) {
      FuncName = FuncName.substr(1);
    }
    Function *Fn = M->getFunction(FuncName);
    assert(Fn != nullptr && "a compiled function cannot be null");

    bool requireFP = true;
    assert(Fn->hasFnAttribute("go-no-frame-checked") && "See checkGoFrame in PEI");
    auto Attr = Fn->getFnAttribute("go-no-frame-checked").getValueAsString();
    if (Attr == "true") {
      requireFP = false;
    }

    for (unsigned i = 0; i < RecordCount; i++, CSI++) {
      uint64_t ID = (*CSI).ID;
      if (ID >= MaxID) {
        MaxID = ID + 1;
      }
      assert(ID >= BeginID && "invariant");
      uint64_t Index = ID - BeginID + 1; // Index starts from 1. 0 is reserved for entry stack map.
      if (Index >= ArgsTable.size()) {
        ArgsTable.resize(Index + 1);
        LocalsTable.resize(Index + 1);
      }
      uint32_t ArgsSize = 0, LocalsSize = 0;
      std::vector<uint8_t> &ArgsMap = ArgsTable[Index];
      std::vector<uint8_t> &LocalsMap = LocalsTable[Index];

#if 0
      dbgs() << "Compute StackMap for " << FR.first->getName() << ", ID=" << ID << "\n";
#endif

      computeBitVector(Fn, FR.second.StackSize, requireFP, (*CSI).Locations, ArgsSize, ArgsMap, LocalsSize, LocalsMap);
      if (ArgsSize > MaxArgsSize) {
        MaxArgsSize = ArgsSize;
      }
      if (LocalsSize > MaxLocalsSize) {
        MaxLocalsSize = LocalsSize;
      }

#if 0
      dbgs() << "Print StackMap for " << FR.first->getName() << ", ID=" << ID << "\n";
      dbgs() << "ArgsSize: " << ArgsSize << "\n";
      for (uint8_t Byte : ArgsMap) {
        dbgs() << " " << int(Byte);
      }
      dbgs() << "\n";
      dbgs() << "LocalsSize: " << LocalsSize << "\n";
      for (uint8_t Byte : LocalsMap) {
        dbgs() << " " << int(Byte);
      }
      dbgs() << "\n";
#endif
    }
    {
      std::string symbol = FR.first->getName().str();
      symbol += "..argspointermaps";
      MCSymbol *AddrSymbol = OutContext.getOrCreateSymbol(StringRef(symbol));
      OS.emitLabel(AddrSymbol);
      std::vector<uint8_t> &EntryMap = ArgsTable[0];
      emitEntryStackMap(Fn, AP, MaxArgsSize, EntryMap);
      emitStackMaps(MaxArgsSize, ArgsTable, OS, false);
    }
    {
      std::string symbol = FR.first->getName().str();
      symbol += "..localspointermaps";
      MCSymbol *AddrSymbol = OutContext.getOrCreateSymbol(StringRef(symbol));
      OS.emitLabel(AddrSymbol);
      emitStackMaps(MaxLocalsSize, LocalsTable, OS, true);
    }
  }
}

bool
GoGCPrinter::emitStackMaps(StackMaps &SM, AsmPrinter &AP) {
  const TargetLoweringObjectFile &TLOF= AP.getObjFileLowering();
  const DataLayout &DL = AP.getDataLayout();
  Align Alignment(1); // variant length encoding
  MCSection *ReadOnlySection = TLOF.getSectionForConstant(
      DL, SectionKind::getReadOnly(), /*C=*/nullptr, Alignment);
  AP.OutStreamer->pushSection();
  AP.OutStreamer->switchSection(ReadOnlySection);

  emitCallsiteEntries(SM, AP);

  AP.OutStreamer->popSection();
  return true;
}
