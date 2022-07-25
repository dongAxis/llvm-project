//===-- WebAssemblyFrameLowering.cpp - WebAssembly Frame Lowering ----------==//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the WebAssembly implementation of
/// TargetFrameLowering class.
///
/// On WebAssembly, there aren't a lot of things to do here. There are no
/// callee-saved registers to save, and no spill slots.
///
/// The stack grows downward.
///
//===----------------------------------------------------------------------===//

#include "WebAssemblyFrameLowering.h"
#include "MCTargetDesc/WebAssemblyMCTargetDesc.h"
#include "Utils/WebAssemblyTypeUtilities.h"
#include "WebAssembly.h"
#include "WebAssemblyInstrInfo.h"
#include "WebAssemblyMachineFunctionInfo.h"
#include "WebAssemblySubtarget.h"
#include "WebAssemblyTargetMachine.h"
#include "llvm/CodeGen/Analysis.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineModuleInfoImpls.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/IR/Instructions.h"
#include "llvm/MC/MCAsmInfo.h"
#include "llvm/Support/Debug.h"
using namespace llvm;

#define DEBUG_TYPE "wasm-frame-info"

// TODO: wasm64
// TODO: Emit TargetOpcode::CFI_INSTRUCTION instructions

// In an ideal world, when objects are added to the MachineFrameInfo by
// FunctionLoweringInfo::set, we could somehow hook into target-specific code to
// ensure they are assigned the right stack ID.  However there isn't a hook that
// runs between then and DAG building time, though, so instead we hoist stack
// objects lazily when they are first used, and comprehensively after the DAG is
// built via the PreprocessISelDAG hook, called by the
// SelectionDAGISel::runOnMachineFunction.  We have to do it in two places
// because we want to do it while building the selection DAG for uses of alloca,
// but not all alloca instructions are used so we have to follow up afterwards.
Optional<unsigned>
WebAssemblyFrameLowering::getLocalForStackObject(MachineFunction &MF,
                                                 int FrameIndex) {
  MachineFrameInfo &MFI = MF.getFrameInfo();

  // If already hoisted to a local, done.
  if (MFI.getStackID(FrameIndex) == TargetStackID::WasmLocal)
    return static_cast<unsigned>(MFI.getObjectOffset(FrameIndex));

  // If not allocated in the object address space, this object will be in
  // linear memory.
  const AllocaInst *AI = MFI.getObjectAllocation(FrameIndex);
  if (!AI ||
      !WebAssembly::isWasmVarAddressSpace(AI->getType()->getAddressSpace()))
    return None;

  // Otherwise, allocate this object in the named value stack, outside of linear
  // memory.
  SmallVector<EVT, 4> ValueVTs;
  const WebAssemblyTargetLowering &TLI =
      *MF.getSubtarget<WebAssemblySubtarget>().getTargetLowering();
  WebAssemblyFunctionInfo *FuncInfo = MF.getInfo<WebAssemblyFunctionInfo>();
  ComputeValueVTs(TLI, MF.getDataLayout(), AI->getAllocatedType(), ValueVTs);
  MFI.setStackID(FrameIndex, TargetStackID::WasmLocal);
  // Abuse SP offset to record the index of the first local in the object.
  unsigned Local = FuncInfo->getParams().size() + FuncInfo->getLocals().size();
  MFI.setObjectOffset(FrameIndex, Local);
  // Allocate WebAssembly locals for each non-aggregate component of the
  // allocation.
  for (EVT ValueVT : ValueVTs)
    FuncInfo->addLocal(ValueVT.getSimpleVT());
  // Abuse object size to record number of WebAssembly locals allocated to
  // this object.
  MFI.setObjectSize(FrameIndex, ValueVTs.size());
  return static_cast<unsigned>(Local);
}

