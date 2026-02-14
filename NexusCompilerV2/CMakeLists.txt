cmake_minimum_required(VERSION 3.20)

project(NexusCompiler
    VERSION 0.1.0
    DESCRIPTION "This is a Compiler for the language nexus"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB_RECURSE SOURCES "src/*.cpp")

add_executable(NexusCompiler ${SOURCES})

target_compile_options(NexusCompiler PRIVATE
    -Wall -Wextra -Wpedantic
)

target_include_directories(NexusCompiler PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
