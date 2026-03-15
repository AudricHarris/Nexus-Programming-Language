cmake_minimum_required(VERSION 3.20)

project(NexusCompiler
    VERSION 0.1.3
    DESCRIPTION "Compiler for the Nexus programming language"
    LANGUAGES C CXX
)

# -----------------------------
# C++ Settings
# -----------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Prefer static LLVM
set(LLVM_LINK_LLVM_DYLIB OFF)
set(BUILD_SHARED_LIBS OFF)

# -----------------------------
# Find LLVM
# -----------------------------
set(LLVM_ENABLE_ZSTD OFF CACHE BOOL "" FORCE)
find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# -----------------------------
# Source Files
# -----------------------------
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

# -----------------------------
# Executable
# -----------------------------
add_executable(nexus ${SOURCES})

target_include_directories(nexus
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# -----------------------------
# LLVM Components
# -----------------------------
llvm_map_components_to_libnames(LLVM_LIBS
    Core
    Support
    IRReader
    BitWriter
    Target
    X86
    X86CodeGen
)

# Link LLVM statically
target_link_libraries(nexus
    PRIVATE
        ${LLVM_LIBS}
)

target_include_directories(nexus
    SYSTEM PRIVATE
    ${LLVM_INCLUDE_DIRS}
)

target_compile_definitions(nexus
    PRIVATE
    ${LLVM_DEFINITIONS}
)

# -----------------------------
# Compiler Warnings
# -----------------------------
target_compile_options(nexus
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
)

# -----------------------------
# Debug / Release Flags
# -----------------------------
target_compile_options(nexus
    PRIVATE
        $<$<CONFIG:Debug>:-g -O0>
        $<$<CONFIG:Release>:-O2 -DNDEBUG>
)

# -----------------------------
# Installation
# -----------------------------
install(TARGETS nexus
    DESTINATION bin
)

# -----------------------------
# Testing
# -----------------------------
enable_testing()

add_test(
    NAME Smoke
    COMMAND nexus --help
)
