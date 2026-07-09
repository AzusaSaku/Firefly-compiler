/* LLVM IR backend for the first Firefly compilation stage. */

#ifndef FIREFLY_COMPILER_LLVM_CODEGEN_HPP
#define FIREFLY_COMPILER_LLVM_CODEGEN_HPP

#include "ast.hpp"
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace std;

enum class FireflyLlvmTypeKind {
    UNKNOWN,
    VOID,
    INT,
    BOOL,
    STRING,
    FUNCTION
};

struct FireflyLlvmType {
    FireflyLlvmTypeKind kind{FireflyLlvmTypeKind::UNKNOWN};
    vector<FireflyLlvmType> params;
    unique_ptr<FireflyLlvmType> return_type;

    FireflyLlvmType() = default;

    explicit FireflyLlvmType(FireflyLlvmTypeKind kind) : kind(kind) {}

    FireflyLlvmType(const FireflyLlvmType &other) : kind(other.kind), params(other.params) {
        if (other.return_type != nullptr) {
            return_type = make_unique<FireflyLlvmType>(*other.return_type);
        }
    }

    FireflyLlvmType &operator=(const FireflyLlvmType &other) {
        if (this == &other) {
            return *this;
        }

        kind = other.kind;
        params = other.params;
        if (other.return_type != nullptr) {
            return_type = make_unique<FireflyLlvmType>(*other.return_type);
        } else {
            return_type.reset();
        }
        return *this;
    }

    static FireflyLlvmType unknown() {
        return FireflyLlvmType(FireflyLlvmTypeKind::UNKNOWN);
    }

    static FireflyLlvmType void_type() {
        return FireflyLlvmType(FireflyLlvmTypeKind::VOID);
    }

    static FireflyLlvmType int_type() {
        return FireflyLlvmType(FireflyLlvmTypeKind::INT);
    }

    static FireflyLlvmType bool_type() {
        return FireflyLlvmType(FireflyLlvmTypeKind::BOOL);
    }

    static FireflyLlvmType string_type() {
        return FireflyLlvmType(FireflyLlvmTypeKind::STRING);
    }

    static FireflyLlvmType function_type(vector<FireflyLlvmType> params, FireflyLlvmType result) {
        FireflyLlvmType type(FireflyLlvmTypeKind::FUNCTION);
        type.params = std::move(params);
        type.return_type = make_unique<FireflyLlvmType>(std::move(result));
        return type;
    }

    bool is_unknown() const {
        return kind == FireflyLlvmTypeKind::UNKNOWN;
    }

    bool is_function() const {
        return kind == FireflyLlvmTypeKind::FUNCTION;
    }

    string to_string() const {
        switch (kind) {
            case FireflyLlvmTypeKind::UNKNOWN:
                return "unknown";
            case FireflyLlvmTypeKind::VOID:
                return "void";
            case FireflyLlvmTypeKind::INT:
                return "int";
            case FireflyLlvmTypeKind::BOOL:
                return "bool";
            case FireflyLlvmTypeKind::STRING:
                return "string";
            case FireflyLlvmTypeKind::FUNCTION: {
                stringstream out;
                out << "func(";
                for (size_t i = 0; i < params.size(); i++) {
                    if (i > 0) {
                        out << ", ";
                    }
                    out << params[i].to_string();
                }
                out << ") -> ";
                out << (return_type == nullptr ? "unknown" : return_type->to_string());
                return out.str();
            }
        }
        return "unknown";
    }
};

inline bool same_firefly_llvm_type(const FireflyLlvmType &left, const FireflyLlvmType &right) {
    if (left.kind != right.kind) {
        return false;
    }

    if (left.kind != FireflyLlvmTypeKind::FUNCTION) {
        return true;
    }

    if (left.params.size() != right.params.size()) {
        return false;
    }

    for (size_t i = 0; i < left.params.size(); i++) {
        if (!same_firefly_llvm_type(left.params[i], right.params[i])) {
            return false;
        }
    }

    if (left.return_type == nullptr || right.return_type == nullptr) {
        return left.return_type == nullptr && right.return_type == nullptr;
    }

    return same_firefly_llvm_type(*left.return_type, *right.return_type);
}

struct FireflyLlvmFunctionInfo {
    string name;
    FunctionLiteral *literal{};
    FireflyLlvmType type;
};

struct FireflyLlvmSymbol {
    FireflyLlvmType type;
    bool is_mutable{true};
};

class FireflyLlvmFeatureChecker {
public:
    bool check(Program *program) {
        if (program != nullptr) {
            for (auto *stmt : program->statements) {
                check_statement(stmt);
            }
        }
        return errors.empty();
    }

    vector<string> errors;

private:
    set<string> seen_errors;

    void add_error_once(const string &message) {
        if (seen_errors.insert(message).second) {
            errors.push_back(message);
        }
    }

    static bool starts_with(const string &value, const string &prefix) {
        return value.rfind(prefix, 0) == 0;
    }

    static bool is_builtin_name(const string &name) {
        static const set<string> builtin_names = {"len", "first", "last", "rest", "push", "puts"};
        return builtin_names.find(name) != builtin_names.end();
    }

    static bool is_supported_builtin_name(const string &name) {
        return name == "puts";
    }

    void check_type_annotation(const string &annotation) {
        if (annotation.empty()) {
            return;
        }

        if (starts_with(annotation, "&")) {
            add_error_once("unsupported feature for --emit-llvm: reference type annotations");
        }

        if (annotation == "array" || starts_with(annotation, "array<") || annotation == "hash") {
            add_error_once("unsupported feature for --emit-llvm: array and hash type annotations");
        }
    }

    void check_statement(Statement *stmt) {
        if (stmt == nullptr) {
            return;
        }

        if (auto *var_stmt = dynamic_cast<VarStatement *>(stmt)) {
            if (var_stmt->name != nullptr) {
                check_type_annotation(var_stmt->name->type_annotation);
            }
            check_expression(var_stmt->value);
            return;
        }

        if (auto *return_stmt = dynamic_cast<ReturnStatement *>(stmt)) {
            check_expression(return_stmt->return_value);
            return;
        }

        if (auto *expression_stmt = dynamic_cast<ExpressionStatement *>(stmt)) {
            check_expression(expression_stmt->expression);
            return;
        }

        if (auto *block_stmt = dynamic_cast<BlockStatement *>(stmt)) {
            check_block(block_stmt);
        }
    }

    void check_block(BlockStatement *block) {
        if (block == nullptr) {
            return;
        }

        for (auto *stmt : block->statements) {
            check_statement(stmt);
        }
    }

