cmake_minimum_required(VERSION 3.26)
project(Firefly VERSION 0.1.0)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(FIREFLY_LOCAL_LLVM_PREFIX "D:/vcpkg/installed/x64-windows-release" CACHE PATH
        "Local fallback LLVM prefix for the maintainer's vcpkg classic-mode install")
if (NOT LLVM_DIR
        AND NOT CMAKE_TOOLCHAIN_FILE
        AND EXISTS "${FIREFLY_LOCAL_LLVM_PREFIX}/share/llvm/LLVMConfig.cmake")
    set(LLVM_DIR "${FIREFLY_LOCAL_LLVM_PREFIX}/share/llvm" CACHE PATH "Path to LLVMConfig.cmake")
    list(APPEND CMAKE_PREFIX_PATH "${FIREFLY_LOCAL_LLVM_PREFIX}")
endif ()

find_package(LLVM CONFIG REQUIRED)

add_executable(firefly src/main.cpp
        src/token.hpp
        src/lexer.hpp
        src/repl.hpp
        src/ast.hpp
        src/parser.hpp
        src/semantic.hpp
        src/object.hpp
        src/environment.hpp
        src/evaluator.hpp
        src/driver.hpp
        src/llvm_codegen.hpp
)

target_include_directories(firefly SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITION_OPTIONS NATIVE_COMMAND "${LLVM_DEFINITIONS}")
target_compile_options(firefly PRIVATE ${LLVM_DEFINITION_OPTIONS})
if (MSVC)
    target_compile_options(firefly PRIVATE /utf-8)
endif ()
if (LLVM_TOOLS_BINARY_DIR)
    target_compile_definitions(firefly PRIVATE FIREFLY_LLVM_TOOLS_DIR="${LLVM_TOOLS_BINARY_DIR}")
endif ()
target_compile_definitions(firefly PRIVATE FIREFLY_VERSION="${PROJECT_VERSION}")
llvm_map_components_to_libnames(LLVM_LIBS support core)
target_link_libraries(firefly PRIVATE ${LLVM_LIBS})

install(TARGETS firefly RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
