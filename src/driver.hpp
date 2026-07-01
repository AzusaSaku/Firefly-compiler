/* 命令行驱动和可复用前端入口 */

#ifndef FIREFLY_COMPILER_DRIVER_HPP
#define FIREFLY_COMPILER_DRIVER_HPP

#include "evaluator.hpp"
#include "llvm_codegen.hpp"
#include "parser.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

enum class DriverMode {
    EVAL,
    AST,
    EMIT_LLVM
};

struct FrontendResult {
    unique_ptr<Program> program;
    vector<string> errors;

    bool ok() const {
        return errors.empty() && program != nullptr;
    }
};

inline string read_source_file(const string &path, vector<string> &errors) {
    ifstream in(path);
    if (!in) {
        errors.push_back("could not open file: " + path);
        return "";
    }

    stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

inline FrontendResult parse_source(const string &source) {
    Lexer *lexer = new_lex(source);
    Parser parser(lexer);
    Program *program = parser.parse_program();
    delete lexer;

    FrontendResult result;
    result.program = unique_ptr<Program>(program);
    result.errors = parser.errors;
    return result;
}

inline int run_source(const string &source, DriverMode mode, ostream &out, ostream &err) {
    auto frontend = parse_source(source);
    if (!frontend.ok()) {
        for (auto &msg : frontend.errors) {
            err << "parser error: " << msg << endl;
        }
        return 1;
    }

    if (mode == DriverMode::AST) {
        out << frontend.program->to_string() << endl;
        return 0;
    }

    if (mode == DriverMode::EMIT_LLVM) {
        string ir;
        vector<string> codegen_errors;
        if (!compile_program_to_llvm_ir(frontend.program.get(), "firefly", ir, codegen_errors)) {
            for (auto &msg : codegen_errors) {
                err << "llvm error: " << msg << endl;
            }
            return 1;
        }

        out << ir;
        return 0;
    }

    auto env = new_environment();
    auto evaluated = eval(frontend.program.get(), env);
    if (evaluated != nullptr) {
        if (evaluated->type() == ObjectType::ERROR) {
            err << evaluated->inspect() << endl;
            return 1;
        }
        out << evaluated->inspect() << endl;
    }

    return 0;
}

inline int run_file(const string &path, DriverMode mode, ostream &out, ostream &err) {
    vector<string> errors;
    string source = read_source_file(path, errors);
    if (!errors.empty()) {
        for (auto &msg : errors) {
            err << msg << endl;
        }
        return 1;
    }

    if (mode != DriverMode::EMIT_LLVM) {
        return run_source(source, mode, out, err);
    }

    auto frontend = parse_source(source);
    if (!frontend.ok()) {
        for (auto &msg : frontend.errors) {
            err << "parser error: " << msg << endl;
        }
        return 1;
    }

    string ir;
    vector<string> codegen_errors;
    filesystem::path output_path(path);
    output_path.replace_extension(".ll");

    if (!compile_program_to_llvm_ir(frontend.program.get(), output_path.filename().string(), ir, codegen_errors)) {
        for (auto &msg : codegen_errors) {
            err << "llvm error: " << msg << endl;
        }
        return 1;
    }

    ofstream output(output_path);
    if (!output) {
        err << "could not open output file: " << output_path.string() << endl;
        return 1;
    }

    output << ir;
    out << "wrote " << output_path.string() << endl;
    return 0;
}

inline void print_usage(ostream &out, const string &program_name) {
    out << "Usage:" << endl;
    out << "  " << program_name << "                 Start REPL" << endl;
    out << "  " << program_name << " <file.ff>       Evaluate file" << endl;
    out << "  " << program_name << " --eval <file>   Evaluate file" << endl;
    out << "  " << program_name << " --ast <file>    Print AST" << endl;
    out << "  " << program_name << " --emit-llvm <file>  Emit LLVM IR to <file>.ll" << endl;
    out << "  " << program_name << " --repl          Start REPL" << endl;
    out << "  " << program_name << " --help          Show help" << endl;
}

#endif //FIREFLY_COMPILER_DRIVER_HPP