    void check_expression(Expression *expression) {
        if (expression == nullptr) {
            return;
        }

        if (auto *ident = dynamic_cast<Identifier *>(expression)) {
            if (is_builtin_name(ident->value) && !is_supported_builtin_name(ident->value)) {
                add_error_once("unsupported feature for --emit-llvm: builtins");
            }
            check_type_annotation(ident->type_annotation);
            return;
        }

        if (dynamic_cast<IntegerLiteral *>(expression) != nullptr ||
            dynamic_cast<Boolean *>(expression) != nullptr ||
            dynamic_cast<StringLiteral *>(expression) != nullptr) {
            return;
        }

        if (auto *borrow = dynamic_cast<BorrowExpression *>(expression)) {
            add_error_once("unsupported feature for --emit-llvm: borrow expressions");
            check_expression(borrow->value);
            return;
        }

        if (auto *prefix = dynamic_cast<PrefixExpression *>(expression)) {
            check_expression(prefix->right);
            return;
        }

        if (auto *infix = dynamic_cast<InfixExpression *>(expression)) {
            check_expression(infix->left);
            check_expression(infix->right);
            return;
        }

        if (auto *if_exp = dynamic_cast<IfExpression *>(expression)) {
            check_expression(if_exp->condition);
            check_block(if_exp->consequence);
            check_block(if_exp->alternative);
            return;
        }

        if (auto *while_exp = dynamic_cast<WhileExpression *>(expression)) {
            check_expression(while_exp->condition);
            check_block(while_exp->body);
            return;
        }

        if (auto *function = dynamic_cast<FunctionLiteral *>(expression)) {
            for (auto *param : function->parameters) {
                if (param != nullptr) {
                    check_type_annotation(param->type_annotation);
                }
            }
            check_type_annotation(function->return_type_annotation);
            check_block(function->body);
            return;
        }

        if (auto *call = dynamic_cast<CallExpression *>(expression)) {
            if (auto *ident = dynamic_cast<Identifier *>(call->function)) {
                if (is_builtin_name(ident->value) && !is_supported_builtin_name(ident->value)) {
                    add_error_once("unsupported feature for --emit-llvm: builtins");
                }
            }
            check_expression(call->function);
            for (auto *arg : call->arguments) {
                check_expression(arg);
            }
            return;
        }

        if (auto *assign = dynamic_cast<AssignExpression *>(expression)) {
            if (dynamic_cast<IndexExpression *>(assign->left) != nullptr) {
                add_error_once("unsupported feature for --emit-llvm: index assignment");
            }
            check_expression(assign->left);
            check_expression(assign->value);
            return;
        }

        if (auto *array = dynamic_cast<ArrayLiteral *>(expression)) {
            add_error_once("unsupported feature for --emit-llvm: arrays");
            for (auto *element : array->elements) {
                check_expression(element);
            }
            return;
        }

        if (auto *index = dynamic_cast<IndexExpression *>(expression)) {
            add_error_once("unsupported feature for --emit-llvm: index expressions");
            check_expression(index->left);
            check_expression(index->index);
            return;
        }

        if (auto *hash = dynamic_cast<HashLiteral *>(expression)) {
            add_error_once("unsupported feature for --emit-llvm: hashes");
            for (auto &pair : hash->pairs) {
                check_expression(pair.first);
                check_expression(pair.second);
            }
        }
    }
};

class FireflyLlvmTypeInferer {
public:
    vector<string> errors;

    FireflyLlvmType infer(Program *program) {
        begin_scope();
        collect_top_level_functions(program);

        for (int i = 0; i < 5; i++) {
            changed = false;
            infer_program(program);
            for (auto &entry : functions) {
                infer_function(entry.second);
            }
            if (!changed) {
                break;
            }
        }

        resolve_unknown_functions();
        end_scope();
        return FireflyLlvmType::int_type();
    }

    map<string, FireflyLlvmFunctionInfo> functions;

private:
    vector<map<string, FireflyLlvmSymbol>> scopes;
    bool changed{};

    void begin_scope() {
        scopes.emplace_back();
    }

    void end_scope() {
        scopes.pop_back();
    }

    void define(const string &name, const FireflyLlvmType &type, bool is_mutable = true) {
        if (scopes.empty()) {
            begin_scope();
        }
        auto &slot = scopes.back()[name];
        slot.is_mutable = is_mutable;
        merge_into(slot.type, type, "variable '" + name + "'");
    }

