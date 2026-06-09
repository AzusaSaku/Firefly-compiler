/* 定义抽象语法树AST */

#ifndef FIREFLY_INTERPRETER_AST_HPP
#define FIREFLY_INTERPRETER_AST_HPP

#include "token.hpp"
#include <vector>
#include <sstream>

using namespace std;

class Node {
public:
    virtual string token_literal() = 0;
    virtual string to_string() = 0;
};

class Statement : public Node { // 语句
public:
    virtual string statement_node() = 0;
};

class Expression : public Node { // 表达式
public:
    string token_literal() override{
        return "";
    };
    string to_string() override {
        return "";
    };
    virtual string expression_node(){
        return "";
    };
};

class Program : public Node {
public:
    vector<Statement *> statements;
    string token_literal() override {
        if (!statements.empty()) {
            return statements[0]->token_literal();
        } else {
            return "";
        }
    }

    string to_string() override {
        stringstream out;
        for (auto &stmt : statements) {
            out << stmt->to_string();
        }
        return out.str();
    }
};

// 标识符表达式
class Identifier : public Expression {
public:
    Token token;
    string value;
    string token_literal() override {
        return token.Literal;
    }
    string to_string() override {
        return value;
    }
};

// 整型字面量表达式
class IntegerLiteral : public Expression {
public:
    Token token;
    int value{};
    string token_literal() override {
        return token.Literal;
    }
    string to_string() override {
        return token.Literal;
    }
};

// 前缀表达式
class PrefixExpression : public Expression {
public:
    Token token;
    string op;
    Expression *right{};
    string token_literal() override {
        return token.Literal;
    }
    string to_string() override {
        stringstream out;
        out << "(" << op;
        if (right != nullptr) {
            out << right->to_string();
        }
        out << ")";
        return out.str();
    }
};

// 中缀表达式
class InfixExpression : public Expression {
public:
    Token token;
    Expression *left{};
    string op;
    Expression *right{};
    string token_literal() override {
        return token.Literal;
    }
    string to_string() override {
        stringstream out;
        out << "(";
        if (left != nullptr) {
            out << left->to_string();
        }
        out << " " << op << " ";
        if (right != nullptr) {
            out << right->to_string();
        }
        out << ")";
        return out.str();
    }
};

// 布尔表达式
class Boolean : public Expression {
public:
    Token token;
    bool value{};
    string token_literal() override {
        return token.Literal;
    }
    string to_string() override {
        return token.Literal;
    }
};

// 语句块
class BlockStatement : public Statement {
public:
    Token token;
    vector<Statement *> statements;
    string statement_node() override {
        return "";
    }
    string token_literal() override {
        return token.Literal;
    }
    string to_string() override {
        stringstream out;
        for (auto &stmt : statements) {
            out << stmt->to_string();
        }
        return out.str();
    }
};

// IF表达式
class IfExpression : public Expression {
public:
    Token token;
    Expression *condition{};
    BlockStatement *consequence{};
    BlockStatement *alternative{};
    string token_literal() override {
        return token.Literal;
    }
    string to_string() override {
        stringstream out;
        out << "if";
        if (condition != nullptr) {
            out << condition->to_string();
        }
        out << " ";
        if (consequence != nullptr) {
            out << consequence->to_string();
        }
        if (alternative != nullptr) {
            out << "else " << alternative->to_string();
        }
        return out.str();
    }
};

// 函数字面量表达式
class FunctionLiteral : public Expression {
public:
    Token token;
    vector<Identifier *> parameters;
    BlockStatement *body{};
    string token_literal() override {
        return token.Literal;
    }
    string to_string() override {
        stringstream out;
        out << token_literal() << "(";
        for (size_t i = 0; i < parameters.size(); i++) {
            if (i > 0) {
                out << ", ";
            }
            out << parameters[i]->to_string();
        }
        out << ") ";
        if (body != nullptr) {
            out << body->to_string();
        }
        return out.str();
    }
};

// 函数调用表达式
class CallExpression : public Expression {
public:
    Token token;
    Expression *function{};
    vector<Expression *> arguments;
    string token_literal() override {
        return token.Literal;
    }
    string to_string() override {
        stringstream out;
        if (function != nullptr) {
            out << function->to_string();
        }
        out << "(";
        for (size_t i = 0; i < arguments.size(); i++) {
            if (i > 0) {
                out << ", ";
            }
            out << arguments[i]->to_string();
        }
        out << ")";
        return out.str();
    }
};

// VAR语句
class VarStatement : public Statement {
public:
    Token token;
    Identifier *name{};
    Expression *value{};
    string statement_node() override {
        return "";
    }
    string token_literal() override {
        return token.Literal;
    }
    string to_string() override {
        stringstream out;
        out << token_literal() << " ";
        if (name != nullptr) {
            out << name->to_string();
        }
        out << " = ";
        if (value != nullptr) {
            out << value->to_string();
        }
        out << ";";
        return out.str();
    }
};

// RETURN语句
class ReturnStatement : public Statement {
public:
    Token token;
    Expression *return_value{};
    string statement_node() override {
        return "";
    }
    string token_literal() override {
        return token.Literal;
    }
    string to_string() override {
        stringstream out;
        out << token_literal() << " ";
        if (return_value != nullptr) {
            out << return_value->to_string();
        }
        out << ";";
        return out.str();
    }
};

// 仅由一个表达式构成的语句
class ExpressionStatement : public Statement {
public:
    Token token;
    Expression *expression{};
    string statement_node() override {
        return "";
    }
    string token_literal() override {
        return token.Literal;
    }
    string to_string() override {
        if (expression != nullptr) {
            return expression->to_string();
        }
        return "";
    }
};

#endif //FIREFLY_INTERPRETER_AST_HPP
