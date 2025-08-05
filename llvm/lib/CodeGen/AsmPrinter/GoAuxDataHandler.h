//===- GoAuxDataHandler.h - Machine Code Go Auxiliary Data support ---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 ByteDance Ltd. and/or its affiliates.
//===----------------------------------------------------------------------===//
//
// This file contains the declaration of GoAuxDataHandler to print MCGoAuxData.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CODEGEN_ASMPRINTER_GOAUXDATAHANDLER_H
#define LLVM_CODEGEN_ASMPRINTER_GOAUXDATAHANDLER_H


#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/CodeGen/AsmPrinter.h"
#include "llvm/CodeGen/DebugHandlerBase.h"
#include "llvm/IR/Type.h"
#include "llvm/MC/MCGoAuxData.h"
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

namespace llvm {

class AsmPrinter;
class MCSymbol;
class DILocation;
class DIFile;

class MCGoAuxData : public DebugHandlerBase {
public:
  struct PCData {
    int32_t Value;
    MCSymbol *PC;

    PCData(int32_t value, MCSymbol *pc) : Value(value), PC(pc) {}
  };

private:
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
  std::vector<PCData> PCIndex;

  std::vector<MCGoFileEntry> FileTable;
  DenseMap<DIFile*, int32_t> FileTableMap;
  std::vector<MCGoDwarfEntry> DwarfTable;
  bool FuncInfoHasStartLine;

  int32_t LastSPAdjust;
  DenseMap<const MachineInstr*, int32_t> PreSPMap;
  DenseMap<const MachineInstr*, int32_t> PostSPMap;

  int32_t LastFileIndex;
  int32_t LastLineNumber;
  int32_t LastInlineTree;
  int32_t LastPCIndex;
  bool    FileStarted;
  bool    LineStarted;
  bool    InlineTreeStarted;

  int32_t LastStackMapIndex;
  bool    HasStatepoint;
  bool    RequireDeferReturnMap;

  uint64_t MaxStatepointID;
  uint64_t FunctionBeginID;
public:
  MCGoAuxData(AsmPrinter* A) : DebugHandlerBase(A) {
    resetModule();
  }

  virtual ~MCGoAuxData() {
    resetModule();
  }

  virtual void beginModule(Module *M) override;

  virtual void endModule() override {}

  virtual void beginFunctionImpl(const MachineFunction *MF) override;

  virtual void endFunctionImpl(const MachineFunction *MF) override;

  virtual void beginInstruction(const MachineInstr *MI) override;

  virtual void endInstruction() override;

  static bool hasPointerTy(Type*);

  static void emitFunctionStackMaps(const Function *Fn, AsmPrinter *Asm,
      std::vector<std::vector<uint8_t>> &ArgsTable, std::vector<std::vector<uint8_t>> &LocalsTable,
      uint32_t MaxArgsSize, uint32_t MaxLocalsSize);
private:

  void resetModule() {
    resetFunction();

    FileTable.clear();
    FileTableMap.clear();
    FuncInfoHasStartLine = false;
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
    HasStatepoint = false;
    RequireDeferReturnMap = false;
    LastStackMapIndex = -1;
    LastPCIndex = 0;
    FileStarted = false;
    LineStarted = false;
    InlineTreeStarted = false;

    PreSPMap.clear();
    PostSPMap.clear();
    PCSP.clear();
    PCFile.clear();
    PCLine.clear();
    PCInline.clear();
    PCStackMapIndex.clear();
    PCIndex.clear();

    PCUnsafePoint.clear();
    PCStackMapIndex.clear();

    FuncInfo.clear();
    DwarfTable.clear();
    // FunctionBeginID is the maximum ID seen in previous functions.
    // See GoGCPrinter.cpp
    FunctionBeginID = MaxStatepointID;

  }

  static unsigned ptrDataSize(const DataLayout&, Type*);
  static std::string bitVecToString(const BitVector& Bits,
                                    unsigned PtrDataSize,
                                    unsigned PtrSize);
  static void typeSetBits(const DataLayout&,Type*,unsigned,BitVector&);

  void computeFunctionSP(const MachineFunction *MF);
  void computeFuncInfo(const MachineFunction *MF);
  MCGoInlTreeNode* addInlNode(DILocation* M, DIScope *Called);

  int32_t getFileIndex(DIFile* F);
  int32_t getInlineTreeNodeIndex(DILocation* DL);

  MCSymbol* getOrCreatePrePCSymbol(const MachineInstr* MI);
  MCSymbol* getOrCreatePostPCSymbol(const MachineInstr* MI);

  void emitPCData(std::vector<PCData> &list, std::string &sym);
  void appendPCData(std::vector<PCData> &list, int32_t value, MCSymbol* PC);
  void MDNodeToFileTab(std::vector<MCGoFileEntry> &Dst, NamedMDNode *Meta);
};


} // end namespace llvm

#endif // LLVM_CODEGEN_ASMPRINTER_GOAUXDATAHANDLER_H
