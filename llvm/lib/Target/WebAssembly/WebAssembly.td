//- WebAssembly.td - Describe the WebAssembly Target Machine --*- tablegen -*-//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This is a target description file for the WebAssembly architecture,
/// which is also known as "wasm".
///
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Target-independent interfaces which we are implementing
//===----------------------------------------------------------------------===//

include "llvm/Target/Target.td"

//===----------------------------------------------------------------------===//
// WebAssembly Subtarget features.
//===----------------------------------------------------------------------===//

def FeatureSIMD128 : SubtargetFeature<"simd128", "SIMDLevel", "SIMD128",
                                      "Enable 128-bit SIMD">;

def FeatureRelaxedSIMD : SubtargetFeature<"relaxed-simd", "SIMDLevel", "RelaxedSIMD",
                                      "Enable relaxed-simd instructions">;

def FeatureAtomics : SubtargetFeature<"atomics", "HasAtomics", "true",
                                      "Enable Atomics">;

def FeatureNontrappingFPToInt :
      SubtargetFeature<"nontrapping-fptoint",
                       "HasNontrappingFPToInt", "true",
                       "Enable non-trapping float-to-int conversion operators">;

def FeatureSignExt :
      SubtargetFeature<"sign-ext",
                       "HasSignExt", "true",
                       "Enable sign extension operators">;

def FeatureExceptionHandling :
      SubtargetFeature<"exception-handling", "HasExceptionHandling", "true",
                       "Enable Wasm exception handling">;

def FeatureBulkMemory :
      SubtargetFeature<"bulk-memory", "HasBulkMemory", "true",
                       "Enable bulk memory operations">;

def FeatureExtendedConst :
      SubtargetFeature<"extended-const", "HasExtendedConst", "true",
                       "Enable extended const expressions">;

//===----------------------------------------------------------------------===//
// Architectures.
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Architectures.
//===----------------------------------------------------------------------===//
include "WebAssemblyCallingConv.td"

//===----------------------------------------------------------------------===//
// Register File Description
//===----------------------------------------------------------------------===//

include "WebAssemblyRegisterInfo.td"

//===----------------------------------------------------------------------===//
// Instruction Descriptions
//===----------------------------------------------------------------------===//

include "WebAssemblyInstrInfo.td"

def WebAssemblyInstrInfo : InstrInfo;

//===----------------------------------------------------------------------===//
// WebAssembly Processors supported.
//===----------------------------------------------------------------------===//

// Minimal Viable Product.
def : ProcessorModel<"mvp", NoSchedModel, []>;

// Generic processor: latest stable version.
def : ProcessorModel<"generic", NoSchedModel, []>;

//===----------------------------------------------------------------------===//
// Target Declaration
//===----------------------------------------------------------------------===//

def WebAssemblyAsmParser : AsmParser {
  // The physical register names are not in the binary format or asm text
  let ShouldEmitMatchRegisterName = 0;
}

def WebAssemblyAsmWriter : AsmWriter {
  string AsmWriterClassName  = "InstPrinter";
  int PassSubtarget = 0;
  int Variant = 0;
  bit isMCAsmWriter = 1;
}

def WebAssembly : Target {
  let InstructionSet = WebAssemblyInstrInfo;
  let AssemblyParsers  = [WebAssemblyAsmParser];
  let AssemblyWriters = [WebAssemblyAsmWriter];
}