/// We need a base pointer in the case of having items on the stack that
/// require stricter alignment than the stack pointer itself.  Because we need
/// to shift the stack pointer by some unknown amount to force the alignment,
/// we need to record the value of the stack pointer on entry to the function.
bool WebAssemblyFrameLowering::hasBP(const MachineFunction &MF) const {
  const auto *RegInfo =
      MF.getSubtarget<WebAssemblySubtarget>().getRegisterInfo();
  return RegInfo->hasStackRealignment(MF);
}

/// Return true if the specified function should have a dedicated frame pointer
/// register.
bool WebAssemblyFrameLowering::hasFP(const MachineFunction &MF) const {
  const MachineFrameInfo &MFI = MF.getFrameInfo();

  // When we have var-sized objects, we move the stack pointer by an unknown
  // amount, and need to emit a frame pointer to restore the stack to where we
  // were on function entry.
  // If we already need a base pointer, we use that to fix up the stack pointer.
  // If there are no fixed-size objects, we would have no use of a frame
  // pointer, and thus should not emit one.
  bool HasFixedSizedObjects = MFI.getStackSize() > 0;
  bool NeedsFixedReference = !hasBP(MF) || HasFixedSizedObjects;

  return MFI.isFrameAddressTaken() ||
         (MFI.hasVarSizedObjects() && NeedsFixedReference) ||
         MFI.hasStackMap() || MFI.hasPatchPoint();
}

/// Under normal circumstances, when a frame pointer is not required, we reserve
/// argument space for call sites in the function immediately on entry to the
/// current function. This eliminates the need for add/sub sp brackets around
/// call sites. Returns true if the call frame is included as part of the stack
/// frame.
bool WebAssemblyFrameLowering::hasReservedCallFrame(
    const MachineFunction &MF) const {
  return !MF.getFrameInfo().hasVarSizedObjects();
}

// Returns true if this function needs a local user-space stack pointer for its
// local frame (not for exception handling).
bool WebAssemblyFrameLowering::needsSPForLocalFrame(
    const MachineFunction &MF) const {
  auto &MFI = MF.getFrameInfo();
  return MFI.getStackSize() || MFI.adjustsStack() || hasFP(MF);
}

// In function with EH pads, we need to make a copy of the value of
// __stack_pointer global in SP32/64 register, in order to use it when
// restoring __stack_pointer after an exception is caught.
bool WebAssemblyFrameLowering::needsPrologForEH(
    const MachineFunction &MF) const {
  auto EHType = MF.getTarget().getMCAsmInfo()->getExceptionHandlingType();
  return EHType == ExceptionHandling::Wasm &&
         MF.getFunction().hasPersonalityFn() && MF.getFrameInfo().hasCalls();
}

/// Returns true if this function needs a local user-space stack pointer.
/// Unlike a machine stack pointer, the wasm user stack pointer is a global
/// variable, so it is loaded into a register in the prolog.
bool WebAssemblyFrameLowering::needsSP(const MachineFunction &MF) const {
  return true;
}

/// Returns true if the local user-space stack pointer needs to be written back
/// to __stack_pointer global by this function (this is not meaningful if
/// needsSP is false). If false, the stack red zone can be used and only a local
/// SP is needed.
bool WebAssemblyFrameLowering::needsSPWriteback(
    const MachineFunction &MF) const {
    return true;
}

unsigned WebAssemblyFrameLowering::getSPReg(const MachineFunction &MF) {
  return MF.getSubtarget<WebAssemblySubtarget>().hasAddr64()
             ? WebAssembly::SP64
             : WebAssembly::SP32;
}

unsigned WebAssemblyFrameLowering::getFPReg(const MachineFunction &MF) {
  return MF.getSubtarget<WebAssemblySubtarget>().hasAddr64()
             ? WebAssembly::FP64
             : WebAssembly::FP32;
}

unsigned WebAssemblyFrameLowering::getOpcConst(const MachineFunction &MF) {
  return MF.getSubtarget<WebAssemblySubtarget>().hasAddr64()
             ? WebAssembly::CONST_I64
             : WebAssembly::CONST_I32;
}

