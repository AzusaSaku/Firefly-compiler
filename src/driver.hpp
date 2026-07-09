/* 命令行驱动和可复用前端入口 */

#ifndef FIREFLY_COMPILER_DRIVER_HPP
#define FIREFLY_COMPILER_DRIVER_HPP

#include "evaluator.hpp"
#include "llvm_codegen.hpp"
#include "parser.hpp"
#include "semantic.hpp"
#include <cstdlib>
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
    EMIT_LLVM,
    EMIT_OBJ,
    BUILD_EXE
};

struct FrontendResult {
    unique_ptr<Program> program;
    vector<string> errors;

    bool ok() const {
        return errors.empty() && program != nullptr;
    }
};

inline filesystem::path &firefly_executable_path_storage() {
    static filesystem::path path;
    return path;
}

inline void set_firefly_executable_path(const string &path) {
    firefly_executable_path_storage() = filesystem::absolute(filesystem::path(path));
}

inline filesystem::path firefly_executable_dir() {
    auto path = firefly_executable_path_storage();
    if (path.empty()) {
        return filesystem::path();
    }
    return path.parent_path();
}

// Frontend pipeline: source text -> AST -> semantic checks.
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

inline string diagnostic_with_source(const string &msg, const string &source_name) {
    if (source_name.empty()) {
        return msg;
    }
    return msg + " in " + source_name;
}

inline bool analyze_frontend(Program *program, ostream &err, const string &source_name = "") {
    SemanticAnalyzer analyzer;
    analyzer.analyze(program);

    if (!analyzer.errors.empty()) {
        for (auto &msg : analyzer.errors) {
            err << "semantic error: " << diagnostic_with_source(msg, source_name) << endl;
        }
        return false;
    }

    return true;
}

// Host tool discovery and shell command helpers used by native output modes.
inline string quote_command_path(const filesystem::path &path) {
    filesystem::path normalized = path;
    if (path.is_relative() && (path.has_parent_path() || filesystem::exists(path))) {
        normalized = filesystem::absolute(path);
    }
    normalized.make_preferred();

    string text = normalized.string();
    string quoted = "\"";
    for (char ch : text) {
        if (ch == '"') {
            quoted += "\\\"";
        } else {
            quoted += ch;
        }
    }
    quoted += "\"";
    return quoted;
}

inline filesystem::path llvm_tools_dir() {
    const char *env = getenv("FIREFLY_LLVM_TOOLS_DIR");
    if (env != nullptr && env[0] != '\0') {
        return filesystem::path(env);
    }

#ifdef FIREFLY_LLVM_TOOLS_DIR
    return filesystem::path(FIREFLY_LLVM_TOOLS_DIR);
#else
    return filesystem::path();
#endif
}

inline filesystem::path find_llvm_tool(const string &tool_name) {
    vector<filesystem::path> search_dirs;

    const char *env = getenv("FIREFLY_LLVM_TOOLS_DIR");
    if (env != nullptr && env[0] != '\0') {
        search_dirs.emplace_back(env);
    }

    auto exe_dir = firefly_executable_dir();
    if (!exe_dir.empty()) {
        search_dirs.push_back(exe_dir);
    }

#ifdef FIREFLY_LLVM_TOOLS_DIR
    search_dirs.emplace_back(FIREFLY_LLVM_TOOLS_DIR);
#endif

    for (auto &dir : search_dirs) {
        if (dir.empty()) {
            continue;
        }
        auto candidate = dir / tool_name;
        if (filesystem::exists(candidate)) {
            return candidate;
        }
    }

    return filesystem::path(tool_name);
}

inline string executable_tool_name(const string &name) {
#ifdef _WIN32
    return name + ".exe";
#else
    return name;
#endif
}

inline string native_object_extension() {
#ifdef _WIN32
    return ".obj";
#else
    return ".o";
#endif
}

inline string native_executable_extension() {
#ifdef _WIN32
    return ".exe";
#else
    return "";
#endif
}

inline int run_command(const string &command, ostream &err) {
#ifdef _WIN32
    string shell_command = "\"" + command + "\"";
#else
    string shell_command = command;
#endif

    int result = system(shell_command.c_str());
    if (result != 0) {
        err << "command failed: " << command << endl;
        return 1;
    }
    return 0;
}

inline int write_text_file(const filesystem::path &path, const string &content, ostream &err) {
    ofstream output(path);
    if (!output) {
        err << "could not open output file: " << path.string() << endl;
        return 1;
    }

    output << content;
    return 0;
}

// LLVM/backend boundary: compile Firefly source to IR and materialize native artifacts.
inline bool compile_source_to_ir(const string &source,
                                 const string &module_name,
                                 string &ir,
                                 ostream &err,
                                 const string &source_name = "") {
    auto frontend = parse_source(source);
    if (!frontend.ok()) {
        for (auto &msg : frontend.errors) {
            err << "parser error: " << diagnostic_with_source(msg, source_name) << endl;
        }
        return false;
    }

    if (!analyze_frontend(frontend.program.get(), err, source_name)) {
        return false;
    }

    vector<string> codegen_errors;
    if (!compile_program_to_llvm_ir(frontend.program.get(), module_name, ir, codegen_errors)) {
        for (auto &msg : codegen_errors) {
            err << "llvm error: " << msg << endl;
        }
        return false;
    }

    return true;
}

