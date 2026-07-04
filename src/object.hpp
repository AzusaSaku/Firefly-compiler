/* 定义解释器运行时对象 */

#ifndef FIREFLY_COMPILER_OBJECT_HPP
#define FIREFLY_COMPILER_OBJECT_HPP

#include "ast.hpp"
#include <functional>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace std;

class Environment;

enum class ObjectType {
    INTEGER,
    BOOLEAN,
    NULL_OBJ,
    RETURN_VALUE,
    FUNCTION,
    STRING,
    ARRAY,
    HASH,
    BUILTIN,
    ERROR
};

class Object {
public:
    virtual ~Object() = default;
    virtual ObjectType type() const = 0;
    virtual string inspect() const = 0;
};

using ObjectPtr = shared_ptr<Object>;

class IntegerObject : public Object {
public:
    int value{};

    explicit IntegerObject(int value) : value(value) {}

    ObjectType type() const override {
        return ObjectType::INTEGER;
    }

    string inspect() const override {
        return to_string(value);
    }
};

class BooleanObject : public Object {
public:
    bool value{};

    explicit BooleanObject(bool value) : value(value) {}

    ObjectType type() const override {
        return ObjectType::BOOLEAN;
    }

    string inspect() const override {
        return value ? "true" : "false";
    }
};

class StringObject : public Object {
public:
    string value;

    explicit StringObject(string value) : value(std::move(value)) {}

    ObjectType type() const override {
        return ObjectType::STRING;
    }

    string inspect() const override {
        return value;
    }
};

class NullObject : public Object {
public:
    ObjectType type() const override {
        return ObjectType::NULL_OBJ;
    }

    string inspect() const override {
        return "null";
    }
};

class ArrayObject : public Object {
public:
    vector<ObjectPtr> elements;

    explicit ArrayObject(vector<ObjectPtr> elements) : elements(std::move(elements)) {}

    ObjectType type() const override {
        return ObjectType::ARRAY;
    }

    string inspect() const override {
        stringstream out;
        out << "[";
        for (size_t i = 0; i < elements.size(); i++) {
            if (i > 0) {
                out << ", ";
            }
            out << elements[i]->inspect();
        }
        out << "]";
        return out.str();
    }
};

struct HashKey {
    ObjectType type;
    string value;

    bool operator<(const HashKey &other) const {
        if (type != other.type) {
            return type < other.type;
        }
        return value < other.value;
    }
};

struct HashPair {
    ObjectPtr key;
    ObjectPtr value;
};

class HashObject : public Object {
public:
    map<HashKey, HashPair> pairs;

    explicit HashObject(map<HashKey, HashPair> pairs) : pairs(std::move(pairs)) {}

    ObjectType type() const override {
        return ObjectType::HASH;
    }

    string inspect() const override {
        stringstream out;
        out << "{";

        size_t i = 0;
        for (const auto &entry : pairs) {
            if (i > 0) {
                out << ", ";
            }
            out << entry.second.key->inspect() << ": " << entry.second.value->inspect();
            i++;
        }

        out << "}";
        return out.str();
    }
};

class ReturnValueObject : public Object {
public:
    ObjectPtr value;

    explicit ReturnValueObject(ObjectPtr value) : value(std::move(value)) {}

    ObjectType type() const override {
        return ObjectType::RETURN_VALUE;
    }

    string inspect() const override {
        return value == nullptr ? "" : value->inspect();
    }
};

using BuiltinFunction = function<ObjectPtr(vector<ObjectPtr>)>;

class BuiltinObject : public Object {
public:
    BuiltinFunction fn;

    explicit BuiltinObject(BuiltinFunction fn) : fn(std::move(fn)) {}

    ObjectType type() const override {
        return ObjectType::BUILTIN;
    }

    string inspect() const override {
        return "builtin function";
    }
};

inline Identifier *clone_identifier(Identifier *ident) {
    if (ident == nullptr) {
        return nullptr;
    }

    auto *clone = new Identifier();
    clone->token = ident->token;
    clone->value = ident->value;
    clone->type_annotation = ident->type_annotation;
    return clone;
}