    FireflyLlvmSymbol *resolve(const string &name) {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) {
                return &found->second;
            }
        }
        return nullptr;
    }

    FireflyLlvmType type_from_annotation(const string &annotation, const string &context) {
        if (annotation.empty()) {
            return FireflyLlvmType::unknown();
        }
        if (annotation == "int") {
            return FireflyLlvmType::int_type();
        }
        if (annotation == "bool") {
            return FireflyLlvmType::bool_type();
        }
        if (annotation == "string") {
            return FireflyLlvmType::string_type();
        }
        if (annotation == "void") {
            return FireflyLlvmType::void_type();
        }

        errors.push_back("unsupported type annotation for --emit-llvm in " + context + ": " + annotation);
        return FireflyLlvmType::unknown();
    }

    vector<FireflyLlvmType> parameter_types(FunctionLiteral *function) {
        vector<FireflyLlvmType> params;
        if (function == nullptr) {
            return params;
        }

        for (auto *param : function->parameters) {
            if (param == nullptr) {
                params.push_back(FireflyLlvmType::unknown());
                continue;
            }
            params.push_back(type_from_annotation(param->type_annotation, "parameter '" + param->value + "'"));
        }
        return params;
    }

    void collect_top_level_functions(Program *program) {
        if (program == nullptr) {
            return;
        }

        for (auto *stmt : program->statements) {
            auto *var_stmt = dynamic_cast<VarStatement *>(stmt);
            if (var_stmt == nullptr || var_stmt->name == nullptr) {
                continue;
            }

            auto *function = dynamic_cast<FunctionLiteral *>(var_stmt->value);
            if (function == nullptr) {
                continue;
            }

            auto params = parameter_types(function);
            auto return_type = type_from_annotation(function->return_type_annotation, "function return");
            FireflyLlvmType type = FireflyLlvmType::function_type(params, return_type);
            functions[var_stmt->name->value] = FireflyLlvmFunctionInfo{var_stmt->name->value, function, type};
            define(var_stmt->name->value, type, var_stmt->is_mutable);
        }
    }

    void infer_program(Program *program) {
        if (program == nullptr) {
            return;
        }

        for (auto *stmt : program->statements) {
            infer_statement(stmt);
        }
    }

    FireflyLlvmType infer_block(BlockStatement *block) {
        begin_scope();
        FireflyLlvmType result = FireflyLlvmType::void_type();
        if (block != nullptr) {
            for (auto *stmt : block->statements) {
                result = infer_statement(stmt);
                if (dynamic_cast<ReturnStatement *>(stmt) != nullptr) {
                    break;
                }
            }
        }
        end_scope();
        return result;
    }

    FireflyLlvmType infer_statement(Statement *stmt) {
        if (stmt == nullptr) {
            return FireflyLlvmType::void_type();
        }

        if (auto *var_stmt = dynamic_cast<VarStatement *>(stmt)) {
            if (var_stmt->name == nullptr) {
                return FireflyLlvmType::void_type();
            }

            if (dynamic_cast<FunctionLiteral *>(var_stmt->value) != nullptr) {
                auto found = functions.find(var_stmt->name->value);
                if (found == functions.end()) {
                    errors.push_back("nested function literals are not supported by --emit-llvm yet: " + var_stmt->name->value);
                    return FireflyLlvmType::void_type();
                }
                define(var_stmt->name->value, found->second.type, var_stmt->is_mutable);
                return found->second.type;
            }

            auto type = infer_expression(var_stmt->value);
            define(var_stmt->name->value, type, var_stmt->is_mutable);
            return type;
        }

        if (auto *return_stmt = dynamic_cast<ReturnStatement *>(stmt)) {
            return infer_expression(return_stmt->return_value);
        }

        if (auto *expression_stmt = dynamic_cast<ExpressionStatement *>(stmt)) {
            return infer_expression(expression_stmt->expression);
        }

        if (auto *block_stmt = dynamic_cast<BlockStatement *>(stmt)) {
            return infer_block(block_stmt);
        }

        return FireflyLlvmType::void_type();
    }

    FireflyLlvmType infer_expression(Expression *expression) {
        if (expression == nullptr) {
            return FireflyLlvmType::void_type();
        }

        if (dynamic_cast<IntegerLiteral *>(expression) != nullptr) {
            return FireflyLlvmType::int_type();
        }

        if (dynamic_cast<Boolean *>(expression) != nullptr) {
            return FireflyLlvmType::bool_type();
        }

        if (dynamic_cast<StringLiteral *>(expression) != nullptr) {
            return FireflyLlvmType::string_type();
        }

        if (auto *ident = dynamic_cast<Identifier *>(expression)) {
            auto *symbol = resolve(ident->value);
            if (symbol != nullptr) {
                return symbol->type;
            }
            errors.push_back("undefined identifier in LLVM backend: " + ident->value);
            return FireflyLlvmType::unknown();
        }

        if (auto *prefix = dynamic_cast<PrefixExpression *>(expression)) {
            auto right = infer_expression(prefix->right);
            if (prefix->op == "-") {
                require_type(right, FireflyLlvmType::int_type(), "prefix '-'");
                return FireflyLlvmType::int_type();
            }
            if (prefix->op == "!") {
                return FireflyLlvmType::bool_type();
            }
            errors.push_back("unsupported prefix operator for --emit-llvm: " + prefix->op);
            return FireflyLlvmType::unknown();
        }

        if (auto *infix = dynamic_cast<InfixExpression *>(expression)) {
            return infer_infix_expression(infix);
        }

        if (auto *if_exp = dynamic_cast<IfExpression *>(expression)) {
            infer_expression(if_exp->condition);
            auto consequence = infer_block(if_exp->consequence);
            auto alternative = if_exp->alternative == nullptr ? FireflyLlvmType::void_type() : infer_block(if_exp->alternative);
            if (alternative.kind == FireflyLlvmTypeKind::VOID) {
                return FireflyLlvmType::void_type();
            }
            merge_into(consequence, alternative, "if branches");
            return consequence;
        }

        if (auto *while_exp = dynamic_cast<WhileExpression *>(expression)) {
            infer_expression(while_exp->condition);
            infer_block(while_exp->body);
            return FireflyLlvmType::void_type();
        }

        if (auto *function = dynamic_cast<FunctionLiteral *>(expression)) {
            auto params = parameter_types(function);
            auto return_type = type_from_annotation(function->return_type_annotation, "function return");
            return FireflyLlvmType::function_type(params, return_type);
        }

        if (auto *call = dynamic_cast<CallExpression *>(expression)) {
            return infer_call_expression(call);
        }

        if (auto *assign = dynamic_cast<AssignExpression *>(expression)) {
            return infer_assign_expression(assign);
        }

        if (dynamic_cast<ArrayLiteral *>(expression) != nullptr ||
            dynamic_cast<IndexExpression *>(expression) != nullptr ||
            dynamic_cast<HashLiteral *>(expression) != nullptr) {
            errors.push_back("unsupported AST node for --emit-llvm in first stage: " + expression->to_string());
            return FireflyLlvmType::unknown();
        }

        return FireflyLlvmType::unknown();
    }

    FireflyLlvmType infer_assign_expression(AssignExpression *assign) {
        auto value = infer_expression(assign->value);

        auto *ident = dynamic_cast<Identifier *>(assign->left);
        if (ident == nullptr) {
            errors.push_back("only identifier assignment is supported by --emit-llvm yet");
            return FireflyLlvmType::unknown();
        }

        auto *target = resolve(ident->value);
        if (target == nullptr) {
            errors.push_back("undefined identifier in LLVM backend assignment: " + ident->value);
            return FireflyLlvmType::unknown();
        }

        if (!target->is_mutable) {
            errors.push_back("cannot assign to immutable binding: " + ident->value);
            return target->type;
        }

        merge_into(target->type, value, "assignment to '" + ident->value + "'");
        return target->type;
    }

    FireflyLlvmType infer_infix_expression(InfixExpression *infix) {
        auto left = infer_expression(infix->left);
        auto right = infer_expression(infix->right);

        if (infix->op == "+" || infix->op == "-" || infix->op == "*" || infix->op == "/") {
            if (infix->op == "+" && left.kind == FireflyLlvmTypeKind::STRING && right.kind == FireflyLlvmTypeKind::STRING) {
                errors.push_back("string concatenation is not supported by --emit-llvm yet");
                return FireflyLlvmType::string_type();
            }
            require_type(left, FireflyLlvmType::int_type(), "left operand of '" + infix->op + "'");
            require_type(right, FireflyLlvmType::int_type(), "right operand of '" + infix->op + "'");
            return FireflyLlvmType::int_type();
        }

        if (infix->op == "<" || infix->op == ">" || infix->op == "<=" || infix->op == ">=") {
            require_type(left, FireflyLlvmType::int_type(), "left operand of '" + infix->op + "'");
            require_type(right, FireflyLlvmType::int_type(), "right operand of '" + infix->op + "'");
            return FireflyLlvmType::bool_type();
        }

        if (infix->op == "==" || infix->op == "!=") {
            if (left.kind == FireflyLlvmTypeKind::STRING || right.kind == FireflyLlvmTypeKind::STRING) {
                errors.push_back("string equality is not supported by --emit-llvm yet");
            } else {
                merge_into(left, right, "operands of '" + infix->op + "'");
            }
            return FireflyLlvmType::bool_type();
        }

        if (infix->op == "&&" || infix->op == "||") {
            require_type(left, FireflyLlvmType::bool_type(), "left operand of '" + infix->op + "'");
            require_type(right, FireflyLlvmType::bool_type(), "right operand of '" + infix->op + "'");
            return FireflyLlvmType::bool_type();
        }

        errors.push_back("unsupported infix operator for --emit-llvm: " + infix->op);
        return FireflyLlvmType::unknown();
    }

    FireflyLlvmType infer_call_expression(CallExpression *call) {
        vector<FireflyLlvmType> args;
        for (auto *arg : call->arguments) {
            args.push_back(infer_expression(arg));
        }

        auto *ident = dynamic_cast<Identifier *>(call->function);
        if (ident == nullptr) {
            errors.push_back("only calls to named functions are supported by --emit-llvm yet");
            return FireflyLlvmType::unknown();
        }

        if (ident->value == "puts") {
            if (args.size() != 1) {
                errors.push_back("wrong number of arguments for puts: expected 1, got " + std::to_string(args.size()));
                return FireflyLlvmType::unknown();
            }
            require_type(args[0], FireflyLlvmType::string_type(), "argument 1 of puts");
            return FireflyLlvmType::void_type();
        }

        auto found = functions.find(ident->value);
        if (found == functions.end()) {
            errors.push_back("unknown function for --emit-llvm: " + ident->value);
            return FireflyLlvmType::unknown();
        }

        auto &fn_type = found->second.type;
        if (args.size() != fn_type.params.size()) {
            errors.push_back("wrong number of arguments for " + ident->value + ": expected " +
                             std::to_string(fn_type.params.size()) + ", got " + std::to_string(args.size()));
            return FireflyLlvmType::unknown();
        }

        for (size_t i = 0; i < args.size(); i++) {
            merge_into(fn_type.params[i], args[i], "argument " + std::to_string(i + 1) + " of " + ident->value);
        }

        if (fn_type.return_type == nullptr) {
            return FireflyLlvmType::unknown();
        }
        return *fn_type.return_type;
    }

    void infer_function(FireflyLlvmFunctionInfo &info) {
        begin_scope();
        for (size_t i = 0; i < info.literal->parameters.size(); i++) {
            auto name = info.literal->parameters[i]->value;
            define(name, info.type.params[i]);
        }

        auto result = infer_block(info.literal->body);
        if (result.kind == FireflyLlvmTypeKind::VOID) {
            result = FireflyLlvmType::int_type();
        }

        if (info.type.return_type == nullptr) {
            info.type.return_type = make_unique<FireflyLlvmType>(result);
            changed = true;
        } else {
            merge_into(*info.type.return_type, result, "return type of " + info.name);
        }

        for (size_t i = 0; i < info.literal->parameters.size(); i++) {
            auto *param_symbol = resolve(info.literal->parameters[i]->value);
            if (param_symbol != nullptr) {
                merge_into(info.type.params[i], param_symbol->type, "parameter " + info.literal->parameters[i]->value);
            }
        }
        end_scope();
    }

    void resolve_unknown_functions() {
        for (auto &entry : functions) {
            auto &type = entry.second.type;
            for (auto &param : type.params) {
                if (param.is_unknown()) {
                    param = FireflyLlvmType::int_type();
                    changed = true;
                }
            }
            if (type.return_type == nullptr || type.return_type->is_unknown() || type.return_type->kind == FireflyLlvmTypeKind::VOID) {
                type.return_type = make_unique<FireflyLlvmType>(FireflyLlvmType::int_type());
            }
        }
    }

    void require_type(FireflyLlvmType &actual, const FireflyLlvmType &expected, const string &context) {
        merge_into(actual, expected, context);
    }

    void merge_into(FireflyLlvmType &target, const FireflyLlvmType &source, const string &context) {
        if (source.kind == FireflyLlvmTypeKind::UNKNOWN) {
            return;
        }

        if (target.kind == FireflyLlvmTypeKind::UNKNOWN) {
            target = source;
            changed = true;
            return;
        }

        if (target.kind == FireflyLlvmTypeKind::VOID && source.kind != FireflyLlvmTypeKind::VOID) {
            target = source;
            changed = true;
            return;
        }

        if (target.kind == FireflyLlvmTypeKind::FUNCTION && source.kind == FireflyLlvmTypeKind::FUNCTION) {
            if (target.params.size() != source.params.size()) {
                errors.push_back("type mismatch in " + context + ": " + target.to_string() + " vs " + source.to_string());
                return;
            }

            for (size_t i = 0; i < target.params.size(); i++) {
                merge_into(target.params[i], source.params[i], context + " parameter " + std::to_string(i + 1));
            }

            if (source.return_type != nullptr) {
                if (target.return_type == nullptr) {
                    target.return_type = make_unique<FireflyLlvmType>(*source.return_type);
                    changed = true;
                } else {
                    merge_into(*target.return_type, *source.return_type, context + " return type");
                }
            }
            return;
        }

        if (!same_firefly_llvm_type(target, source)) {
            errors.push_back("type mismatch in " + context + ": " + target.to_string() + " vs " + source.to_string());
        }
    }
};

