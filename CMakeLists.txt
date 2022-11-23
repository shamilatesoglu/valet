# :/ I know, right? It's a pity.
cmake_minimum_required(VERSION 3.22)

project(autob)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cxx"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hxx")

add_executable(autob ${SOURCES})
target_include_directories(autob PRIVATE "./third_party/")