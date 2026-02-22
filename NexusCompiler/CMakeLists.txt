cmake_minimum_required(VERSION 3.20)

project(NexusCompiler
    VERSION 0.1.3
    DESCRIPTION "Compiler for the Nexus programming language"
    LANGUAGES CXX C
)

enable_language(C) 

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "src/*.cpp"
    "src/*.h"
)

add_executable(nexus ${SOURCES})

target_include_directories(nexus
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

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

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

llvm_map_components_to_libnames(LLVM_LIBS
    Core
    Support
    IRReader
    BitWriter
    Target
    X86CodeGen
)

target_link_libraries(nexus
    PRIVATE
        LLVM
)   

target_include_directories(nexus SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
target_compile_definitions(nexus PRIVATE ${LLVM_DEFINITIONS})

target_compile_options(nexus
    PRIVATE
        $<$<CONFIG:Debug>:-g -O0>
        $<$<CONFIG:Release>:-O2 -DNDEBUG>
)

install(TARGETS nexus
    DESTINATION bin
)

enable_testing()
add_test(NAME Smoke COMMAND nexus --help)
