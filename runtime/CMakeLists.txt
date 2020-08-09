cmake_minimum_required(VERSION 3.4.3)
project(helix-runtime)

add_library(HelixRT STATIC helix.cpp)
target_include_directories(HelixRT PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
