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
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/CodeGen/AsmPrinterHandler.h"
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

namespace llvm {

class AsmPrinter;
class MCSymbol;
class DILocation;


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



class MCGoAuxData : public AsmPrinterHandler {

public:
  struct PCData {
    int32_t Value;
    MCSymbol *PC;

    PCData(int32_t value, MCSymbol *pc) : Value(value), PC(pc) {}
  };


  struct FileEntry {
    StringRef Dir;
    StringRef File;
    int32_t Index;
    FileEntry(StringRef D, StringRef F, int32_t I) : Dir(D), File(F), Index(I) {}
  };

private:
  AsmPrinter *Asm;

  // All following data are reset for each new function.
  // the PC of method entry
  MCSymbol* EntryPC;
  MCSymbol* PrePC;
  MCSymbol* PostPC;
  MCGoFuncInfo FuncInfo;
  std::vector<PCData> PCSP;
  std::vector<PCData> PCFile;
  std::vector<PCData> PCLine;
  std::vector<PCData> PCInline;

  std::vector<PCData> PCUnsafePoint;
  std::vector<PCData> PCStackMapIndex;

  std::vector<FileEntry> FileTable;

  int32_t LastSPAdjust;
  DenseMap<const MachineInstr*, int32_t> PostSPMap;

  int32_t LastFileIndex;
  int32_t LastLineNumber;
  int32_t LastInlineTree;
  bool    FileStarted;
  bool    LineStarted;
  bool    InlineTreeStarted;

  int32_t LastStackMapIndex;
  uint64_t MaxStatepointID;
  uint64_t FunctionBeginID;
public:
  MCGoAuxData(AsmPrinter* A) : Asm(A) {
    resetModule();
  }

  virtual ~MCGoAuxData() {
    resetModule();
  }

  virtual void setSymbolSize(const MCSymbol *Sym, uint64_t Size) {}

  virtual void beginModule(Module *M) override;

  virtual void endModule() override {}

  virtual void beginFunction(const MachineFunction *MF);

  // mark the end of function code
  virtual void markFunctionEnd() override {}

  virtual void endFunction(const MachineFunction *MF) override;

  virtual void beginInstruction(const MachineInstr *MI);

  virtual void endInstruction();

  static bool hasPointerTy(Type*);

private:

  void resetModule() {
    resetFunction();

    FileTable.clear();
    MaxStatepointID = 0;
  }


  void resetFunction() {
    EntryPC = nullptr;
    PrePC = nullptr;
    PostPC = nullptr;
    LastSPAdjust = -1;
    LastFileIndex = -1;
    LastLineNumber = -1;
    LastInlineTree = -1;
    LastStackMapIndex = -1;
    FileStarted = false;
    LineStarted = false;
    InlineTreeStarted = false;

    PostSPMap.clear();
    PCSP.clear();
    PCFile.clear();
    PCLine.clear();
    PCInline.clear();
    PCStackMapIndex.clear();

    PCUnsafePoint.clear();
    PCStackMapIndex.clear();

    FuncInfo.clear();
    // FunctionBeginID is the maximum ID seen in previous functions.
    // See GoGC.cpp
    FunctionBeginID = MaxStatepointID;
  }

  static unsigned ptrDataSize(const DataLayout&, Type*);
  static std::string bitVecToString(const BitVector& Bits,
                                    unsigned PtrDataSize,
                                    unsigned PtrSize);
  static void typeSetBits(const DataLayout&,Type*,unsigned,BitVector&);

  void computeFunctionSP(const MachineFunction *MF);
  void computeFuncInfo(const MachineFunction *MF);
  MCGoInlTreeNode* addInlNode(DILocation* M, DISubprogram *Called);

  int32_t getFileIndex(DIFile* F);
  int32_t getInlineTreeNodeIndex(DILocation* DL);

  MCSymbol* getOrCreatePrePCSymbol(const MachineInstr* MI);
  MCSymbol* getOrCreatePostPCSymbol(const MachineInstr* MI);

  void emitPCData(std::vector<PCData> &list, std::string &sym);
  void appendPCData(std::vector<PCData> &list, int32_t value, MCSymbol* PC);
};


} // end namespace llvm

#endif // LLVM_MC_MCGOAUXDATA_H
