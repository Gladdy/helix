//===------ helix/RegisterPasses.h - Register the Helix passes *- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// Functions to register the Helix passes in a LLVM pass manager.
//
//===----------------------------------------------------------------------===//

#ifndef HELIX_REGISTER_PASSES_H
#define HELIX_REGISTER_PASSES_H

#include "llvm/IR/LegacyPassManager.h"

namespace llvm {
namespace legacy {
class PassManagerBase;
} // namespace legacy
} // namespace llvm

namespace helix {
void initializeHelixPasses(llvm::PassRegistry &Registry);
void registerHelixPasses(llvm::legacy::PassManagerBase &PM);
} // namespace helix 
#endif
