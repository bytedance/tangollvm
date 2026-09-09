//===-- X86SelectionDAGInfo.cpp - X86 SelectionDAG Info -------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Copyright (c) 2025 ByteDance Ltd. and/or its affiliates.
// This file has been modified by ByteDance Ltd. and/or its affiliates on 2025-01-28.
//===----------------------------------------------------------------------===//
//
// This file implements the X86SelectionDAGInfo class.
//
//===----------------------------------------------------------------------===//

#include "X86SelectionDAGInfo.h"
#include "X86ISelLowering.h"
#include "X86InstrInfo.h"
#include "X86RegisterInfo.h"
#include "X86Subtarget.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/CodeGen/TargetLowering.h"
#include "llvm/Support/TypeSize.h"
#include "llvm/IR/Module.h"
#include "llvm/TangoLLVM/TangoLLVM.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"

using namespace llvm;

#define DEBUG_TYPE "x86-selectiondag-info"

static cl::opt<bool>
    UseFSRMForMemcpy("x86-use-fsrm-for-memcpy", cl::Hidden, cl::init(false),
                     cl::desc("Use fast short rep mov in memcpy lowering"));

// Use the AVX version of duffzero even when AVX is not available in general.
static cl::opt<bool>
    UseAVXForDuffzero("x86-use-avx-for-duffzero", cl::Hidden, cl::init(false),
                     cl::desc("Use AVX version of duffzero"));

// Use the AVX version of duffcopy even when AVX is not available in general.
static cl::opt<bool>
    UseAVXForDuffcopy("x86-use-avx-for-duffcopy", cl::Hidden, cl::init(false),
                     cl::desc("Use AVX version of duffcopy"));


bool X86SelectionDAGInfo::isBaseRegConflictPossible(
    SelectionDAG &DAG, ArrayRef<MCPhysReg> ClobberSet) const {
  // We cannot use TRI->hasBasePointer() until *after* we select all basic
  // blocks.  Legalization may introduce new stack temporaries with large
  // alignment requirements.  Fall back to generic code if there are any
  // dynamic stack adjustments (hopefully rare) and the base pointer would
  // conflict if we had to use it.
  MachineFrameInfo &MFI = DAG.getMachineFunction().getFrameInfo();
  if (!MFI.hasVarSizedObjects() && !MFI.hasOpaqueSPAdjustment())
    return false;

  const X86RegisterInfo *TRI = static_cast<const X86RegisterInfo *>(
      DAG.getSubtarget().getRegisterInfo());
  return llvm::is_contained(ClobberSet, TRI->getBaseRegister());
}

