# :/ I know, right?
cmake_minimum_required(VERSION 3.22)

project(valet)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if (APPLE)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "14.1")
endif()

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cxx"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hxx"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/**/*.cxx"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/**/*.hxx")

add_executable(valet ${SOURCES})
target_include_directories(valet PRIVATE "./third_party/" "./src/core/include" "./src/core/src")

# pthread
find_package(Threads REQUIRED)
target_link_libraries(valet PRIVATE Threads::Threads)