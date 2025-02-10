// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 ByteDance Ltd. and/or its affiliates.

//===- lib/MC/MCGoAuxData.cpp - MCGoAuxData implementation ----------------===//

#include "llvm/IR/DebugLoc.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/MC/MCGoAuxData.h"

using namespace llvm;

static int locationIndex(std::vector<DILocation*> &list, DILocation* L) {
  for (std::vector<DILocation*>::size_type i = 0; i < list.size(); i++) {
    if (list[i] == L) {
      return i;
    }
  }
  return -1;
}

void MCGoInlTreeNode::addIncluded(DILocation* L) {
  if (locationIndex(Included, L) == -1) {
    Included.push_back(L);
  }
}

void MCGoFuncInfo::addIncluded(DILocation* L) {
  if (locationIndex(Included, L) == -1) {
    Included.push_back(L);
  }
}

// locations are sorted by line number.
// All locations must be in the same scope (guaranteed by construction);
static DILocation* findDILocation(std::vector<DILocation*> &locations, DILocation *DL) {
  if (locations.empty()) {
    return nullptr;
  }
  int begin = 0;
  int end = locations.size();
  int mid = 0;
  // each location in locations has a PC
  while (begin < end) {
    mid = (begin + end) >> 1;
    DILocation *L = locations[mid];
    // begin <= mid < end
    if (L->getLine() == DL->getLine()) {
      return L;
    } else if (L->getLine() < DL->getLine()) {
      begin = mid + 1;
    } else {
      end = mid;
    }
  }

  // 0 <= mid < locations.size()
  DILocation *L = locations[mid];
  if (L->getLine() < DL->getLine()) {
    return L;
  }
  mid = mid - 1;
  if (mid >= 0) {
    return locations[mid];
  }
  return nullptr;
}

DILocation* MCGoFuncInfo::findRealLocation(DILocation *DL) {
  // internal inlined tree node: find the nearest sibling if it has.
  // used to find the PC of parent.
  DILocation* InlinedAt = DL->getInlinedAt();
  while (InlinedAt != nullptr) {
    auto It = LocToInlNode.find(InlinedAt);
    if (It == LocToInlNode.end()) {
      report_fatal_error("cannot find MCGoInlTreeNode");
    }
    MCGoInlTreeNode *Node = It->second;
    DILocation *R = findDILocation(Node->Included, DL);
    if (R != nullptr) {
      return R;
    }
    DL = InlinedAt;
    InlinedAt = DL->getInlinedAt();
  }

  return findDILocation(Included, DL);
}

// return nullptr if it cannot find any location.
// Caller then can use the entry of the function.
const MachineInstr* MCGoFuncInfo::findInstr(DILocation *DL) {
  {
    // a real location that has a PC
    auto It = LocToInstr.find(DL);
    if (It != LocToInstr.end()) {
      return It->second;
    }
  }

  DILocation *RDL = findRealLocation(DL);
  if (RDL != nullptr) {
    auto It = LocToInstr.find(RDL);
    if (It != LocToInstr.end()) {
      return It->second;
    }
    report_fatal_error("cannot find a MachineInstr for a real location");
  }
  return nullptr;
}


void MCGoFuncInfo::recordUsedFile(uint32_t Index) {
  for (auto F : File) {
    if (F == Index) {
      return;
    }
  }
  File.push_back(Index);
}

