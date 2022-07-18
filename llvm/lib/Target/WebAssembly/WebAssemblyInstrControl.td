//===- WebAssemblyInstrControl.td-WebAssembly control-flow ------*- tablegen -*-
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// WebAssembly control-flow code-gen constructs.
///
//===----------------------------------------------------------------------===//

let isBranch = 1, isTerminator = 1, hasCtrlDep = 1 in {
// The condition operand is a boolean value which WebAssembly represents as i32.
defm BR_IF : I<(outs), (ins bb_op:$dst, I32:$cond),
               [(brcond I32:$cond, bb:$dst)],
                "br_if   \t$dst, $cond", 0x0d>;
let isCodeGenOnly = 1 in
defm BR_UNLESS : I<(outs), (ins bb_op:$dst, I32:$cond),
                   []>;
let isBarrier = 1 in
defm BR   : I<(outs), (ins bb_op:$dst),
                [(br bb:$dst)],
                "br      \t$dst", 0x0c>;
} // isBranch = 1, isTerminator = 1, hasCtrlDep = 1

def : Pat<(brcond (i32 (setne I32:$cond, 0)), bb:$dst),
          (BR_IF bb_op:$dst, I32:$cond)>;
def : Pat<(brcond (i32 (seteq I32:$cond, 0)), bb:$dst),
          (BR_UNLESS bb_op:$dst, I32:$cond)>;
def : Pat<(brcond (i32 (xor bool_node:$cond, (i32 1))), bb:$dst),
          (BR_UNLESS bb_op:$dst, I32:$cond)>;

// A list of branch targets enclosed in {} and separated by comma.
// Used by br_table only.
def BrListAsmOperand : AsmOperandClass { let Name = "BrList"; }
let OperandNamespace = "WebAssembly", OperandType = "OPERAND_BRLIST" in
def brlist : Operand<i32> {
  let ParserMatchClass = BrListAsmOperand;
  let PrintMethod = "printBrList";
}

// Duplicating a BR_TABLE is almost never a good idea. In particular, it can
// lead to some nasty irreducibility due to tail merging when the br_table is in
// a loop.
let isTerminator = 1, hasCtrlDep = 1, isBarrier = 1, isNotDuplicable = 1 in {

defm BR_TABLE_I32 : I<(outs), (ins I32:$index, variable_ops),
                      [(WebAssemblybr_table I32:$index)],
                      "br_table \t$index",
                      0x0e>;
// TODO: SelectionDAG's lowering insists on using a pointer as the index for
// jump tables, so in practice we don't ever use BR_TABLE_I64 in wasm32 mode
// currently.
defm BR_TABLE_I64 : I<(outs), (ins I64:$index, variable_ops),
                      [(WebAssemblybr_table I64:$index)],
                      "br_table \t$index",
                      0x0e>;
} // isTerminator = 1, hasCtrlDep = 1, isBarrier = 1, isNotDuplicable = 1

// This is technically a control-flow instruction, since all it affects is the
// IP.
defm NOP : I<(outs), (ins), [], "nop", 0x01>;

let hasCtrlDep = 1, isBarrier = 1 in {
let isTerminator = 1 in {
let isReturn = 1 in {

defm RETURN : I<(outs), (ins variable_ops),
                [(WebAssemblyreturn)],
                "return", 0x0f>;
// Equivalent to RETURN, for use at the end of a function when wasm
// semantics return by falling off the end of the block.
let isCodeGenOnly = 1 in
defm FALLTHROUGH_RETURN : I<(outs), (ins variable_ops), []>;

} // isReturn = 1

let IsCanonical = 1, isTrap = 1 in
defm UNREACHABLE : I<(outs), (ins), [(trap)], "unreachable", 0x00>;

} // isTerminator = 1

// debugtrap explicitly returns despite trapping because it is supposed to just
// get the attention of the debugger. Unfortunately, because UNREACHABLE is a
// terminator, lowering debugtrap to UNREACHABLE can create an invalid
// MachineBasicBlock when there is additional code after it. Lower it to this
// non-terminator version instead.
// TODO: Actually execute the debugger statement when running on the Web
let isTrap = 1 in
defm DEBUG_UNREACHABLE : I<(outs), (ins), [(debugtrap)], "unreachable", 0x00>;

} // hasCtrlDep = 1, isBarrier = 1

