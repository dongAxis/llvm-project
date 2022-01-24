//===-- Cpu0ISelDAGToDAG.h - A DAG to DAG Inst Selector for Cpu0 -*- C++
//-*-===//
//
//                    The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file defines an instruction selector for the CPU0 target.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_LIB_TARGET_CPU0_CPU0ISELDAGTODAG_H
#define LLVM_LIB_TARGET_CPU0_CPU0ISELDAGTODAG_H

#include "Cpu0.h"
#include "Cpu0Subtarget.h"
#include "Cpu0TargetMachine.h"

#include "llvm/CodeGen/SelectionDAGISel.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/Debug.h"

//===----------------------------------------------------------------------===//
// Instruction Selector Implementation
//===----------------------------------------------------------------------===//
namespace llvm {

class Cpu0DAGToDAGISel : public SelectionDAGISel {
public:
  explicit Cpu0DAGToDAGISel(TargetMachine &tm,
                            CodeGenOpt::Level OL = CodeGenOpt::Default)
      : SelectionDAGISel(tm, OL), Subtarget(nullptr) {}

  bool runOnMachineFunction(MachineFunction &MF) override;

  StringRef getPassName() const override {
    return "CPU0 DAG to DAG Pattern Instruction Selection";
  }

protected:
  // Keep a pointer to the Cpu0Subtarget around so that we can make the right
  // decision when generating code for different targets.
  const Cpu0Subtarget *Subtarget;

  SDNode *getGlobalBaseReg();

private:
#include "Cpu0GenDAGISel.inc"

  void Select(SDNode *N) override;
  virtual bool trySelect(SDNode *N) = 0;

  bool SelectAddr(SDNode *Parent, SDValue Addr, SDValue &Base, SDValue &Offset);
  inline SDValue getImm(const SDNode *Node, unsigned Imm) {
    return CurDAG->getTargetConstant(Imm, SDLoc(Node), Node->getValueType(0));
  }
  virtual void processFunctionAfterISel(MachineFunction &MF) = 0;
};

} // namespace llvm

#endif