inline filesystem::path temporary_ir_path_for(const filesystem::path &input_path) {
    filesystem::path temp = input_path;
    temp.replace_filename(input_path.stem().string() + ".firefly.tmp.ll");
    return temp;
}

inline int emit_object_from_ir(const filesystem::path &ir_path,
                               const filesystem::path &object_path,
                               ostream &err) {
    auto llc = find_llvm_tool(executable_tool_name("llc"));
    string command = quote_command_path(llc) +
                     " -filetype=obj " +
                     quote_command_path(ir_path) +
                     " -o " +
                     quote_command_path(object_path);
    return run_command(command, err);
}

inline int link_native_executable(const filesystem::path &object_path,
                                  const filesystem::path &exe_path,
                                  ostream &err) {
#ifdef _WIN32
    auto linker = find_llvm_tool("lld-link.exe");
    string command = quote_command_path(linker) +
                     " /nologo /machine:x64 /subsystem:console " +
                     quote_command_path(object_path) +
                     " /out:" +
                     quote_command_path(exe_path) +
                     " /defaultlib:libcmt /defaultlib:oldnames";
    return run_command(command, err);
#else
    auto linker = find_llvm_tool("clang");
    if (!filesystem::exists(linker) && !llvm_tools_dir().empty()) {
        linker = filesystem::path("cc");
    }

    string command = quote_command_path(linker) +
                     " " +
                     quote_command_path(object_path) +
                     " -o " +
                     quote_command_path(exe_path);
    return run_command(command, err);
#endif
}

// Driver entry points used by main.cpp and tests.
inline int run_source(const string &source,
                      DriverMode mode,
                      ostream &out,
                      ostream &err,
                      const string &source_name = "") {
    auto frontend = parse_source(source);
    if (!frontend.ok()) {
        for (auto &msg : frontend.errors) {
            err << "parser error: " << diagnostic_with_source(msg, source_name) << endl;
        }
        return 1;
    }

    if (mode == DriverMode::AST) {
        out << frontend.program->to_string() << endl;
        return 0;
    }

    if (!analyze_frontend(frontend.program.get(), err, source_name)) {
        return 1;
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

    if (mode == DriverMode::EMIT_OBJ || mode == DriverMode::BUILD_EXE) {
        err << "native output modes require an input file path" << endl;
        return 1;
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

    if (mode != DriverMode::EMIT_LLVM && mode != DriverMode::EMIT_OBJ && mode != DriverMode::BUILD_EXE) {
        return run_source(source, mode, out, err, path);
    }

    string ir;
    filesystem::path input_path(path);
    if (!compile_source_to_ir(source, input_path.filename().string(), ir, err, path)) {
        return 1;
    }

    if (mode == DriverMode::EMIT_LLVM) {
        filesystem::path output_path(input_path);
        output_path.replace_extension(".ll");
        if (write_text_file(output_path, ir, err) != 0) {
            return 1;
        }
        out << "wrote " << output_path.string() << endl;
        return 0;
    }

    filesystem::path temp_ir_path = temporary_ir_path_for(input_path);
    if (write_text_file(temp_ir_path, ir, err) != 0) {
        return 1;
    }

    filesystem::path object_path(input_path);
    object_path.replace_extension(native_object_extension());
    int object_result = emit_object_from_ir(temp_ir_path, object_path, err);
    filesystem::remove(temp_ir_path);
    if (object_result != 0) {
        return object_result;
    }

    if (mode == DriverMode::EMIT_OBJ) {
        out << "wrote " << object_path.string() << endl;
        return 0;
    }

    filesystem::path exe_path(input_path);
    exe_path.replace_extension(native_executable_extension());
    int link_result = link_native_executable(object_path, exe_path, err);
    filesystem::remove(object_path);
    if (link_result != 0) {
        return link_result;
    }

    out << "wrote " << exe_path.string() << endl;
    return 0;
}

inline void print_usage(ostream &out, const string &program_name) {
    out << "Usage:" << endl;
    out << "  " << program_name << "                 Start REPL" << endl;
    out << "  " << program_name << " <file.ff>       Evaluate file" << endl;
    out << "  " << program_name << " --eval <file>   Evaluate file" << endl;
    out << "  " << program_name << " --ast <file>    Print AST" << endl;
    out << "  " << program_name << " --emit-llvm <file>  Emit LLVM IR to <file>.ll" << endl;
    out << "  " << program_name << " --emit-obj <file>   Emit native object file" << endl;
    out << "  " << program_name << " --build <file>      Build native executable" << endl;
    out << "  " << program_name << " --compile <file>    Alias for --build" << endl;
    out << "  " << program_name << " --repl          Start REPL" << endl;
    out << "  " << program_name << " --version       Show version" << endl;
    out << "  " << program_name << " --help          Show help" << endl;
}

#endif //FIREFLY_COMPILER_DRIVER_HPP
