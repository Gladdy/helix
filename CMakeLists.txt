project(HELIX)
cmake_minimum_required(VERSION 3.4.3)

# Where is LLVM installed?
find_package(LLVM CONFIG REQUIRED)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${LLVM_CMAKE_DIR})
include(HandleLLVMOptions)
include(AddLLVM)

# Add the llvm header path.
include_directories(${LLVM_INCLUDE_DIRS})

# Sources available, too?
execute_process(COMMAND "${LLVM_TOOLS_BINARY_DIR}/llvm-config" --src-root OUTPUT_VARIABLE MAIN_SRC_DIR OUTPUT_STRIP_TRAILING_WHITESPACE)

set(LLVM_SOURCE_ROOT ${MAIN_SRC_DIR} CACHE PATH "Path to LLVM source tree")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include("helix_macros")

set(HELIX_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(HELIX_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})

# Add appropriate flags for GCC
if (CMAKE_COMPILER_IS_GNUCXX)
  # FIXME: Turn off exceptions, RTTI:
  # -fno-exceptions -fno-rtti
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-common -Woverloaded-virtual -Wno-long-long -Wall -W -Wno-unused-parameter -Wwrite-strings -fno-exceptions -fno-rtti")
elseif (MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHs-c-")
  add_definitions("-D_HAS_EXCEPTIONS=0")
else ()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions -fno-rtti")
endif ()

# Add path for custom modules
# set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${HELIX_SOURCE_DIR}/cmake")

# SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

include_directories(
  BEFORE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  )

if (NOT LLVM_INSTALL_TOOLCHAIN_ONLY)
  install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING
    PATTERN "*.h"
    PATTERN ".svn" EXCLUDE
    )

  install(DIRECTORY ${HELIX_BINARY_DIR}/include/
    DESTINATION include
    FILES_MATCHING
    PATTERN "*.h"
    PATTERN "CMakeFiles" EXCLUDE
    PATTERN ".svn" EXCLUDE
    )
endif()

add_definitions( -D_GNU_SOURCE )

#add_subdirectory(docs)
add_subdirectory(lib)
#add_subdirectory(test)
#add_subdirectory(unittests)
#add_subdirectory(tools)
#add_subdirectory(cmake)
