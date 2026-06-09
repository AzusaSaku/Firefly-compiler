/* 控制台 交互模式 */

#ifndef FIREFLY_INTERPRETER_REPL_HPP
#define FIREFLY_INTERPRETER_REPL_HPP

#include "lexer.hpp"
#include "parser.hpp"
#include "semantic.hpp"
#include <iostream>

const string PROMPT = ">> ";

void start() {
    string input;
    cout << PROMPT << flush;
    while (getline(cin, input)) {
        if (input.empty()) {
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
            cout << PROMPT << flush;
            continue;
        }

        SemanticAnalyzer semantic;
        semantic.analyze(program);
        if (!semantic.errors.empty()) {
            for (auto &msg : semantic.errors) {
                cout << "semantic error: " << msg << endl;
            }
            cout << PROMPT << flush;
            continue;
        }

        cout << program->to_string() << endl;
        cout << PROMPT << flush;
    }
}

#endif //FIREFLY_INTERPRETER_REPL_HPP