unsigned WebAssemblyFrameLowering::getOpcAdd(const MachineFunction &MF) {
  return MF.getSubtarget<WebAssemblySubtarget>().hasAddr64()
             ? WebAssembly::ADD_I64
             : WebAssembly::ADD_I32;
}

unsigned WebAssemblyFrameLowering::getOpcSub(const MachineFunction &MF) {
  return MF.getSubtarget<WebAssemblySubtarget>().hasAddr64()
             ? WebAssembly::SUB_I64
             : WebAssembly::SUB_I32;
}

unsigned WebAssemblyFrameLowering::getOpcImmSub(const MachineFunction &MF) {
  return MF.getSubtarget<WebAssemblySubtarget>().hasAddr64()
             ? WebAssembly::SUB_IMM_I64
             : WebAssembly::SUB_IMM_I32;
}

unsigned WebAssemblyFrameLowering::getOpcAnd(const MachineFunction &MF) {
  return MF.getSubtarget<WebAssemblySubtarget>().hasAddr64()
             ? WebAssembly::AND_I64
             : WebAssembly::AND_I32;
}

unsigned WebAssemblyFrameLowering::getOpcImmAnd(const MachineFunction &MF) {
  return MF.getSubtarget<WebAssemblySubtarget>().hasAddr64()
             ? WebAssembly::AND_I64
             : WebAssembly::AND_I32;
}

unsigned WebAssemblyFrameLowering::getOpcGlobGet(const MachineFunction &MF) {
  return MF.getSubtarget<WebAssemblySubtarget>().hasAddr64()
             ? WebAssembly::GLOBAL_GET_I64
             : WebAssembly::GLOBAL_GET_I32;
}

unsigned WebAssemblyFrameLowering::getOpcGlobSet(const MachineFunction &MF) {
  return MF.getSubtarget<WebAssemblySubtarget>().hasAddr64()
             ? WebAssembly::GLOBAL_SET_I64
             : WebAssembly::GLOBAL_SET_I32;
}

void WebAssemblyFrameLowering::writeSPToGlobal(
    unsigned SrcReg, MachineFunction &MF, MachineBasicBlock &MBB,
    MachineBasicBlock::iterator &InsertStore, const DebugLoc &DL) const {
  const auto *TII = MF.getSubtarget<WebAssemblySubtarget>().getInstrInfo();

  const char *ES = "__stack_pointer";
  auto *SPSymbol = MF.createExternalSymbolName(ES);

  BuildMI(MBB, InsertStore, DL, TII->get(getOpcGlobSet(MF)))
      .addExternalSymbol(SPSymbol)
      .addReg(SrcReg);
}

MachineBasicBlock::iterator
WebAssemblyFrameLowering::eliminateCallFramePseudoInstr(
    MachineFunction &MF, MachineBasicBlock &MBB,
    MachineBasicBlock::iterator I) const {
  assert(!I->getOperand(0).getImm() && (hasFP(MF) || hasBP(MF)) &&
         "Call frame pseudos should only be used for dynamic stack adjustment");
  auto &ST = MF.getSubtarget<WebAssemblySubtarget>();
  const auto *TII = ST.getInstrInfo();
  if (I->getOpcode() == TII->getCallFrameDestroyOpcode() &&
      needsSPWriteback(MF)) {
    DebugLoc DL = I->getDebugLoc();
    writeSPToGlobal(getSPReg(MF), MF, MBB, I, DL);
  }
  return MBB.erase(I);
}

