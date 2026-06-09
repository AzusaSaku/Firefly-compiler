cmake_minimum_required(VERSION 3.26)
project(Firefly_interpreter)

set(CMAKE_CXX_STANDARD 20)

add_executable(Firefly_interpreter src/main.cpp
        src/token.hpp
        src/lexer.hpp
        src/repl.hpp
        src/ast.hpp
        src/parser.hpp
        src/semantic.hpp
)