inline Expression *clone_expression(Expression *expression);
inline Statement *clone_statement(Statement *statement);

inline BlockStatement *clone_block_statement(BlockStatement *block) {
    if (block == nullptr) {
        return nullptr;
    }

    auto *clone = new BlockStatement();
    clone->token = block->token;
    for (auto *stmt : block->statements) {
        clone->statements.push_back(clone_statement(stmt));
    }
    return clone;
}

inline Expression *clone_expression(Expression *expression) {
    if (expression == nullptr) {
        return nullptr;
    }

    if (auto *ident = dynamic_cast<Identifier *>(expression)) {
        return clone_identifier(ident);
    }

    if (auto *literal = dynamic_cast<IntegerLiteral *>(expression)) {
        auto *clone = new IntegerLiteral();
        clone->token = literal->token;
        clone->value = literal->value;
        return clone;
    }

    if (auto *literal = dynamic_cast<StringLiteral *>(expression)) {
        auto *clone = new StringLiteral();
        clone->token = literal->token;
        clone->value = literal->value;
        return clone;
    }

    if (auto *boolean = dynamic_cast<Boolean *>(expression)) {
        auto *clone = new Boolean();
        clone->token = boolean->token;
        clone->value = boolean->value;
        return clone;
    }

    if (auto *prefix = dynamic_cast<PrefixExpression *>(expression)) {
        auto *clone = new PrefixExpression();
        clone->token = prefix->token;
        clone->op = prefix->op;
        clone->right = clone_expression(prefix->right);
        return clone;
    }

    if (auto *borrow = dynamic_cast<BorrowExpression *>(expression)) {
        auto *clone = new BorrowExpression();
        clone->token = borrow->token;
        clone->is_mutable = borrow->is_mutable;
        clone->value = clone_expression(borrow->value);
        return clone;
    }

    if (auto *infix = dynamic_cast<InfixExpression *>(expression)) {
        auto *clone = new InfixExpression();
        clone->token = infix->token;
        clone->left = clone_expression(infix->left);
        clone->op = infix->op;
        clone->right = clone_expression(infix->right);
        return clone;
    }

    if (auto *if_exp = dynamic_cast<IfExpression *>(expression)) {
        auto *clone = new IfExpression();
        clone->token = if_exp->token;
        clone->condition = clone_expression(if_exp->condition);
        clone->consequence = clone_block_statement(if_exp->consequence);
        clone->alternative = clone_block_statement(if_exp->alternative);
        return clone;
    }

    if (auto *while_exp = dynamic_cast<WhileExpression *>(expression)) {
        auto *clone = new WhileExpression();
        clone->token = while_exp->token;
        clone->condition = clone_expression(while_exp->condition);
        clone->body = clone_block_statement(while_exp->body);
        return clone;
    }

    if (auto *function = dynamic_cast<FunctionLiteral *>(expression)) {
        auto *clone = new FunctionLiteral();
        clone->token = function->token;
        clone->return_type_annotation = function->return_type_annotation;
        for (auto *param : function->parameters) {
            clone->parameters.push_back(clone_identifier(param));
        }
        clone->body = clone_block_statement(function->body);
        return clone;
    }

    if (auto *call = dynamic_cast<CallExpression *>(expression)) {
        auto *clone = new CallExpression();
        clone->token = call->token;
        clone->function = clone_expression(call->function);
        for (auto *arg : call->arguments) {
            clone->arguments.push_back(clone_expression(arg));
        }
        return clone;
    }

    if (auto *array = dynamic_cast<ArrayLiteral *>(expression)) {
        auto *clone = new ArrayLiteral();
        clone->token = array->token;
        for (auto *element : array->elements) {
            clone->elements.push_back(clone_expression(element));
        }
        return clone;
    }

    if (auto *index = dynamic_cast<IndexExpression *>(expression)) {
        auto *clone = new IndexExpression();
        clone->token = index->token;
        clone->left = clone_expression(index->left);
        clone->index = clone_expression(index->index);
        return clone;
    }

    if (auto *assign = dynamic_cast<AssignExpression *>(expression)) {
        auto *clone = new AssignExpression();
        clone->token = assign->token;
        clone->left = clone_expression(assign->left);
        clone->value = clone_expression(assign->value);
        return clone;
    }

    if (auto *hash = dynamic_cast<HashLiteral *>(expression)) {
        auto *clone = new HashLiteral();
        clone->token = hash->token;
        for (auto &pair : hash->pairs) {
            clone->pairs.emplace_back(clone_expression(pair.first), clone_expression(pair.second));
        }
        return clone;
    }

    return nullptr;
}

