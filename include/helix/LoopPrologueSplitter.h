//===- LoopRotation.h - Loop Rotation -------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file provides the interface for the Loop Rotation pass.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TRANSFORMS_HELIX_PROLOGUE_H
#define LLVM_TRANSFORMS_HELIX_PROLOGUE_H

#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Transforms/Scalar/LoopPassManager.h"

namespace llvm {

/// A simple loop rotation transformation.
class LoopPrologueSplitter : public PassInfoMixin<LoopPrologueSplitter> {
public:
  LoopPrologueSplitter();
  PreservedAnalyses run(Loop &L, LoopAnalysisManager &AM,
                        LoopStandardAnalysisResults &AR, LPMUpdater &U);
};
}

#endif // LLVM_TRANSFORMS_HELIX_PROLOGUE_H