SDValue X86SelectionDAGInfo::EmitTargetCodeForMemset(
    SelectionDAG &DAG, const SDLoc &dl, SDValue Chain, SDValue Dst, SDValue Val,
    SDValue Size, Align Alignment, bool isVolatile, bool AlwaysInline,
    MachinePointerInfo DstPtrInfo) const {
  // If to a segment-relative address space, use the default lowering.
  if (DstPtrInfo.getAddrSpace() >= 256)
    return SDValue();

  // If the base register might conflict with our physical registers, bail out.
  const MCPhysReg ClobberSet[] = {X86::RCX, X86::RAX, X86::RDI,
                                  X86::ECX, X86::EAX, X86::EDI};
  if (isBaseRegConflictPossible(DAG, ClobberSet))
    return SDValue();

  ConstantSDNode *ConstantSize = dyn_cast<ConstantSDNode>(Size);
  const X86Subtarget &Subtarget =
      DAG.getMachineFunction().getSubtarget<X86Subtarget>();

  // TANGOLLVM
  // Handle medium and large sizes for OpZero in Golang.
  if (TangoLLVM::isGoABIInternal(DAG.getMachineFunction().getFunction())) {
    auto ConstantVal = dyn_cast<ConstantSDNode>(Val);
    if (ConstantSize && ConstantVal && ConstantVal->isZero()) {
      uint64_t SizeVal = dyn_cast<ConstantSDNode>(Size)->getZExtValue();

      // Use duffzero for medium sizes.
      // 1024 is a magic number, which can be tuned.
      if (SizeVal <= 1024) {
        const uint64_t dzBlocks    = 16; // number of MOV/ADD blocks
        const uint64_t dzBlockLen  = Subtarget.hasAVX() || UseAVXForDuffzero ? 2 : 4;  // number of clears per block
        const uint64_t dzBlockSize = Subtarget.hasAVX() || UseAVXForDuffzero ? 13 : 23; // size of instructions in a single block
        const uint64_t dzMovSize   = 5;  // size of single MOV instruction w/ offset
        const uint64_t dzLeaqSize  = 4;  // size of single LEAQ instruction
        const uint64_t dzClearStep = Subtarget.hasAVX() || UseAVXForDuffzero ? 32 : 16; // number of bytes cleared by each MOV instruction
        const uint64_t dzOffset    = Subtarget.hasAVX() || UseAVXForDuffzero ? 23 * 16 + 1 : 0;

        const uint64_t BytesLeft = SizeVal % dzClearStep;
        const uint64_t AlignedSizeVal = SizeVal - BytesLeft;

        uint64_t Steps = AlignedSizeVal / dzClearStep;
        const uint64_t Blocks = Steps / dzBlockLen;
        Steps %= dzBlockLen;
        uint64_t Off = dzOffset + dzBlockSize * (dzBlocks - Blocks);
        int64_t Adj = 0;
        if (Steps != 0) {
          Off -= dzLeaqSize;
          Off -= dzMovSize * Steps;
          Adj -= dzClearStep * (dzBlockLen - Steps);
        }

        if (Adj != 0) {
          Dst = DAG.getMemBasePlusOffset(Dst, TypeSize::getFixed(Adj), dl);
        }

        TargetLowering::ArgListTy Args;
        TargetLowering::ArgListEntry Entry;
        Entry.Ty = PointerType::getUnqual(Type::getInt8Ty(*DAG.getContext()));
        Entry.Node = Dst; Args.push_back(Entry);

        const TargetLowering &TLI = DAG.getTargetLoweringInfo();
        const char * Symbol = "runtime.duffzero";
        auto *M = DAG.getMachineFunction().getFunction().getParent();
        auto *Fn = M->getFunction(Symbol);
        if (Fn == nullptr) {
          Fn = TangoLLVM::createGoFunction(FunctionType::get(Type::getVoidTy(*DAG.getContext()), {}, false),
                       Function::ExternalLinkage, Symbol, M);
        }
        auto FnPtrTy = TLI.getPointerTy(DAG.getDataLayout(), Fn->getAddressSpace());
        SDValue Callee = DAG.getTargetGlobalAddress(Fn, dl, FnPtrTy, Off);

        // FIXME: pass in SDLoc
        TargetLowering::CallLoweringInfo CLI(DAG);
        CLI.setDebugLoc(dl)
          .setChain(Chain)
          .setCallee(TLI.getLibcallCallingConv(RTLIB::MEMSET), // Use DI to pass the first argument
              Dst.getValueType().getTypeForEVT(*DAG.getContext()),
              Callee, std::move(Args))
          .setDiscardResult();
        std::pair<SDValue,SDValue> CallResult = TLI.LowerCallTo(CLI);
        Chain = CallResult.second;

        // Use inlined SIMD instructions for the remaining bytes if needed.
        if (BytesLeft) {
          EVT AddrVT = Dst.getValueType();
          EVT SizeVT = Size.getValueType();
          Chain =
              DAG.getMemset(Chain, dl,
                            DAG.getNode(ISD::ADD, dl, AddrVT, Dst,
                                        DAG.getConstant(AlignedSizeVal-Adj, dl, AddrVT)),
                            Val, DAG.getConstant(BytesLeft, dl, SizeVT), Alignment,
                            isVolatile, /* AlwaysInline */ true, /* CI */ nullptr,
                            DstPtrInfo.getWithOffset(AlignedSizeVal-Adj));
        }

        return Chain;
      }

      // Use repstosb for large sizes assuming ERMSB is popular.
      // TODO: Generate repstoq for CPU without ERMSB.
      SDValue InGlue;
      EVT AVT = MVT::i8;
      unsigned ValReg = X86::AL;
      SDValue Count = DAG.getIntPtrConstant(SizeVal, dl);

      Chain = DAG.getCopyToReg(Chain, dl, ValReg, DAG.getConstant(0, dl, AVT),
                             InGlue);
      InGlue = Chain.getValue(1);

      bool Use64BitRegs = Subtarget.isTarget64BitLP64();
      Chain = DAG.getCopyToReg(Chain, dl, Use64BitRegs ? X86::RCX : X86::ECX,
                              Count, InGlue);
      InGlue = Chain.getValue(1);
      Chain = DAG.getCopyToReg(Chain, dl, Use64BitRegs ? X86::RDI : X86::EDI,
                              Dst, InGlue);
      InGlue = Chain.getValue(1);

      SDVTList Tys = DAG.getVTList(MVT::Other, MVT::Glue);
      SDValue Ops[] = { Chain, DAG.getValueType(AVT), InGlue };
      Chain = DAG.getNode(X86ISD::REP_STOS, dl, Tys, Ops);
      return Chain;
    }
  }

  // If not DWORD aligned or size is more than the threshold, call the library.
  // The libc version is likely to be faster for these cases. It can use the
  // address value and run time information about the CPU.
  if (Alignment < Align(4) || !ConstantSize ||
      ConstantSize->getZExtValue() > Subtarget.getMaxInlineSizeThreshold())
    return SDValue();

  uint64_t SizeVal = ConstantSize->getZExtValue();
  SDValue InGlue;
  EVT AVT;
  SDValue Count;
  unsigned BytesLeft = 0;
  if (auto *ValC = dyn_cast<ConstantSDNode>(Val)) {
    unsigned ValReg;
    uint64_t Val = ValC->getZExtValue() & 255;

    // If the value is a constant, then we can potentially use larger sets.
    if (Alignment >= Align(4)) {
      // DWORD aligned
      AVT = MVT::i32;
      ValReg = X86::EAX;
      Val = (Val << 8)  | Val;
      Val = (Val << 16) | Val;
      if (Subtarget.is64Bit() && Alignment >= Align(8)) { // QWORD aligned
        AVT = MVT::i64;
        ValReg = X86::RAX;
        Val = (Val << 32) | Val;
      }
    } else if (Alignment == Align(2)) {
      // WORD aligned
      AVT = MVT::i16;
      ValReg = X86::AX;
      Val = (Val << 8) | Val;
    } else {
      // Byte aligned
      AVT = MVT::i8;
      ValReg = X86::AL;
      Count = DAG.getIntPtrConstant(SizeVal, dl);
    }

    if (AVT.bitsGT(MVT::i8)) {
      unsigned UBytes = AVT.getSizeInBits() / 8;
      Count = DAG.getIntPtrConstant(SizeVal / UBytes, dl);
      BytesLeft = SizeVal % UBytes;
    }

    Chain = DAG.getCopyToReg(Chain, dl, ValReg, DAG.getConstant(Val, dl, AVT),
                             InGlue);
    InGlue = Chain.getValue(1);
  } else {
    AVT = MVT::i8;
    Count  = DAG.getIntPtrConstant(SizeVal, dl);
    Chain  = DAG.getCopyToReg(Chain, dl, X86::AL, Val, InGlue);
    InGlue = Chain.getValue(1);
  }

  bool Use64BitRegs = Subtarget.isTarget64BitLP64();
  Chain = DAG.getCopyToReg(Chain, dl, Use64BitRegs ? X86::RCX : X86::ECX,
                           Count, InGlue);
  InGlue = Chain.getValue(1);
  Chain = DAG.getCopyToReg(Chain, dl, Use64BitRegs ? X86::RDI : X86::EDI,
                           Dst, InGlue);
  InGlue = Chain.getValue(1);

  SDVTList Tys = DAG.getVTList(MVT::Other, MVT::Glue);
  SDValue Ops[] = {Chain, DAG.getValueType(AVT), InGlue};
  SDValue RepStos = DAG.getNode(X86ISD::REP_STOS, dl, Tys, Ops);

  /// RepStos can process the whole length.
  if (BytesLeft == 0)
    return RepStos;

  // Handle the last 1 - 7 bytes.
  SmallVector<SDValue, 4> Results;
  Results.push_back(RepStos);
  unsigned Offset = SizeVal - BytesLeft;
  EVT AddrVT = Dst.getValueType();
  EVT SizeVT = Size.getValueType();

  Results.push_back(
      DAG.getMemset(Chain, dl,
                    DAG.getNode(ISD::ADD, dl, AddrVT, Dst,
                                DAG.getConstant(Offset, dl, AddrVT)),
                    Val, DAG.getConstant(BytesLeft, dl, SizeVT), Alignment,
                    isVolatile, AlwaysInline,
                    /* CI */ nullptr, DstPtrInfo.getWithOffset(Offset)));

  return DAG.getNode(ISD::TokenFactor, dl, MVT::Other, Results);
}

