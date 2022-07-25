//===-- WebAssemblyCallingConv.td - Calling Conventions WebAssembly --------*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===--------------------------------------------------------------------------------------===//
//
// This describes the calling conventions for the BPF architecture.
//
//===-------------------------------------------------------------------------------------===//
def CC_WebAssembly : CallingConv<[
   CCIfType<[i8, i16], CCPromoteToType<i32>>,
   
   CCIfType<[i8, i16, i32, f32], CCAssignToStack<4, 4>>,
   
   CCIfType<[i64, f64], CCAssignToStack<8, 4>>
]>;

def RetCC_WebAssembly : CallingConv<[
   CCIfType<[i8, i16], CCPromoteToType<i32>>,

   CCIfType<[i8, i16, i32, f32], CCAssignToStack<4, 4>>,
   
   CCIfType<[i64, f64], CCAssignToStack<8, 4>>
]>;