struct FireflyLlvmValue {
    llvm::Value *value{};
    FireflyLlvmType type;
};

struct FireflyLlvmVariable {
    llvm::Value *storage{};
    FireflyLlvmType type;
    bool is_mutable{true};
};

class FireflyLlvmCodegen {
public:
    explicit FireflyLlvmCodegen(string module_name)
        : context(make_unique<llvm::LLVMContext>()),
          module(make_unique<llvm::Module>(std::move(module_name), *context)),
          builder(make_unique<llvm::IRBuilder<>>(*context)) {}

    bool generate(Program *program) {
        FireflyLlvmFeatureChecker feature_checker;
        if (!feature_checker.check(program)) {
            errors = std::move(feature_checker.errors);
            return false;
        }

        FireflyLlvmTypeInferer inferer;
        inferer.infer(program);
        functions = std::move(inferer.functions);
        errors = inferer.errors;

        if (!errors.empty()) {
            return false;
        }

        declare_functions();
        emit_functions();
        emit_main(program);

        string verify_errors;
        llvm::raw_string_ostream verify_stream(verify_errors);
        if (llvm::verifyModule(*module, &verify_stream)) {
            verify_stream.flush();
            errors.push_back("LLVM verifier failed:\n" + verify_errors);
            return false;
        }

        return errors.empty();
    }