/// Emit a single REP MOVS{B,W,D,Q} instruction.
static SDValue emitRepmovs(const X86Subtarget &Subtarget, SelectionDAG &DAG,
                           const SDLoc &dl, SDValue Chain, SDValue Dst,
                           SDValue Src, SDValue Size, MVT AVT) {
  const bool Use64BitRegs = Subtarget.isTarget64BitLP64();
  const unsigned CX = Use64BitRegs ? X86::RCX : X86::ECX;
  const unsigned DI = Use64BitRegs ? X86::RDI : X86::EDI;
  const unsigned SI = Use64BitRegs ? X86::RSI : X86::ESI;

  SDValue InGlue;
  Chain = DAG.getCopyToReg(Chain, dl, CX, Size, InGlue);
  InGlue = Chain.getValue(1);
  Chain = DAG.getCopyToReg(Chain, dl, DI, Dst, InGlue);
  InGlue = Chain.getValue(1);
  Chain = DAG.getCopyToReg(Chain, dl, SI, Src, InGlue);
  InGlue = Chain.getValue(1);

  SDVTList Tys = DAG.getVTList(MVT::Other, MVT::Glue);
  SDValue Ops[] = {Chain, DAG.getValueType(AVT), InGlue};
  return DAG.getNode(X86ISD::REP_MOVS, dl, Tys, Ops);
}

