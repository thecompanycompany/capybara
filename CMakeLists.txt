cmake_minimum_required(VERSION 4.0)
project(capybara LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

add_executable(capybara
    src/main.cpp
    src/compiler.cpp
    src/lsp.cpp
    src/lexer.cpp
    src/parser.cpp
    src/ast.cpp
)

target_include_directories(capybara PRIVATE
    ${LLVM_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_options(capybara PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -fno-rtti
    -fno-exceptions
)

#llvm_map_components_to_libnames(llvm_libs ${LLVM_TARGETS_TO_BUILD} support core orcjit native supportlsp)
target_link_libraries(capybara PRIVATE LLVM)
target_link_directories(capybara PRIVATE /usr/local/lib)
