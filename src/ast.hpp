/* 定义抽象语法树AST */

#ifndef FIREFLY_COMPILER_AST_HPP
#define FIREFLY_COMPILER_AST_HPP

#include "token.hpp"
#include <vector>
#include <sstream>
#include <utility>

using namespace std;

class Node {
public:
    virtual ~Node() = default;
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
    ~Program() override {
        for (auto *stmt : statements) {
            delete stmt;
        }
    }

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

// 字符串字面量表达式
class StringLiteral : public Expression {
public:
    Token token;
    string value;
    string token_literal() override {
        return token.Literal;
    }
    string to_string() override {
        return "\"" + value + "\"";
    }
};

// 前缀表达式
class PrefixExpression : public Expression {
public:
    Token token;
    string op;
    Expression *right{};
    ~PrefixExpression() override {
        delete right;
    }

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
    ~InfixExpression() override {
        delete left;
        delete right;
    }

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
    ~BlockStatement() override {
        for (auto *stmt : statements) {
            delete stmt;
        }
    }

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
    ~IfExpression() override {
        delete condition;
        delete consequence;
        delete alternative;
    }

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

// WHILE表达式
class WhileExpression : public Expression {
public:
    Token token;
    Expression *condition{};
    BlockStatement *body{};
    ~WhileExpression() override {
        delete condition;
        delete body;
    }

    string token_literal() override {
        return token.Literal;
    }
    string to_string() override {
        stringstream out;
        out << "while";
        if (condition != nullptr) {
            out << condition->to_string();
        }
        out << " ";
        if (body != nullptr) {
            out << body->to_string();
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
    ~FunctionLiteral() override {
        for (auto *param : parameters) {
            delete param;
        }
        delete body;
    }

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
    ~CallExpression() override {
        delete function;
        for (auto *arg : arguments) {
            delete arg;
        }
    }

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

// 数组字面量表达式
class ArrayLiteral : public Expression {
public:
    Token token;
    vector<Expression *> elements;
    ~ArrayLiteral() override {
        for (auto *element : elements) {
            delete element;
        }
    }

    string token_literal() override {
        return token.Literal;
    }
    string to_string() override {
        stringstream out;
        out << "[";
        for (size_t i = 0; i < elements.size(); i++) {
            if (i > 0) {
                out << ", ";
            }
            out << elements[i]->to_string();
        }
        out << "]";
        return out.str();
    }
};

// 索引表达式
class IndexExpression : public Expression {
public:
    Token token;
    Expression *left{};
    Expression *index{};
    ~IndexExpression() override {
        delete left;
        delete index;
    }

    string token_literal() override {
        return token.Literal;
    }
    string to_string() override {
        stringstream out;
        out << "(";
        if (left != nullptr) {
            out << left->to_string();
        }
        out << "[";
        if (index != nullptr) {
            out << index->to_string();
        }
        out << "])";
        return out.str();
    }
};

// 赋值表达式
class AssignExpression : public Expression {
public:
    Token token;
    Expression *left{};
    Expression *value{};
    ~AssignExpression() override {
        delete left;
        delete value;
    }

    string token_literal() override {
        return token.Literal;
    }
    string to_string() override {
        stringstream out;
        out << "(";
        if (left != nullptr) {
            out << left->to_string();
        }
        out << " = ";
        if (value != nullptr) {
            out << value->to_string();
        }
        out << ")";
        return out.str();
    }
};

// 哈希字面量表达式
class HashLiteral : public Expression {
public:
    Token token;
    vector<pair<Expression *, Expression *>> pairs;
    ~HashLiteral() override {
        for (auto &pair : pairs) {
            delete pair.first;
            delete pair.second;
        }
    }

    string token_literal() override {
        return token.Literal;
    }
    string to_string() override {
        stringstream out;
        out << "{";
        for (size_t i = 0; i < pairs.size(); i++) {
            if (i > 0) {
                out << ", ";
            }
            out << pairs[i].first->to_string() << ": " << pairs[i].second->to_string();
        }
        out << "}";
        return out.str();
    }
};

// VAR语句
class VarStatement : public Statement {
public:
    Token token;
    Identifier *name{};
    Expression *value{};
    ~VarStatement() override {
        delete name;
        delete value;
    }

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
    ~ReturnStatement() override {
        delete return_value;
    }

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
    ~ExpressionStatement() override {
        delete expression;
    }

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

#endif //FIREFLY_COMPILER_AST_HPP
