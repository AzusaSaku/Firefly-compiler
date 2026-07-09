#include <iostream>
#include "driver.hpp"
#include "repl.hpp"
using namespace std;

#ifndef FIREFLY_VERSION
#define FIREFLY_VERSION "dev"
#endif

int main(int argc, char *argv[])
{
    string program_name = argc > 0 ? argv[0] : "firefly";
    set_firefly_executable_path(program_name);

    if (argc == 1) {
        cout << "Hello Firefly!" << endl;
        cout << "Enter Firefly code. Use :ast, :eval, or :exit." << endl;
        start();
        return 0;
    }

    DriverMode mode = DriverMode::EVAL;
    string file_path;

    for (int i = 1; i < argc; i++) {
        string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            print_usage(cout, program_name);
            return 0;
        }

        if (arg == "--version" || arg == "-V") {
            cout << "firefly " << FIREFLY_VERSION << endl;
            return 0;
        }

        if (arg == "--repl") {
            cout << "Hello Firefly!" << endl;
            cout << "Enter Firefly code. Use :ast, :eval, or :exit." << endl;
            start();
            return 0;
        }

        if (arg == "--eval") {
            mode = DriverMode::EVAL;
            continue;
        }

        if (arg == "--ast") {
            mode = DriverMode::AST;
            continue;
        }

        if (arg == "--emit-llvm") {
            mode = DriverMode::EMIT_LLVM;
            continue;
        }

        if (arg == "--emit-obj") {
            mode = DriverMode::EMIT_OBJ;
            continue;
        }

        if (arg == "--build" || arg == "--compile") {
            mode = DriverMode::BUILD_EXE;
            continue;
        }

        if (!arg.empty() && arg[0] == '-') {
            cerr << "unknown option: " << arg << endl;
            print_usage(cerr, program_name);
            return 1;
        }

        if (!file_path.empty()) {
            cerr << "only one input file is supported" << endl;
            print_usage(cerr, program_name);
            return 1;
        }

        file_path = arg;
    }

    if (file_path.empty()) {
        cerr << "missing input file" << endl;
        print_usage(cerr, program_name);
        return 1;
    }

    return run_file(file_path, mode, cout, cerr);
}