/// Emit a single REP MOVSB instruction for a particular constant size.
static SDValue emitRepmovsB(const X86Subtarget &Subtarget, SelectionDAG &DAG,
                            const SDLoc &dl, SDValue Chain, SDValue Dst,
                            SDValue Src, uint64_t Size) {
  return emitRepmovs(Subtarget, DAG, dl, Chain, Dst, Src,
                     DAG.getIntPtrConstant(Size, dl), MVT::i8);
}

/// Returns the best type to use with repmovs depending on alignment.
static MVT getOptimalRepmovsType(const X86Subtarget &Subtarget,
                                 Align Alignment) {
  uint64_t Align = Alignment.value();
  assert((Align != 0) && "Align is normalized");
  assert(isPowerOf2_64(Align) && "Align is a power of 2");
  switch (Align) {
  case 1:
    return MVT::i8;
  case 2:
    return MVT::i16;
  case 4:
    return MVT::i32;
  default:
    return Subtarget.is64Bit() ? MVT::i64 : MVT::i32;
  }
}

/// Returns a REP MOVS instruction, possibly with a few load/stores to implement
/// a constant size memory copy. In some cases where we know REP MOVS is
/// inefficient we return an empty SDValue so the calling code can either
/// generate a load/store sequence or call the runtime memcpy function.
static SDValue emitConstantSizeRepmov(
    SelectionDAG &DAG, const X86Subtarget &Subtarget, const SDLoc &dl,
    SDValue Chain, SDValue Dst, SDValue Src, uint64_t Size, EVT SizeVT,
    Align Alignment, bool isVolatile, bool AlwaysInline,
    MachinePointerInfo DstPtrInfo, MachinePointerInfo SrcPtrInfo) {

  /// TODO: Revisit next line: big copy with ERMSB on march >= haswell are very
  /// efficient.
  if (!AlwaysInline && Size > Subtarget.getMaxInlineSizeThreshold())
    return SDValue();

  /// If we have enhanced repmovs we use it.
  if (Subtarget.hasERMSB())
    return emitRepmovsB(Subtarget, DAG, dl, Chain, Dst, Src, Size);

  assert(!Subtarget.hasERMSB() && "No efficient RepMovs");
  /// We assume runtime memcpy will do a better job for unaligned copies when
  /// ERMS is not present.
  if (!AlwaysInline && (Alignment.value() & 3) != 0)
    return SDValue();

  const MVT BlockType = getOptimalRepmovsType(Subtarget, Alignment);
  const uint64_t BlockBytes = BlockType.getSizeInBits() / 8;
  const uint64_t BlockCount = Size / BlockBytes;
  const uint64_t BytesLeft = Size % BlockBytes;
  SDValue RepMovs =
      emitRepmovs(Subtarget, DAG, dl, Chain, Dst, Src,
                  DAG.getIntPtrConstant(BlockCount, dl), BlockType);

  /// RepMov can process the whole length.
  if (BytesLeft == 0)
    return RepMovs;

  assert(BytesLeft && "We have leftover at this point");

  /// In case we optimize for size we use repmovsb even if it's less efficient
  /// so we can save the loads/stores of the leftover.
  if (DAG.getMachineFunction().getFunction().hasMinSize())
    return emitRepmovsB(Subtarget, DAG, dl, Chain, Dst, Src, Size);

  // Handle the last 1 - 7 bytes.
  SmallVector<SDValue, 4> Results;
  Results.push_back(RepMovs);
  unsigned Offset = Size - BytesLeft;
  EVT DstVT = Dst.getValueType();
  EVT SrcVT = Src.getValueType();
  Results.push_back(DAG.getMemcpy(
      Chain, dl,
      DAG.getNode(ISD::ADD, dl, DstVT, Dst, DAG.getConstant(Offset, dl, DstVT)),
      DAG.getNode(ISD::ADD, dl, SrcVT, Src, DAG.getConstant(Offset, dl, SrcVT)),
      DAG.getConstant(BytesLeft, dl, SizeVT), Alignment, isVolatile,
      /*AlwaysInline*/ true, /*CI=*/nullptr, std::nullopt,
      DstPtrInfo.getWithOffset(Offset), SrcPtrInfo.getWithOffset(Offset)));
  return DAG.getNode(ISD::TokenFactor, dl, MVT::Other, Results);
}

