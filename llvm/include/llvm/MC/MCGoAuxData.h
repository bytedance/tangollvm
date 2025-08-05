//===- MCGoAuxData.h - Machine Code Go Auxiliary Data support ---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 ByteDance Ltd. and/or its affiliates.
//===----------------------------------------------------------------------===//
//
// This file contains the declaration of the MCDwarfFile to support the dwarf
// .file directive and the .loc directive.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_MC_MCGOAUXDATA_H
#define LLVM_MC_MCGOAUXDATA_H


#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringRef.h"
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

namespace llvm {

class MCSymbol;
class MCSymbolRefExpr;
class MCExpr;
class MachineInstr;
class DILocation;

struct MCGoFileEntry {
  std::string Dir;
  std::string File;
  int32_t Index;
  MCGoFileEntry(StringRef D, StringRef F, int32_t I) : Dir(D.str()), File(F.str()), Index(I) {}
};

struct MCGoDwarfEntry {
  uint32_t FileNum;
  uint32_t Index;
  uint32_t Line;
  uint16_t Column;
  uint8_t Flags;
  uint32_t Discriminator;

  MCGoDwarfEntry(uint32_t file_num, uint32_t index, uint32_t line,
                 uint16_t column, uint8_t flags, uint32_t discriminator)
      : FileNum(file_num), Index(index), Line(line), Column(column), Flags(flags), Discriminator(discriminator) {}
};

struct MCGoStackObjectRecord {
  int32_t FrameOffset;
  int32_t ObjectSize;
  int32_t PtrData;
  const MCSymbolRefExpr* GCData;

  MCGoStackObjectRecord(int32_t F, int32_t S, int32_t P, const MCSymbolRefExpr* G)
    : FrameOffset(F), ObjectSize(S), PtrData(P), GCData(G) {}
};


class MCGoInlTreeNode {
  friend class MCGoAuxData;
  friend class MCGoFuncInfo;
public:
  int32_t Parent;
  uint32_t File;
  int32_t Line;
  const MCSymbolRefExpr* Func;
  const MCExpr* ParentPC;

private:
  // local index
  int32_t Index;
  // the location of this inlined call
  DILocation *Loc;
  // directly included locations of the inlined call
  std::vector<DILocation*> Included;

  MCGoInlTreeNode *ParentNode;

  MCGoInlTreeNode() : Parent(0), File(0), Line(0), Func(nullptr), ParentPC(nullptr),
    Index(0), Loc(nullptr), Included(), ParentNode(nullptr) {}

  void addIncluded(DILocation* L);
};

class MCGoFuncInfo {
  friend class MCGoAuxData;
public:
  uint32_t Args;
  uint32_t Locals;
  uint8_t FuncID;
  uint8_t FuncFlag;
  uint32_t StartLine;
  bool WriteStartLine;
  std::vector<uint32_t> File;
  // every location has one or nil inlined at
  std::vector<MCGoInlTreeNode*> InlTree;

private:
  DenseMap<DILocation*, MCGoInlTreeNode*> LocToInlNode;
  // Use LocToInstr do a precomputation to avoid creating too many MCSymbol
  DenseMap<DILocation*, const MachineInstr*> LocToInstr;
  DenseMap<DILocation*, MCSymbol*> LocToPC;
  // for those with empty inlinedAt
  std::vector<DILocation*> Included;

  void addIncluded(DILocation* L);

  void recordUsedFile(uint32_t Index);

  void clear() {
    Args = 0;
    Locals = 0;
    FuncID = 0;
    FuncFlag = 0;

    StartLine = 0;
    WriteStartLine = false;

    File.clear();
    for (auto it : LocToInlNode) {
      delete it.second;
    }
    LocToInlNode.clear();
    InlTree.clear();
    LocToInstr.clear();
    Included.clear();
  }

  DILocation* findRealLocation(DILocation *DL);
  const MachineInstr* findInstr(DILocation *DL);
};

} // end namespace llvm

#endif // LLVM_MC_MCGOAUXDATA_H