inline Statement *clone_statement(Statement *statement) {
    if (statement == nullptr) {
        return nullptr;
    }

    if (auto *block = dynamic_cast<BlockStatement *>(statement)) {
        return clone_block_statement(block);
    }

    if (auto *var_stmt = dynamic_cast<VarStatement *>(statement)) {
        auto *clone = new VarStatement();
        clone->token = var_stmt->token;
        clone->name = clone_identifier(var_stmt->name);
        clone->value = clone_expression(var_stmt->value);
        clone->is_mutable = var_stmt->is_mutable;
        return clone;
    }

    if (auto *return_stmt = dynamic_cast<ReturnStatement *>(statement)) {
        auto *clone = new ReturnStatement();
        clone->token = return_stmt->token;
        clone->return_value = clone_expression(return_stmt->return_value);
        return clone;
    }

    if (auto *expression_stmt = dynamic_cast<ExpressionStatement *>(statement)) {
        auto *clone = new ExpressionStatement();
        clone->token = expression_stmt->token;
        clone->expression = clone_expression(expression_stmt->expression);
        return clone;
    }

    return nullptr;
}

class FunctionObject : public Object {
public:
    vector<Identifier *> parameters;
    BlockStatement *body{};
    shared_ptr<Environment> env;

    FunctionObject(vector<Identifier *> parameters, BlockStatement *body, shared_ptr<Environment> env)
        : body(clone_block_statement(body)), env(std::move(env)) {
        for (auto *param : parameters) {
            this->parameters.push_back(clone_identifier(param));
        }
    }

    ~FunctionObject() override {
        for (auto *param : parameters) {
            delete param;
        }
        delete body;
    }

    ObjectType type() const override {
        return ObjectType::FUNCTION;
    }

    string inspect() const override {
        stringstream out;
        out << "func(";
        for (size_t i = 0; i < parameters.size(); i++) {
            if (i > 0) {
                out << ", ";
            }
            out << parameters[i]->to_string();
        }
        out << ") { ";
        if (body != nullptr) {
            out << body->to_string();
        }
        out << " }";
        return out.str();
    }
};

class ErrorObject : public Object {
public:
    string message;

    explicit ErrorObject(string message) : message(std::move(message)) {}

    ObjectType type() const override {
        return ObjectType::ERROR;
    }

    string inspect() const override {
        return "ERROR: " + message;
    }
};

inline string object_type_to_string(ObjectType type) {
    switch (type) {
        case ObjectType::INTEGER:
            return "INTEGER";
        case ObjectType::BOOLEAN:
            return "BOOLEAN";
        case ObjectType::NULL_OBJ:
            return "NULL";
        case ObjectType::RETURN_VALUE:
            return "RETURN_VALUE";
        case ObjectType::FUNCTION:
            return "FUNCTION";
        case ObjectType::STRING:
            return "STRING";
        case ObjectType::ARRAY:
            return "ARRAY";
        case ObjectType::HASH:
            return "HASH";
        case ObjectType::BUILTIN:
            return "BUILTIN";
        case ObjectType::ERROR:
            return "ERROR";
        default:
            return "UNKNOWN";
    }
}

#endif //FIREFLY_COMPILER_OBJECT_HPP