SDValue X86SelectionDAGInfo::EmitTargetCodeForMemcpy(
    SelectionDAG &DAG, const SDLoc &dl, SDValue Chain, SDValue Dst, SDValue Src,
    SDValue Size, Align Alignment, bool isVolatile, bool AlwaysInline,
    MachinePointerInfo DstPtrInfo, MachinePointerInfo SrcPtrInfo) const {
  // If to a segment-relative address space, use the default lowering.
  if (DstPtrInfo.getAddrSpace() >= 256 || SrcPtrInfo.getAddrSpace() >= 256)
    return SDValue();

  // If the base registers conflict with our physical registers, use the default
  // lowering.
  const MCPhysReg ClobberSet[] = {X86::RCX, X86::RSI, X86::RDI,
                                  X86::ECX, X86::ESI, X86::EDI};
  if (isBaseRegConflictPossible(DAG, ClobberSet))
    return SDValue();

  const X86Subtarget &Subtarget =
      DAG.getMachineFunction().getSubtarget<X86Subtarget>();

  // Handle medium and large sizes for OpMove in Golang.
  if (TangoLLVM::isGoABIInternal(DAG.getMachineFunction().getFunction())) {
    ConstantSDNode *ConstantSize = dyn_cast<ConstantSDNode>(Size);
    if (ConstantSize) {
      uint64_t SizeVal = ConstantSize->getZExtValue();

      // Use duffcopy for medium sizes.
      // 1024 is a magic number, which can be tuned.
      if (SizeVal <= 1024) {
        ////////// AVX case:  See src/runtime/duff_amd64.s:duffcopy in Golang.
        // 897, 14 and 32 are magic constants.  897 is the offset to the first instruction of avx part.
        // 16 is the number of bytes to encode:
        //	VMOVUPS	(SI), Y0
        //	ADDQ	$16, SI
        //	VMOVUPS	Y0, (DI)
        //	ADDQ	$16, DI
        // and 32 is the number of such blocks.
        ////////// SSE case:  See src/runtime/duff_amd64.s:duffcopy in Golang.
        // 14 and 64 are magic constants.  14 is the number of bytes to encode:
        //	MOVUPS	(SI), X0
        //	ADDQ	$16, SI
        //	MOVUPS	X0, (DI)
        //	ADDQ	$16, DI
        // and 64 is the number of such blocks.
        const uint64_t dcBlocks    = Subtarget.hasAVX() || UseAVXForDuffcopy ? 32 : 64; // number of MOV/ADD blocks
        const uint64_t dcBlockSize = Subtarget.hasAVX() || UseAVXForDuffcopy ? 16 : 14; // size of instructions in a single block
        const uint64_t dcMoveBlock = Subtarget.hasAVX() || UseAVXForDuffcopy ? 32 : 16; // number of bytes moved by each block
        const uint64_t dcOffset    = Subtarget.hasAVX() || UseAVXForDuffcopy ? 14 * 64 + 1 : 0;
        const unsigned BytesLeft = SizeVal % dcMoveBlock;
        const unsigned AlignedSizeVal = SizeVal - BytesLeft;

        const uint64_t Blocks = AlignedSizeVal / dcMoveBlock;
        const uint64_t Offset = dcOffset + dcBlockSize * (dcBlocks - Blocks);

        TargetLowering::ArgListTy Args;
        TargetLowering::ArgListEntry Entry;
        Entry.Ty = PointerType::getUnqual(Type::getInt8Ty(*DAG.getContext()));
        Entry.Node = Dst; Args.push_back(Entry);
        Entry.Node = Src; Args.push_back(Entry);

        const TargetLowering &TLI = DAG.getTargetLoweringInfo();
        const char * Symbol = "runtime.duffcopy";
        auto *M = DAG.getMachineFunction().getFunction().getParent();
        auto *Fn = M->getFunction(Symbol);
        if (Fn == nullptr) {
          Fn = TangoLLVM::createGoFunction(FunctionType::get(Type::getVoidTy(*DAG.getContext()), {}, false),
                       Function::ExternalLinkage, Symbol, M);
        }
        auto FnPtrTy = TLI.getPointerTy(DAG.getDataLayout(), Fn->getAddressSpace());
        SDValue Callee = DAG.getTargetGlobalAddress(Fn, dl, FnPtrTy, Offset);

        // FIXME: pass in SDLoc
        TargetLowering::CallLoweringInfo CLI(DAG);
        CLI.setDebugLoc(dl)
          .setChain(Chain)
          .setCallee(TLI.getLibcallCallingConv(RTLIB::MEMCPY), // DI for Dst and SI for Src
                Dst.getValueType().getTypeForEVT(*DAG.getContext()),
                Callee, std::move(Args))
          .setDiscardResult();
        std::pair<SDValue,SDValue> CallResult = TLI.LowerCallTo(CLI);
        Chain =  CallResult.second;

        // Use inlined SIMD instructions for the remaining bytes if needed.
        if (BytesLeft) {
          const uint64_t Offset = SizeVal - BytesLeft;
          EVT DstVT = Dst.getValueType();
          EVT SrcVT = Src.getValueType();
          EVT SizeVT = Size.getValueType();
          Chain =
              DAG.getMemcpy(Chain, dl,
                            DAG.getNode(ISD::ADD, dl, DstVT, Dst, DAG.getConstant(Offset, dl, DstVT)),
                            DAG.getNode(ISD::ADD, dl, SrcVT, Src, DAG.getConstant(Offset, dl, DstVT)),
                            DAG.getConstant(BytesLeft, dl, SizeVT), Alignment, isVolatile,
                            /*AlwaysInline*/ true, /*isTailCall*/ nullptr, std::optional<bool>(false),
                            DstPtrInfo.getWithOffset(Offset), SrcPtrInfo.getWithOffset(Offset));
        }

        return Chain;
      }

      // Use repmovsb for large sizes assuming ERMSB/FSRM is popular.
      // TODO: Generate repmovsq for CPU without ERMSB/FSRM.
      return emitRepmovsB(Subtarget, DAG, dl, Chain, Dst, Src, SizeVal);
    }
  }

  // If enabled and available, use fast short rep mov.
  if (UseFSRMForMemcpy && Subtarget.hasFSRM())
    return emitRepmovs(Subtarget, DAG, dl, Chain, Dst, Src, Size, MVT::i8);

  /// Handle constant sizes,
  if (ConstantSDNode *ConstantSize = dyn_cast<ConstantSDNode>(Size))
    return emitConstantSizeRepmov(DAG, Subtarget, dl, Chain, Dst, Src,
                                  ConstantSize->getZExtValue(),
                                  Size.getValueType(), Alignment, isVolatile,
                                  AlwaysInline, DstPtrInfo, SrcPtrInfo);

  return SDValue();
}