    string ir_string() {
        string output;
        llvm::raw_string_ostream stream(output);
        module->print(stream, nullptr);
        stream.flush();
        return output;
    }

    vector<string> errors;

private:
    unique_ptr<llvm::LLVMContext> context;
    unique_ptr<llvm::Module> module;
    unique_ptr<llvm::IRBuilder<>> builder;
    map<string, FireflyLlvmFunctionInfo> functions;
    map<string, llvm::Function *> llvm_functions;
    vector<map<string, FireflyLlvmVariable>> variable_scopes;
    llvm::Function *current_function{};

    llvm::Type *llvm_type(const FireflyLlvmType &type) {
        switch (type.kind) {
            case FireflyLlvmTypeKind::INT:
                return llvm::Type::getInt64Ty(*context);
            case FireflyLlvmTypeKind::BOOL:
                return llvm::Type::getInt1Ty(*context);
            case FireflyLlvmTypeKind::STRING:
                return llvm::PointerType::get(*context, 0);
            case FireflyLlvmTypeKind::VOID:
                return llvm::Type::getVoidTy(*context);
            case FireflyLlvmTypeKind::FUNCTION:
                return llvm::PointerType::get(*context, 0);
            case FireflyLlvmTypeKind::UNKNOWN:
                return llvm::Type::getInt64Ty(*context);
        }
        return llvm::Type::getInt64Ty(*context);
    }

    void declare_functions() {
        declare_runtime_functions();

        for (auto &entry : functions) {
            auto &info = entry.second;
            vector<llvm::Type *> params;
            for (auto &param : info.type.params) {
                params.push_back(llvm_type(param));
            }

            auto *fn_type = llvm::FunctionType::get(llvm_type(*info.type.return_type), params, false);
            auto *fn = llvm::Function::Create(fn_type, llvm::Function::ExternalLinkage, info.name, module.get());
            size_t index = 0;
            for (auto &arg : fn->args()) {
                if (index < info.literal->parameters.size()) {
                    arg.setName(info.literal->parameters[index]->value);
                }
                index++;
            }
            llvm_functions[info.name] = fn;
        }
    }

    void declare_runtime_functions() {
        auto *puts_type = llvm::FunctionType::get(llvm::Type::getInt32Ty(*context),
                                                  {llvm::PointerType::get(*context, 0)},
                                                  false);
        module->getOrInsertFunction("puts", puts_type);
    }

    void emit_functions() {
        for (auto &entry : functions) {
            emit_function(entry.second);
        }
    }

    void emit_function(FireflyLlvmFunctionInfo &info) {
        auto *fn = llvm_functions[info.name];
        auto *entry = llvm::BasicBlock::Create(*context, "entry", fn);
        builder->SetInsertPoint(entry);
        current_function = fn;
        begin_scope();

        size_t index = 0;
        for (auto &arg : fn->args()) {
            auto *alloca = create_entry_alloca(fn, string(arg.getName()), info.type.params[index]);
            builder->CreateStore(&arg, alloca);
            define_variable(string(arg.getName()), alloca, info.type.params[index]);
            index++;
        }

        auto result = emit_block(info.literal->body);
        if (!current_block_has_terminator()) {
            if (result.value != nullptr && result.type.kind != FireflyLlvmTypeKind::VOID) {
                builder->CreateRet(cast_value(result, *info.type.return_type));
            } else {
                emit_default_return(*info.type.return_type);
            }
        }

        end_scope();
        current_function = nullptr;
    }

