//===-- Cpu0ISelDAGToDAG.cpp - A DAG to DAG Inst Selector for Cpu0 -*- C++
//-*-===//
//
//                    The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines an instruction selector for the Cpu0 target.
//
//===----------------------------------------------------------------------===//

#include "Cpu0ISelDAGToDAG.h"
#include "Cpu0.h"

#include "Cpu0MachineFunctionInfo.h"
#include "Cpu0RegisterInfo.h"
#include "Cpu0SEISelDAGToDAG.h"
#include "Cpu0TargetMachine.h"

#include "llvm/CodeGen/MachineConstantPool.h"
#include "llvm/CodeGen/MachineFrameInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstrBuilder.h"
#include "llvm/CodeGen/MachineRegisterInfo.h"
#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/CodeGen/SelectionDAGNodes.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/GlobalValue.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"

using namespace llvm;

#define DEBUG_TYPE "cpu0-isel"

//===----------------------------------------------------------------------===//
// Instruction Selector Implementation
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// CPU0 specific code to select CPU0 machine instructions
// for SelectionDAG operations.
//===----------------------------------------------------------------------===//
bool Cpu0DAGToDAGISel::runOnMachineFunction(MachineFunction &MF) {
  return SelectionDAGISel::runOnMachineFunction(MF);
}

// Complex Pattern used on Cpu0InstrInfo
// Used on Cpu0 Load/Store instructions
bool Cpu0DAGToDAGISel::SelectAddr(SDNode *Parent, SDValue Addr, SDValue &Base,
                                  SDValue &Offset) {
  EVT ValTy = Addr.getValueType();
  SDLoc DL(Addr);

  // If Parent is an unaligned f32 load or store, select a (base + index)
  // float point load/store instruction (luxcl or suxcl)
  const LSBaseSDNode *LS = NULL;
  if (Parent && (LS = dyn_cast_or_null<LSBaseSDNode>(Parent))) {
    EVT VT = LS->getMemoryVT();
    if (VT.getSizeInBits() / 8 > LS->getAlignment()) {
      assert(false && "Unaligned loads/stores not supported for this type.");
      if (VT == MVT::f32)
        return false;
    }
  }

  if (auto *FIN = dyn_cast_or_null<FrameIndexSDNode>(Addr)) {
    Base = CurDAG->getTargetFrameIndex(FIN->getIndex(), MVT::i32);
    Offset = CurDAG->getTargetConstant(0, DL, MVT::i32);
    return true;
  }

  // on PIC code Load GA
  if (Addr.getOpcode() == Cpu0ISD::Wrapper) {
    Base = Addr.getOperand(0);
    Offset = Addr.getOperand(1);
    return true;
  }

  // static
  if (TM.getRelocationModel() != Reloc::PIC_)
    if ((Addr.getOpcode() == ISD::TargetExternalSymbol) ||
        (Addr.getOpcode() == ISD::TargetGloablAddress))
      return true;

  Base = Addr;
  Offset = CurDAG->getTargetConstant(0, DL, MVT::i32);
  return true;
}

void Cpu0DAGToDAGISel::Select(SDNode *N) {
  // If we have a custom node, we have already selected
  if (N->isMachineOpcode()) {
    LLVM_DEBUG(errs() << "== "; N->dump(CurDAG); errs() << "\n");
    N->setNodeId(-1);
    return;
  }

  // See if subclasses (e.g. SEISelDAGToDAG) can handle this node.
  if (trySelect(N))
    return;

  // To handle the some SDNodes
  unsigned OpCode = N->getOpcode();
  switch (OpCode) {
  default:
    break;
  case ISD::GLOBAL_OFFSET_TABLE:
    ReplaceNode(Node, getGlobalBaseReg());
    return;
  }

  // fall through to the match table
  SelectCode(N);
}
