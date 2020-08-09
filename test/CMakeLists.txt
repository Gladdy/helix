find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

add_library(no_helix STATIC test_loops.cpp)
add_executable(test_no_helix test.cpp util.cpp )
target_link_libraries(test_no_helix no_helix ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES} pthread)

add_custom_command(
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/test/test_loops.ll"
  COMMAND clang++ "${CMAKE_CURRENT_SOURCE_DIR}/test_loops.cpp" -S -emit-llvm
  #WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/include/foo"
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/test_loops.cpp"
  COMMENT "Converting to llvm IR"
  VERBATIM
)

add_custom_target(test_loops_ll DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/test/test_loops.ll)

# add_custom_command(
#   OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/test/test_loops.ll"
#   COMMAND clang++ "${CMAKE_CURRENT_SOURCE_DIR}/test_loops.cpp" -S -emit-llvm
#   #WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/include/foo"
#   DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/test_loops.cpp"
#   COMMENT "Converting to llvm IR"
#   VERBATIM
# )


# add_library(with_helix STATIC test_loops.cpp)
add_executable(test_with_helix test.cpp util.cpp )
target_link_libraries(test_with_helix ${CMAKE_CURRENT_BINARY_DIR}/test_loops.ll ${GTEST_LIBRARIES} ${GTEST_MAIN_LIBRARIES} pthread)