    void emit_main(Program *program) {
        auto *fn_type = llvm::FunctionType::get(llvm::Type::getInt64Ty(*context), false);
        auto *fn = llvm::Function::Create(fn_type, llvm::Function::ExternalLinkage, "main", module.get());
        auto *entry = llvm::BasicBlock::Create(*context, "entry", fn);
        builder->SetInsertPoint(entry);
        current_function = fn;
        begin_scope();

        if (program != nullptr) {
            for (auto *stmt : program->statements) {
                auto *var_stmt = dynamic_cast<VarStatement *>(stmt);
                if (var_stmt != nullptr && dynamic_cast<FunctionLiteral *>(var_stmt->value) != nullptr) {
                    continue;
                }
                emit_statement(stmt);
                if (current_block_has_terminator()) {
                    break;
                }
            }
        }

        if (!current_block_has_terminator()) {
            builder->CreateRet(llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context), 0, true));
        }

        end_scope();
        current_function = nullptr;
    }

    void begin_scope() {
        variable_scopes.emplace_back();
    }

    void end_scope() {
        variable_scopes.pop_back();
    }

    void define_variable(const string &name, llvm::Value *storage, const FireflyLlvmType &type, bool is_mutable = true) {
        variable_scopes.back()[name] = FireflyLlvmVariable{storage, type, is_mutable};
    }

    FireflyLlvmVariable *resolve_variable(const string &name) {
        for (auto it = variable_scopes.rbegin(); it != variable_scopes.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) {
                return &found->second;
            }
        }
        return nullptr;
    }

    llvm::AllocaInst *create_entry_alloca(llvm::Function *fn, const string &name, const FireflyLlvmType &type) {
        llvm::IRBuilder<> tmp(&fn->getEntryBlock(), fn->getEntryBlock().begin());
        return tmp.CreateAlloca(llvm_type(type), nullptr, name);
    }

    bool current_block_has_terminator() const {
        auto *block = builder->GetInsertBlock();
        return block != nullptr && block->getTerminator() != nullptr;
    }

    void emit_default_return(const FireflyLlvmType &type) {
        switch (type.kind) {
            case FireflyLlvmTypeKind::BOOL:
                builder->CreateRet(llvm::ConstantInt::getFalse(*context));
                break;
            case FireflyLlvmTypeKind::STRING:
                builder->CreateRet(llvm::ConstantPointerNull::get(llvm::PointerType::get(*context, 0)));
                break;
            case FireflyLlvmTypeKind::VOID:
                builder->CreateRetVoid();
                break;
            case FireflyLlvmTypeKind::INT:
            case FireflyLlvmTypeKind::UNKNOWN:
            case FireflyLlvmTypeKind::FUNCTION:
                builder->CreateRet(llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context), 0, true));
                break;
        }
    }

    FireflyLlvmValue emit_statement(Statement *stmt) {
        if (stmt == nullptr || current_block_has_terminator()) {
            return FireflyLlvmValue{nullptr, FireflyLlvmType::void_type()};
        }

        if (auto *var_stmt = dynamic_cast<VarStatement *>(stmt)) {
            return emit_var_statement(var_stmt);
        }

        if (auto *return_stmt = dynamic_cast<ReturnStatement *>(stmt)) {
            auto value = emit_expression(return_stmt->return_value);
            builder->CreateRet(cast_value(value, function_return_type(current_function)));
            return value;
        }

        if (auto *expression_stmt = dynamic_cast<ExpressionStatement *>(stmt)) {
            return emit_expression(expression_stmt->expression);
        }

        if (auto *block_stmt = dynamic_cast<BlockStatement *>(stmt)) {
            return emit_block(block_stmt);
        }

        errors.push_back("unsupported statement for --emit-llvm: " + stmt->to_string());
        return FireflyLlvmValue{nullptr, FireflyLlvmType::unknown()};
    }

    FireflyLlvmValue emit_var_statement(VarStatement *stmt) {
        if (stmt->name == nullptr) {
            return FireflyLlvmValue{nullptr, FireflyLlvmType::void_type()};
        }

        if (dynamic_cast<FunctionLiteral *>(stmt->value) != nullptr) {
            return FireflyLlvmValue{llvm_functions[stmt->name->value], functions[stmt->name->value].type};
        }

        auto value = emit_expression(stmt->value);
        auto *alloca = create_entry_alloca(current_function, stmt->name->value, value.type);
        builder->CreateStore(value.value, alloca);
        define_variable(stmt->name->value, alloca, value.type, stmt->is_mutable);
        return value;
    }

    FireflyLlvmValue emit_block(BlockStatement *block) {
        begin_scope();
        FireflyLlvmValue result{nullptr, FireflyLlvmType::void_type()};
        if (block != nullptr) {
            for (auto *stmt : block->statements) {
                result = emit_statement(stmt);
                if (current_block_has_terminator()) {
                    break;
                }
            }
        }
        end_scope();
        return result;
    }

    FireflyLlvmValue emit_expression(Expression *expression) {
        if (expression == nullptr) {
            return FireflyLlvmValue{nullptr, FireflyLlvmType::void_type()};
        }

        if (auto *integer = dynamic_cast<IntegerLiteral *>(expression)) {
            return FireflyLlvmValue{
                    llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context), integer->value, true),
                    FireflyLlvmType::int_type()};
        }

        if (auto *boolean = dynamic_cast<Boolean *>(expression)) {
            return FireflyLlvmValue{
                    llvm::ConstantInt::get(llvm::Type::getInt1Ty(*context), boolean->value ? 1 : 0, false),
                    FireflyLlvmType::bool_type()};
        }

        if (auto *string_literal = dynamic_cast<StringLiteral *>(expression)) {
            return FireflyLlvmValue{builder->CreateGlobalStringPtr(string_literal->value), FireflyLlvmType::string_type()};
        }

        if (auto *ident = dynamic_cast<Identifier *>(expression)) {
            return emit_identifier(ident);
        }

        if (auto *prefix = dynamic_cast<PrefixExpression *>(expression)) {
            return emit_prefix_expression(prefix);
        }

        if (auto *infix = dynamic_cast<InfixExpression *>(expression)) {
            return emit_infix_expression(infix);
        }

        if (auto *if_exp = dynamic_cast<IfExpression *>(expression)) {
            return emit_if_expression(if_exp);
        }

        if (auto *while_exp = dynamic_cast<WhileExpression *>(expression)) {
            return emit_while_expression(while_exp);
        }

        if (auto *call = dynamic_cast<CallExpression *>(expression)) {
            return emit_call_expression(call);
        }

        if (auto *assign = dynamic_cast<AssignExpression *>(expression)) {
            return emit_assign_expression(assign);
        }

        if (dynamic_cast<FunctionLiteral *>(expression) != nullptr) {
            errors.push_back("function literals must be bound with var before --emit-llvm can emit them");
            return FireflyLlvmValue{nullptr, FireflyLlvmType::unknown()};
        }

        errors.push_back("unsupported expression for --emit-llvm: " + expression->to_string());
        return FireflyLlvmValue{nullptr, FireflyLlvmType::unknown()};
    }

    FireflyLlvmValue emit_assign_expression(AssignExpression *assign) {
        auto *ident = dynamic_cast<Identifier *>(assign->left);
        if (ident == nullptr) {
            errors.push_back("only identifier assignment is supported by --emit-llvm yet");
            return FireflyLlvmValue{nullptr, FireflyLlvmType::unknown()};
        }

        auto *variable = resolve_variable(ident->value);
        if (variable == nullptr) {
            errors.push_back("undefined identifier in LLVM backend assignment: " + ident->value);
            return FireflyLlvmValue{nullptr, FireflyLlvmType::unknown()};
        }

        if (!variable->is_mutable) {
            errors.push_back("cannot assign to immutable binding: " + ident->value);
            return FireflyLlvmValue{nullptr, variable->type};
        }

        auto value = emit_expression(assign->value);
        auto *stored = cast_value(value, variable->type);
        builder->CreateStore(stored, variable->storage);
        return FireflyLlvmValue{stored, variable->type};
    }

    FireflyLlvmValue emit_identifier(Identifier *ident) {
        auto found_function = llvm_functions.find(ident->value);
        if (found_function != llvm_functions.end()) {
            return FireflyLlvmValue{found_function->second, functions[ident->value].type};
        }

        auto *variable = resolve_variable(ident->value);
        if (variable == nullptr) {
            errors.push_back("undefined identifier in LLVM backend: " + ident->value);
            return FireflyLlvmValue{nullptr, FireflyLlvmType::unknown()};
        }

        return FireflyLlvmValue{
                builder->CreateLoad(llvm_type(variable->type), variable->storage, ident->value),
                variable->type};
    }

    FireflyLlvmValue emit_prefix_expression(PrefixExpression *prefix) {
        auto right = emit_expression(prefix->right);
        if (prefix->op == "-") {
            return FireflyLlvmValue{builder->CreateNeg(cast_value(right, FireflyLlvmType::int_type()), "negtmp"),
                                    FireflyLlvmType::int_type()};
        }

        if (prefix->op == "!") {
            auto *condition = truthy_value(right);
            return FireflyLlvmValue{builder->CreateNot(condition, "nottmp"), FireflyLlvmType::bool_type()};
        }

        errors.push_back("unsupported prefix operator for --emit-llvm: " + prefix->op);
        return FireflyLlvmValue{nullptr, FireflyLlvmType::unknown()};
    }

    FireflyLlvmValue emit_infix_expression(InfixExpression *infix) {
        if (infix->op == "&&" || infix->op == "||") {
            return emit_logical_expression(infix);
        }

        auto left = emit_expression(infix->left);
        auto right = emit_expression(infix->right);

        if (infix->op == "+") {
            return FireflyLlvmValue{builder->CreateAdd(cast_value(left, FireflyLlvmType::int_type()),
                                                       cast_value(right, FireflyLlvmType::int_type()), "addtmp"),
                                    FireflyLlvmType::int_type()};
        }
        if (infix->op == "-") {
            return FireflyLlvmValue{builder->CreateSub(cast_value(left, FireflyLlvmType::int_type()),
                                                       cast_value(right, FireflyLlvmType::int_type()), "subtmp"),
                                    FireflyLlvmType::int_type()};
        }
        if (infix->op == "*") {
            return FireflyLlvmValue{builder->CreateMul(cast_value(left, FireflyLlvmType::int_type()),
                                                       cast_value(right, FireflyLlvmType::int_type()), "multmp"),
                                    FireflyLlvmType::int_type()};
        }
        if (infix->op == "/") {
            return FireflyLlvmValue{builder->CreateSDiv(cast_value(left, FireflyLlvmType::int_type()),
                                                        cast_value(right, FireflyLlvmType::int_type()), "divtmp"),
                                    FireflyLlvmType::int_type()};
        }
        if (infix->op == "<") {
            return compare_int(left, right, llvm::CmpInst::ICMP_SLT, "lttmp");
        }
        if (infix->op == ">") {
            return compare_int(left, right, llvm::CmpInst::ICMP_SGT, "gttmp");
        }
        if (infix->op == "<=") {
            return compare_int(left, right, llvm::CmpInst::ICMP_SLE, "letmp");
        }
        if (infix->op == ">=") {
            return compare_int(left, right, llvm::CmpInst::ICMP_SGE, "getmp");
        }
        if (infix->op == "==") {
            return compare_same_type(left, right, llvm::CmpInst::ICMP_EQ, "eqtmp");
        }
        if (infix->op == "!=") {
            return compare_same_type(left, right, llvm::CmpInst::ICMP_NE, "netmp");
        }

        errors.push_back("unsupported infix operator for --emit-llvm: " + infix->op);
        return FireflyLlvmValue{nullptr, FireflyLlvmType::unknown()};
    }

    FireflyLlvmValue emit_logical_expression(InfixExpression *infix) {
        auto left = emit_expression(infix->left);
        auto *left_bool = truthy_value(left);
        auto *fn = current_function;

        auto *rhs_block = llvm::BasicBlock::Create(*context, "logic.rhs", fn);
        auto *merge_block = llvm::BasicBlock::Create(*context, "logic.end", fn);
        auto *left_block = builder->GetInsertBlock();

        if (infix->op == "&&") {
            builder->CreateCondBr(left_bool, rhs_block, merge_block);
        } else {
            builder->CreateCondBr(left_bool, merge_block, rhs_block);
        }

        builder->SetInsertPoint(rhs_block);
        auto right = emit_expression(infix->right);
        auto *right_bool = truthy_value(right);
        auto *right_block = builder->GetInsertBlock();
        builder->CreateBr(merge_block);

        builder->SetInsertPoint(merge_block);
        auto *phi = builder->CreatePHI(llvm::Type::getInt1Ty(*context), 2, "logictmp");
        phi->addIncoming(infix->op == "&&" ? llvm::ConstantInt::getFalse(*context) : llvm::ConstantInt::getTrue(*context), left_block);
        phi->addIncoming(right_bool, right_block);
        return FireflyLlvmValue{phi, FireflyLlvmType::bool_type()};
    }

    FireflyLlvmValue emit_if_expression(IfExpression *if_exp) {
        auto condition = emit_expression(if_exp->condition);
        auto *condition_value = truthy_value(condition);
        auto *fn = current_function;

        auto *then_block = llvm::BasicBlock::Create(*context, "if.then", fn);
        auto *else_block = llvm::BasicBlock::Create(*context, "if.else");
        auto *merge_block = llvm::BasicBlock::Create(*context, "if.end");

        builder->CreateCondBr(condition_value, then_block, if_exp->alternative == nullptr ? merge_block : else_block);

        builder->SetInsertPoint(then_block);
        auto then_value = emit_block(if_exp->consequence);
        auto *then_end = builder->GetInsertBlock();
        bool then_returns = current_block_has_terminator();
        if (!then_returns) {
            builder->CreateBr(merge_block);
        }

        FireflyLlvmValue else_value{nullptr, FireflyLlvmType::void_type()};
        llvm::BasicBlock *else_end = nullptr;
        bool else_returns = false;
        if (if_exp->alternative != nullptr) {
            fn->insert(fn->end(), else_block);
            builder->SetInsertPoint(else_block);
            else_value = emit_block(if_exp->alternative);
            else_end = builder->GetInsertBlock();
            else_returns = current_block_has_terminator();
            if (!else_returns) {
                builder->CreateBr(merge_block);
            }
        }

        fn->insert(fn->end(), merge_block);
        builder->SetInsertPoint(merge_block);

        if (then_returns && (if_exp->alternative == nullptr || else_returns)) {
            auto *after_block = llvm::BasicBlock::Create(*context, "if.after", fn);
            builder->SetInsertPoint(after_block);
            return FireflyLlvmValue{nullptr, FireflyLlvmType::void_type()};
        }

        if (if_exp->alternative != nullptr &&
            !then_returns && !else_returns &&
            then_value.value != nullptr &&
            else_value.value != nullptr &&
            same_firefly_llvm_type(then_value.type, else_value.type) &&
            then_value.type.kind != FireflyLlvmTypeKind::VOID) {
            auto *phi = builder->CreatePHI(llvm_type(then_value.type), 2, "iftmp");
            phi->addIncoming(then_value.value, then_end);
            phi->addIncoming(else_value.value, else_end);
            return FireflyLlvmValue{phi, then_value.type};
        }

        return FireflyLlvmValue{nullptr, FireflyLlvmType::void_type()};
    }

    FireflyLlvmValue emit_while_expression(WhileExpression *while_exp) {
        auto *fn = current_function;
        auto *condition_block = llvm::BasicBlock::Create(*context, "while.cond", fn);
        auto *body_block = llvm::BasicBlock::Create(*context, "while.body");
        auto *after_block = llvm::BasicBlock::Create(*context, "while.end");

        builder->CreateBr(condition_block);

        builder->SetInsertPoint(condition_block);
        auto condition = emit_expression(while_exp->condition);
        builder->CreateCondBr(truthy_value(condition), body_block, after_block);

        fn->insert(fn->end(), body_block);
        builder->SetInsertPoint(body_block);
        emit_block(while_exp->body);
        if (!current_block_has_terminator()) {
            builder->CreateBr(condition_block);
        }

        fn->insert(fn->end(), after_block);
        builder->SetInsertPoint(after_block);
        return FireflyLlvmValue{nullptr, FireflyLlvmType::void_type()};
    }

    FireflyLlvmValue emit_call_expression(CallExpression *call) {
        auto *ident = dynamic_cast<Identifier *>(call->function);
        if (ident == nullptr) {
            errors.push_back("only calls to named functions are supported by --emit-llvm yet");
            return FireflyLlvmValue{nullptr, FireflyLlvmType::unknown()};
        }

        if (ident->value == "puts") {
            return emit_puts_call(call);
        }

        auto found = llvm_functions.find(ident->value);
        if (found == llvm_functions.end()) {
            errors.push_back("unknown function for --emit-llvm: " + ident->value);
            return FireflyLlvmValue{nullptr, FireflyLlvmType::unknown()};
        }

        auto &fn_info = functions[ident->value];
        vector<llvm::Value *> args;
        for (size_t i = 0; i < call->arguments.size(); i++) {
            auto value = emit_expression(call->arguments[i]);
            args.push_back(cast_value(value, fn_info.type.params[i]));
        }

        auto *call_value = builder->CreateCall(found->second, args, "calltmp");
        return FireflyLlvmValue{call_value, *fn_info.type.return_type};
    }

    FireflyLlvmValue emit_puts_call(CallExpression *call) {
        if (call->arguments.size() != 1) {
            errors.push_back("wrong number of arguments for puts: expected 1, got " +
                             std::to_string(call->arguments.size()));
            return FireflyLlvmValue{nullptr, FireflyLlvmType::unknown()};
        }

        auto value = emit_expression(call->arguments[0]);
        auto callee = module->getOrInsertFunction("puts",
                                                  llvm::FunctionType::get(llvm::Type::getInt32Ty(*context),
                                                                          {llvm::PointerType::get(*context, 0)},
                                                                          false));
        builder->CreateCall(callee, {cast_value(value, FireflyLlvmType::string_type())});
        return FireflyLlvmValue{nullptr, FireflyLlvmType::void_type()};
    }

    FireflyLlvmValue compare_int(const FireflyLlvmValue &left,
                                 const FireflyLlvmValue &right,
                                 llvm::CmpInst::Predicate predicate,
                                 const string &name) {
        return FireflyLlvmValue{builder->CreateICmp(predicate,
                                                    cast_value(left, FireflyLlvmType::int_type()),
                                                    cast_value(right, FireflyLlvmType::int_type()),
                                                    name),
                                FireflyLlvmType::bool_type()};
    }

    FireflyLlvmValue compare_same_type(const FireflyLlvmValue &left,
                                       const FireflyLlvmValue &right,
                                       llvm::CmpInst::Predicate predicate,
                                       const string &name) {
        auto type = left.type.kind == FireflyLlvmTypeKind::UNKNOWN ? right.type : left.type;
        return FireflyLlvmValue{builder->CreateICmp(predicate, cast_value(left, type), cast_value(right, type), name),
                                FireflyLlvmType::bool_type()};
    }

    llvm::Value *truthy_value(const FireflyLlvmValue &value) {
        if (value.type.kind == FireflyLlvmTypeKind::BOOL) {
            return value.value;
        }

        if (value.type.kind == FireflyLlvmTypeKind::INT) {
            return builder->CreateICmpNE(value.value, llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context), 0), "truthy");
        }

        if (value.type.kind == FireflyLlvmTypeKind::STRING) {
            return builder->CreateICmpNE(value.value,
                                         llvm::ConstantPointerNull::get(llvm::PointerType::get(*context, 0)),
                                         "truthy");
        }

        return llvm::ConstantInt::getTrue(*context);
    }

    llvm::Value *cast_value(const FireflyLlvmValue &value, const FireflyLlvmType &target) {
        if (value.value == nullptr) {
            return llvm::UndefValue::get(llvm_type(target));
        }

        if (same_firefly_llvm_type(value.type, target)) {
            return value.value;
        }

        if (value.type.kind == FireflyLlvmTypeKind::BOOL && target.kind == FireflyLlvmTypeKind::INT) {
            return builder->CreateZExt(value.value, llvm::Type::getInt64Ty(*context), "booltoint");
        }

        if (value.type.kind == FireflyLlvmTypeKind::INT && target.kind == FireflyLlvmTypeKind::BOOL) {
            return builder->CreateICmpNE(value.value, llvm::ConstantInt::get(llvm::Type::getInt64Ty(*context), 0), "inttobool");
        }

        return value.value;
    }

    FireflyLlvmType function_return_type(llvm::Function *fn) const {
        auto *type = fn->getReturnType();
        if (type->isIntegerTy(1)) {
            return FireflyLlvmType::bool_type();
        }
        if (type->isIntegerTy(64)) {
            return FireflyLlvmType::int_type();
        }
        if (type->isPointerTy()) {
            return FireflyLlvmType::string_type();
        }
        if (type->isVoidTy()) {
            return FireflyLlvmType::void_type();
        }
        return FireflyLlvmType::unknown();
    }
};

inline bool compile_program_to_llvm_ir(Program *program, const string &module_name, string &ir, vector<string> &errors) {
    FireflyLlvmCodegen codegen(module_name);
    if (!codegen.generate(program)) {
        errors = codegen.errors;
        return false;
    }

    ir = codegen.ir_string();
    return true;
}

#endif //FIREFLY_COMPILER_LLVM_CODEGEN_HPP
