//WebAssemblyRegisterInfo.td-Describe the WebAssembly Registers -*- tablegen -*-
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file describes the WebAssembly register classes and some nominal
/// physical registers.
///
//===----------------------------------------------------------------------===//

let Namespace = "WebAssembly" in {
  def sub_32 : SubRegIndex<32>;
}

class WebAssemblyReg<bits<16> Enc, string n> : Register<n> {
  let HWEncoding = Enc;
  let Namespace = "WebAssembly";
}

// Registers are identified with 4-bit ID numbers.
// Ri - 64-bit integer registers
class WebAssemblyReg64<bits<16> Enc, string n, list<Register> subregs>
  : RegisterWithSubRegs<n, subregs> {
  let HWEncoding = Enc;
  let Namespace = "WebAssembly";
  let SubRegIndices = [sub_32];
}

class WebAssemblyRegClass<list<ValueType> regTypes, int alignment, dag regList>
     : RegisterClass<"WebAssembly", regTypes, alignment, regList>;

//===----------------------------------------------------------------------===//
// Registers
//===----------------------------------------------------------------------===//

foreach I = 0-16 in {
  // 32-bit Integer (alias to low part of 64-bit register).
  def W#I  : WebAssemblyReg<I,  "w"#I>,  DwarfRegNum<[I]>;
  // 64-bit Integer registers
  def R#I  : WebAssemblyReg64<I,  "r"#I,  [!cast<WebAssemblyReg>("W"#I)]>,  DwarfRegNum<[I]>;
  // float
  def F#I  : WebAssemblyReg<I,  "f"#I>,  DwarfRegNum<[I]>;
  // double 
  def D#I  : WebAssemblyReg64<I,  "d"#I, [!cast<WebAssemblyReg>("F"#I)]>,  DwarfRegNum<[I]>;
}

def FP32  :  WebAssemblyReg<100, "fp32">, DwarfRegNum<[100]>;
def FP64  :  WebAssemblyReg<200, "fp64">, DwarfRegNum<[200]>;
def SP32  :  WebAssemblyReg<101, "sp32">, DwarfRegNum<[101]>;
def SP64  :  WebAssemblyReg<201, "sp64">, DwarfRegNum<[201]>;

// Register classes.
def I32 : WebAssemblyRegClass<[i32], 64, (add
  (sequence "W%u", 1, 16),
  FP32,
  SP32
)>;

def I64 : WebAssemblyRegClass<[i64], 64, (add
  (sequence "R%u", 1, 9),
  FP64,
  SP64
)>;

def F32 : WebAssemblyRegClass<[f32], 64, (sequence "F%u", 1, 16)>;
def F64 : WebAssemblyRegClass<[f64], 64, (sequence "D%u", 1, 16)>;
