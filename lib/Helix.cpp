//===---------- Helix.cpp - --------------------------- -------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//===----------------------------------------------------------------------===//

#include "helix/RegisterPasses.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"

namespace {

class StaticInitializer {
public:
  StaticInitializer() {
    llvm::PassRegistry &Registry = *llvm::PassRegistry::getPassRegistry();
    helix::initializeHelixPasses(Registry);
  }
};
static StaticInitializer InitializeEverything;
} // end of anonymous namespace.
