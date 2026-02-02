cmake_minimum_required(VERSION 3.20)
project(nexus LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

include_directories(${CMAKE_SOURCE_DIR}/external)

add_executable(nexus
	src/main.cpp
	src/lexer.cpp
	src/parser.cpp
	src/codegen.cpp
)

llvm_map_components_to_libnames(llvm_libs
	core
	orcjit
	native
	support
	irreader
)

target_link_libraries(nexus PRIVATE ${llvm_libs})

target_compile_options(nexus PRIVATE -Wall -Wextra -pedantic)
