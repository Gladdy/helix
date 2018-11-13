set(LLVM_NO_RTTI 1)

# Use an object-library to add the same files to multiple libs without requiring
# the sources them to be recompiled for each of them.
add_library(HelixCore OBJECT
  Transform/LoopPrologueSplitter.cpp
  )
set_target_properties(HelixCore PROPERTIES FOLDER "Helix")

add_helix_library(Helix $<TARGET_OBJECTS:HelixCore>)

target_link_libraries(Helix PUBLIC
  LLVMSupport
  LLVMCore
  LLVMScalarOpts
  LLVMInstCombine
  LLVMTransformUtils
  LLVMAnalysis
  LLVMipo
  LLVMMC
  LLVMPasses
  LLVMLinker
  LLVMIRReader
  LLVMBitReader
  LLVMMCParser
  LLVMObject
  LLVMProfileData
  LLVMTarget
  LLVMVectorize
)

# LLVM's/clang's "-load" option.
add_helix_loadable_module(LLVMHelix
  Helix.cpp
  $<TARGET_OBJECTS:HelixCore>
)

set_target_properties(LLVMHelix
  PROPERTIES
  LINKER_LANGUAGE CXX
  PREFIX "")

if (TARGET intrinsics_gen)
  # Check if we are building as part of an LLVM build
  add_dependencies(HelixCore intrinsics_gen)
endif()

