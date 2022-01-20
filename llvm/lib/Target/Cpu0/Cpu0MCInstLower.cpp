//===-- Cpu0MCInstLower.cpp - Convert Cpu0 MachineInstr to MCInst-*- C++
//-*-===//
//
//                    The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file contains code to lower Cpu0 MachineInstrs to their corresponding
// MCInst records.
//
//===----------------------------------------------------------------------===//

#include "Cpu0MCInstLower.h"

#include "Cpu0AsmPrinter.h"
#include "Cpu0InstrInfo.h"
#include "MCTargetDesc/Cpu0BaseInfo.h"
#include "llvm/CodeGen/MachineFunction.h"
#include "llvm/CodeGen/MachineInstr.h"
#include "llvm/IR/Mangler.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInst.h"

using namespace llvm;

Cpu0MCInstLower::Cpu0MCInstLower(Cpu0AsmPrinter &asmprinter)
    : AsmPrinter(asmprinter) {}

void Cpu0MCInstLower::Initialize(MCContext *C) { Ctx = C; }

static void CreateMCInst(MCInst &Inst, unsigned Opc, const MCOperand &Operand0,
                         const MCOperand &Operand1,
                         const MCOperand &Operand2 = MCOperand()) {
  Inst.setOpcode(Opc);
  Inst.addOperand(Operand0);
  Inst.addOperand(Operand1);
  if (Operand2.isValid())
    Inst.addOperand(Operand2);
}

void Cpu0MCInstLower::Lower(const MachineInstr *MI, MCInst &OutMI) const {
  unsigned Opc = MI->getOpcode();
  OutMI.setOpcode(Opc);

  for (uint64_t Idx = 0; Idx < MI->getNumOperands(); Idx++) {
    const MachineOperand &Operand = MI->getOperand(Idx);
    MCOperand OutOperand = LowerOperand(Operand);
    if (OutOperand.isValid())
      OutMI.addOperand(OutOperand);
  }
}

MCOperand Cpu0MCInstLower::LowerOperand(const MachineOperand &MO,
                                        unsigned offset) const {
  MachineOperandType MOTy = MO.getType();
  switch (MOTy) {
  default:
    llvm_unreachable("unknown operand type");
  case MachineOperand::MO_Register:
    // Ignore all implicit register operands
    if (MO.isImplicit())
      break;
    return MCOperand::createReg(MO.getReg());

  case MachineOperand::MO_Immediate:
    return MCOperand::createImm(MO.getImm() + offset);

  case MachineOperand::MO_RegisterMask:
    break;
  }
  return MCOperand();
}
