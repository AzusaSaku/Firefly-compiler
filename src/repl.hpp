/* 控制台 交互模式 */

#ifndef FIREFLY_COMPILER_REPL_HPP
#define FIREFLY_COMPILER_REPL_HPP

#include "evaluator.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include <iostream>

const string PROMPT = ">> ";

void start() {
    string input;
    auto env = new_environment();
    bool print_ast = false;

    cout << PROMPT << flush;
    while (getline(cin, input)) {
        if (input.empty()) {
            cout << PROMPT << flush;
            continue;
        }

        if (input == ":exit" || input == ":quit") {
            break;
        }

        if (input == ":ast") {
            print_ast = true;
            cout << "AST output enabled" << endl;
            cout << PROMPT << flush;
            continue;
        }

        if (input == ":eval") {
            print_ast = false;
            cout << "Evaluator output enabled" << endl;
            cout << PROMPT << flush;
            continue;
        }

        Lexer *l = new_lex(input);
        auto *p = new Parser(l);

        Program *program = p->parse_program();
        if (!p->errors.empty()) {
            for (auto &msg : p->errors) {
                cout << "parser error: " << msg << endl;
            }
            delete program;
            delete p;
            delete l;
            cout << PROMPT << flush;
            continue;
        }

        if (print_ast) {
            cout << program->to_string() << endl;
        } else {
            auto evaluated = eval(program, env);
            if (evaluated != nullptr) {
                cout << evaluated->inspect() << endl;
            }
        }

        delete program;
        delete p;
        delete l;
        cout << PROMPT << flush;
    }
}

#endif //FIREFLY_COMPILER_REPL_HPP