void WebAssemblyFrameLowering::emitPrologue(MachineFunction &MF,
                                            MachineBasicBlock &MBB) const {
  // TODO: Do ".setMIFlag(MachineInstr::FrameSetup)" on emitted instructions
  auto &MFI = MF.getFrameInfo();
  assert(MFI.getCalleeSavedInfo().empty() &&
         "WebAssembly should not have callee-saved registers");

  if (!needsSP(MF))
    return;
  uint64_t StackSize = MFI.getStackSize();

  auto &ST = MF.getSubtarget<WebAssemblySubtarget>();
  const auto *TII = ST.getInstrInfo();
  auto &MRI = MF.getRegInfo();

  auto InsertPt = MBB.begin();
  while (InsertPt != MBB.end() &&
         WebAssembly::isArgument(InsertPt->getOpcode()))
    ++InsertPt;
  DebugLoc DL;

  const TargetRegisterClass *PtrRC =
      MRI.getTargetRegisterInfo()->getPointerRegClass(MF);
  unsigned SPReg = getSPReg(MF);
  bool HasBP = hasBP(MF);
  if (HasBP) {
    BuildMI(MBB, InsertPt, DL, TII->get(WebAssembly::COPY_I64), getFPReg(MF))
        .addReg(getSPReg(MF));
  }

  if (StackSize)
    BuildMI(MBB, InsertPt, DL, TII->get(getOpcImmSub(MF)), getSPReg(MF))
        .addReg(SPReg)
        .addImm(StackSize);

  if (HasBP) {
    Align Alignment = MFI.getMaxAlign();
    BuildMI(MBB, InsertPt, DL, TII->get(getOpcImmAnd(MF)), getSPReg(MF))
        .addReg(getSPReg(MF))
        .addImm((int64_t) ~(Alignment.value() - 1));
  }
}

void WebAssemblyFrameLowering::emitEpilogue(MachineFunction &MF,
                                            MachineBasicBlock &MBB) const {
  uint64_t StackSize = MF.getFrameInfo().getStackSize();
  if (!needsSP(MF) || !needsSPWriteback(MF))
    return;
  auto &ST = MF.getSubtarget<WebAssemblySubtarget>();
  const auto *TII = ST.getInstrInfo();
  auto &MRI = MF.getRegInfo();
  auto InsertPt = MBB.getFirstTerminator();
  DebugLoc DL;
  
  if (InsertPt != MBB.end())
    DL = InsertPt->getDebugLoc();

  // Restore the stack pointer. If we had fixed-size locals, add the offset
  // subtracted in the prolog.
  unsigned SPReg = 0;
  unsigned SPFPReg = hasFP(MF) ? getFPReg(MF) : getSPReg(MF);
  if (hasBP(MF)) {
    BuildMI(MBB, InsertPt, DL, TII->get(WebAssembly::COPY_I64), getSPReg(MF))
        .addReg(getFPReg(MF));
  } else {
    BuildMI(MBB, InsertPt, DL, TII->get(getOpcAdd(MF)), getSPReg(MF))
        .addReg(getFPReg(MF))
        .addImm(StackSize);
  }
}

bool WebAssemblyFrameLowering::isSupportedStackID(
    TargetStackID::Value ID) const {
  // Use the Object stack for WebAssembly locals which can only be accessed
  // by name, not via an address in linear memory.
  if (ID == TargetStackID::WasmLocal)
    return true;

  return TargetFrameLowering::isSupportedStackID(ID);
}

TargetFrameLowering::DwarfFrameBase
WebAssemblyFrameLowering::getDwarfFrameBase(const MachineFunction &MF) const {
  DwarfFrameBase Loc;
  Loc.Kind = DwarfFrameBase::WasmFrameBase;
  const WebAssemblyFunctionInfo &MFI = *MF.getInfo<WebAssemblyFunctionInfo>();
  if (needsSP(MF) && MFI.isFrameBaseVirtual()) {
    unsigned LocalNum = MFI.getFrameBaseLocal();
    Loc.Location.WasmLoc = {WebAssembly::TI_LOCAL, LocalNum};
  } else {
    // TODO: This should work on a breakpoint at a function with no frame,
    // but probably won't work for traversing up the stack.
    Loc.Location.WasmLoc = {WebAssembly::TI_GLOBAL_RELOC, 0};
  }
  return Loc;
}
