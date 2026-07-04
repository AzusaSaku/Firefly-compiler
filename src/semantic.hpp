/* 语义分析器 */

#ifndef FIREFLY_COMPILER_SEMANTIC_HPP
#define FIREFLY_COMPILER_SEMANTIC_HPP

#include "ast.hpp"
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace std;

enum class SemanticTypeKind {
    UNKNOWN,
    VOID,
    INT,
    BOOL,
    STRING,
    NULL_TYPE,
    ARRAY,
    HASH,
    REFERENCE,
    FUNCTION
};

struct SemanticType {
    SemanticTypeKind kind{SemanticTypeKind::UNKNOWN};
    vector<SemanticType> params;
    unique_ptr<SemanticType> element_type;
    unique_ptr<SemanticType> referenced_type;
    unique_ptr<SemanticType> return_type;
    bool is_mutable_reference{};

    SemanticType() = default;

    explicit SemanticType(SemanticTypeKind kind) : kind(kind) {}

    SemanticType(const SemanticType &other) : kind(other.kind), params(other.params) {
        if (other.element_type != nullptr) {
            element_type = make_unique<SemanticType>(*other.element_type);
        }
        if (other.referenced_type != nullptr) {
            referenced_type = make_unique<SemanticType>(*other.referenced_type);
        }
        if (other.return_type != nullptr) {
            return_type = make_unique<SemanticType>(*other.return_type);
        }
        is_mutable_reference = other.is_mutable_reference;
    }

    SemanticType &operator=(const SemanticType &other) {
        if (this == &other) {
            return *this;
        }

        kind = other.kind;
        params = other.params;
        if (other.element_type != nullptr) {
            element_type = make_unique<SemanticType>(*other.element_type);
        } else {
            element_type.reset();
        }
        if (other.referenced_type != nullptr) {
            referenced_type = make_unique<SemanticType>(*other.referenced_type);
        } else {
            referenced_type.reset();
        }
        if (other.return_type != nullptr) {
            return_type = make_unique<SemanticType>(*other.return_type);
        } else {
            return_type.reset();
        }
        is_mutable_reference = other.is_mutable_reference;
        return *this;
    }

    static SemanticType unknown() {
        return SemanticType(SemanticTypeKind::UNKNOWN);
    }

    static SemanticType void_type() {
        return SemanticType(SemanticTypeKind::VOID);
    }

    static SemanticType int_type() {
        return SemanticType(SemanticTypeKind::INT);
    }

    static SemanticType bool_type() {
        return SemanticType(SemanticTypeKind::BOOL);
    }

    static SemanticType string_type() {
        return SemanticType(SemanticTypeKind::STRING);
    }

    static SemanticType null_type() {
        return SemanticType(SemanticTypeKind::NULL_TYPE);
    }

    static SemanticType array_type(SemanticType element = SemanticType::unknown()) {
        SemanticType type(SemanticTypeKind::ARRAY);
        type.element_type = make_unique<SemanticType>(std::move(element));
        return type;
    }

    static SemanticType hash_type() {
        return SemanticType(SemanticTypeKind::HASH);
    }

    static SemanticType reference_type(SemanticType referenced, bool is_mutable) {
        SemanticType type(SemanticTypeKind::REFERENCE);
        type.referenced_type = make_unique<SemanticType>(std::move(referenced));
        type.is_mutable_reference = is_mutable;
        return type;
    }

    static SemanticType function_type(vector<SemanticType> params, SemanticType result) {
        SemanticType type(SemanticTypeKind::FUNCTION);
        type.params = std::move(params);
        type.return_type = make_unique<SemanticType>(std::move(result));
        return type;
    }

    bool is_unknown() const {
        return kind == SemanticTypeKind::UNKNOWN;
    }

    string to_string() const {
        switch (kind) {
            case SemanticTypeKind::UNKNOWN:
                return "unknown";
            case SemanticTypeKind::VOID:
                return "void";
            case SemanticTypeKind::INT:
                return "int";
            case SemanticTypeKind::BOOL:
                return "bool";
            case SemanticTypeKind::STRING:
                return "string";
            case SemanticTypeKind::NULL_TYPE:
                return "null";
            case SemanticTypeKind::ARRAY:
                if (element_type != nullptr && !element_type->is_unknown()) {
                    return "array<" + element_type->to_string() + ">";
                }
                return "array";
            case SemanticTypeKind::HASH:
                return "hash";
            case SemanticTypeKind::REFERENCE:
                return string("&") + (is_mutable_reference ? "var " : "") +
                       (referenced_type == nullptr ? "unknown" : referenced_type->to_string());
            case SemanticTypeKind::FUNCTION: {
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

inline bool same_semantic_type(const SemanticType &left, const SemanticType &right) {
    if (left.kind != right.kind) {
        return false;
    }

    if (left.kind == SemanticTypeKind::ARRAY) {
        if (left.element_type == nullptr || right.element_type == nullptr) {
            return left.element_type == nullptr && right.element_type == nullptr;
        }
        return same_semantic_type(*left.element_type, *right.element_type);
    }

    if (left.kind == SemanticTypeKind::REFERENCE) {
        if (left.is_mutable_reference != right.is_mutable_reference) {
            return false;
        }
        if (left.referenced_type == nullptr || right.referenced_type == nullptr) {
            return left.referenced_type == nullptr && right.referenced_type == nullptr;
        }
        return same_semantic_type(*left.referenced_type, *right.referenced_type);
    }

    if (left.kind != SemanticTypeKind::FUNCTION) {
        return true;
    }

    if (left.params.size() != right.params.size()) {
        return false;
    }

    for (size_t i = 0; i < left.params.size(); i++) {
        if (!same_semantic_type(left.params[i], right.params[i])) {
            return false;
        }
    }

    if (left.return_type == nullptr || right.return_type == nullptr) {
        return left.return_type == nullptr && right.return_type == nullptr;
    }

    return same_semantic_type(*left.return_type, *right.return_type);
}

struct SemanticSymbol {
    SemanticType type;
    bool is_mutable{true};
    bool is_moved{false};
    int immutable_borrows{};
    int mutable_borrows{};
};

struct SemanticBorrowRecord {
    SemanticSymbol *symbol{};
    bool is_mutable{};
};

enum class SemanticUseMode {
    READ,
    MOVE
};

class SemanticAnalyzer {
public:
    vector<string> errors;

    SemanticAnalyzer() {
        begin_scope();
    }

    void analyze(Program *program) {
        if (program == nullptr) {
            return;
        }

        for (auto &stmt : program->statements) {
            analyze_statement(stmt);
        }
    }

private:
    vector<map<string, SemanticSymbol>> scopes;
    vector<vector<SemanticBorrowRecord>> borrow_scopes;
    vector<SemanticType> function_returns;

    void begin_scope() {
        scopes.emplace_back();
        borrow_scopes.emplace_back();
    }

    void end_scope() {
        if (!borrow_scopes.empty()) {
            release_borrows_from(borrow_scopes.size() - 1, 0);
            borrow_scopes.pop_back();
        }
        if (!scopes.empty()) {
            scopes.pop_back();
        }
    }

    bool define(const string &name, const SemanticType &type, bool is_mutable = true) {
        if (scopes.empty()) {
            begin_scope();
        }

        auto &scope = scopes.back();
        if (scope.find(name) != scope.end()) {
            add_error("identifier already defined: " + name);
            return false;
        }

        scope[name] = SemanticSymbol{type, is_mutable, false};
        return true;
    }

    SemanticSymbol *resolve(const string &name) {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) {
                return &found->second;
            }
        }
        return nullptr;
    }

    void add_error(const string &msg) {
        errors.push_back(msg);
    }

    bool has_active_borrows(const SemanticSymbol &symbol) const {
        return symbol.immutable_borrows > 0 || symbol.mutable_borrows > 0;
    }

    size_t current_borrow_count() const {
        if (borrow_scopes.empty()) {
            return 0;
        }
        return borrow_scopes.back().size();
    }

    void register_borrow(SemanticSymbol *symbol, bool is_mutable) {
        if (symbol == nullptr) {
            return;
        }
        if (borrow_scopes.empty()) {
            begin_scope();
        }

        if (is_mutable) {
            symbol->mutable_borrows++;
        } else {
            symbol->immutable_borrows++;
        }
        borrow_scopes.back().push_back(SemanticBorrowRecord{symbol, is_mutable});
    }

    void release_borrows_from(size_t scope_index, size_t first_record) {
        if (scope_index >= borrow_scopes.size()) {
            return;
        }

        auto &records = borrow_scopes[scope_index];
        if (first_record > records.size()) {
            return;
        }

        for (size_t i = first_record; i < records.size(); i++) {
            auto &record = records[i];
            if (record.symbol == nullptr) {
                continue;
            }
            if (record.is_mutable) {
                record.symbol->mutable_borrows--;
            } else {
                record.symbol->immutable_borrows--;
            }
        }
        records.erase(records.begin() + static_cast<long long>(first_record), records.end());
    }

    void release_current_borrows_from(size_t first_record) {
        if (borrow_scopes.empty()) {
            return;
        }
        release_borrows_from(borrow_scopes.size() - 1, first_record);
    }

    Identifier *root_identifier(Expression *expression) {
        if (expression == nullptr) {
            return nullptr;
        }
        if (auto *ident = dynamic_cast<Identifier *>(expression)) {
            return ident;
        }
        if (auto *index = dynamic_cast<IndexExpression *>(expression)) {
            return root_identifier(index->left);
        }
        return nullptr;
    }

    bool is_builtin_identifier(const string &name) const {
        return name == "len" || name == "first" || name == "last" ||
               name == "rest" || name == "push" || name == "puts";
    }

    SemanticType builtin_type(const string &name) const {
        if (name == "len") {
            return SemanticType::function_type({SemanticType::unknown()}, SemanticType::int_type());
        }
        return SemanticType::unknown();
    }

    SemanticType type_from_annotation(const string &annotation, const string &context) {
        if (annotation.empty()) {
            return SemanticType::unknown();
        }
        if (annotation.rfind("&var ", 0) == 0) {
            auto referenced_annotation = annotation.substr(5);
            return SemanticType::reference_type(type_from_annotation(referenced_annotation, context + " reference"), true);
        }
        if (annotation[0] == '&') {
            auto referenced_annotation = annotation.substr(1);
            return SemanticType::reference_type(type_from_annotation(referenced_annotation, context + " reference"), false);
        }
        if (annotation == "int") {
            return SemanticType::int_type();
        }
        if (annotation == "bool") {
            return SemanticType::bool_type();
        }
        if (annotation == "string") {
            return SemanticType::string_type();
        }
        if (annotation == "array") {
            return SemanticType::array_type();
        }
        if (annotation.rfind("array<", 0) == 0 && annotation.size() > 7 && annotation.back() == '>') {
            auto element_annotation = annotation.substr(6, annotation.size() - 7);
            return SemanticType::array_type(type_from_annotation(element_annotation, context + " element"));
        }
        if (annotation == "hash") {
            return SemanticType::hash_type();
        }
        if (annotation == "void") {
            return SemanticType::void_type();
        }

        add_error("unknown type annotation in " + context + ": " + annotation);
        return SemanticType::unknown();
    }

    vector<SemanticType> parameter_types(FunctionLiteral *function) {
        vector<SemanticType> params;
        if (function == nullptr) {
            return params;
        }

        for (auto *param : function->parameters) {
            if (param == nullptr) {
                params.push_back(SemanticType::unknown());
                continue;
            }
            params.push_back(type_from_annotation(param->type_annotation, "parameter '" + param->value + "'"));
        }
        return params;
    }

    bool compatible(const SemanticType &left, const SemanticType &right) const {
        if (left.is_unknown() || right.is_unknown()) {
            return true;
        }

        if (left.kind == SemanticTypeKind::ARRAY && right.kind == SemanticTypeKind::ARRAY) {
            if (left.element_type == nullptr || right.element_type == nullptr) {
                return true;
            }
            return compatible(*left.element_type, *right.element_type);
        }

        if (left.kind == SemanticTypeKind::REFERENCE && right.kind == SemanticTypeKind::REFERENCE) {
            if (left.is_mutable_reference != right.is_mutable_reference) {
                return false;
            }
            if (left.referenced_type == nullptr || right.referenced_type == nullptr) {
                return true;
            }
            return compatible(*left.referenced_type, *right.referenced_type);
        }

        if (left.kind == SemanticTypeKind::FUNCTION && right.kind == SemanticTypeKind::FUNCTION) {
            if (left.params.size() != right.params.size()) {
                return false;
            }
            for (size_t i = 0; i < left.params.size(); i++) {
                if (!compatible(left.params[i], right.params[i])) {
                    return false;
                }
            }
            if (left.return_type == nullptr || right.return_type == nullptr) {
                return true;
            }
            return compatible(*left.return_type, *right.return_type);
        }

        return same_semantic_type(left, right);
    }

    bool is_copy_type(const SemanticType &type) const {
        return type.kind == SemanticTypeKind::UNKNOWN ||
               type.kind == SemanticTypeKind::VOID ||
               type.kind == SemanticTypeKind::INT ||
               type.kind == SemanticTypeKind::BOOL ||
               type.kind == SemanticTypeKind::NULL_TYPE ||
               type.kind == SemanticTypeKind::REFERENCE ||
               type.kind == SemanticTypeKind::FUNCTION;
    }

    bool is_reference_type(const SemanticType &type) const {
        return type.kind == SemanticTypeKind::REFERENCE;
    }

    void merge_type(SemanticType &target, const SemanticType &source, const string &context) {
        if (source.is_unknown()) {
            return;
        }

        if (target.is_unknown()) {
            target = source;
            return;
        }

        if (target.kind == SemanticTypeKind::ARRAY && source.kind == SemanticTypeKind::ARRAY) {
            if (target.element_type == nullptr && source.element_type != nullptr) {
                target.element_type = make_unique<SemanticType>(*source.element_type);
            } else if (target.element_type != nullptr && source.element_type != nullptr) {
                merge_type(*target.element_type, *source.element_type, context + " element");
            }
            return;
        }

        if (target.kind == SemanticTypeKind::REFERENCE && source.kind == SemanticTypeKind::REFERENCE) {
            if (target.is_mutable_reference != source.is_mutable_reference) {
                add_error("type mismatch in " + context + ": " + target.to_string() + " vs " + source.to_string());
                return;
            }
            if (target.referenced_type == nullptr && source.referenced_type != nullptr) {
                target.referenced_type = make_unique<SemanticType>(*source.referenced_type);
            } else if (target.referenced_type != nullptr && source.referenced_type != nullptr) {
                merge_type(*target.referenced_type, *source.referenced_type, context + " reference");
            }
            return;
        }

        if (target.kind == SemanticTypeKind::FUNCTION && source.kind == SemanticTypeKind::FUNCTION) {
            if (target.params.size() != source.params.size()) {
                add_error("type mismatch in " + context + ": " + target.to_string() + " vs " + source.to_string());
                return;
            }

            for (size_t i = 0; i < target.params.size(); i++) {
                merge_type(target.params[i], source.params[i], context + " parameter " + std::to_string(i + 1));
            }

            if (target.return_type == nullptr && source.return_type != nullptr) {
                target.return_type = make_unique<SemanticType>(*source.return_type);
            } else if (target.return_type != nullptr && source.return_type != nullptr) {
                merge_type(*target.return_type, *source.return_type, context + " return type");
            }
            return;
        }

        if (target.kind != source.kind) {
            add_error("type mismatch in " + context + ": " + target.to_string() + " vs " + source.to_string());
        }
    }

    void require_type(const SemanticType &actual, const SemanticType &expected, const string &context) {
        if (actual.is_unknown()) {
            return;
        }

        if (!compatible(actual, expected)) {
            add_error(context + " must be " + expected.to_string() + ", got " + actual.to_string());
        }
    }

    SemanticType analyze_statement(Statement *stmt) {
        if (stmt == nullptr) {
            return SemanticType::void_type();
        }

        if (auto *var_stmt = dynamic_cast<VarStatement *>(stmt)) {
            return analyze_var_statement(var_stmt);
        }
        if (auto *return_stmt = dynamic_cast<ReturnStatement *>(stmt)) {
            return analyze_return_statement(return_stmt);
        }
        if (auto *expression_stmt = dynamic_cast<ExpressionStatement *>(stmt)) {
            return analyze_expression(expression_stmt->expression);
        }
        if (auto *block_stmt = dynamic_cast<BlockStatement *>(stmt)) {
            return analyze_block_statement(block_stmt);
        }

        return SemanticType::void_type();
    }

    SemanticType analyze_var_statement(VarStatement *stmt) {
        if (stmt->name == nullptr) {
            return SemanticType::void_type();
        }

        if (auto *function = dynamic_cast<FunctionLiteral *>(stmt->value)) {
            auto params = parameter_types(function);
            auto return_type = type_from_annotation(function->return_type_annotation, "function return");
            SemanticType placeholder = SemanticType::function_type(params, return_type);
            define(stmt->name->value, placeholder, stmt->is_mutable);

            auto type = analyze_function_literal(function);
            auto *symbol = resolve(stmt->name->value);
            if (symbol != nullptr) {
                merge_type(symbol->type, type, "binding '" + stmt->name->value + "'");
            }

            auto declared = type_from_annotation(stmt->name->type_annotation, "binding '" + stmt->name->value + "'");
            if (!declared.is_unknown()) {
                merge_type(declared, type, "binding '" + stmt->name->value + "'");
            }
            return type;
        }

        auto type = analyze_expression(stmt->value);
        auto declared = type_from_annotation(stmt->name->type_annotation, "binding '" + stmt->name->value + "'");
        if (!declared.is_unknown()) {
            merge_type(declared, type, "binding '" + stmt->name->value + "'");
            type = declared;
        }
        define(stmt->name->value, type, stmt->is_mutable);
        return type;
    }

    SemanticType analyze_return_statement(ReturnStatement *stmt) {
        if (function_returns.empty()) {
            add_error("return statement outside function");
            analyze_expression(stmt->return_value, SemanticUseMode::MOVE);
            return SemanticType::unknown();
        }

        auto type = analyze_expression(stmt->return_value, SemanticUseMode::MOVE);
        merge_type(function_returns.back(), type, "function return");
        return type;
    }

    SemanticType analyze_block_statement(BlockStatement *stmt) {
        begin_scope();
        auto result = analyze_block_contents(stmt);
        end_scope();
        return result;
    }

    SemanticType analyze_block_contents(BlockStatement *stmt) {
        SemanticType result = SemanticType::void_type();
        if (stmt != nullptr) {
            for (auto &block_stmt : stmt->statements) {
                result = analyze_statement(block_stmt);
                if (dynamic_cast<ReturnStatement *>(block_stmt) != nullptr) {
                    break;
                }
            }
        }
        return result;
    }

    SemanticType analyze_expression(Expression *exp, SemanticUseMode mode = SemanticUseMode::MOVE) {
        if (exp == nullptr) {
            return SemanticType::void_type();
        }

        if (dynamic_cast<IntegerLiteral *>(exp) != nullptr) {
            return SemanticType::int_type();
        }
        if (dynamic_cast<Boolean *>(exp) != nullptr) {
            return SemanticType::bool_type();
        }
        if (dynamic_cast<StringLiteral *>(exp) != nullptr) {
            return SemanticType::string_type();
        }
        if (auto *ident = dynamic_cast<Identifier *>(exp)) {
            return analyze_identifier(ident, mode);
        }
        if (auto *prefix_exp = dynamic_cast<PrefixExpression *>(exp)) {
            return analyze_prefix_expression(prefix_exp);
        }
        if (auto *borrow_exp = dynamic_cast<BorrowExpression *>(exp)) {
            return analyze_borrow_expression(borrow_exp);
        }
        if (auto *infix_exp = dynamic_cast<InfixExpression *>(exp)) {
            return analyze_infix_expression(infix_exp);
        }
        if (auto *if_exp = dynamic_cast<IfExpression *>(exp)) {
            return analyze_if_expression(if_exp);
        }
        if (auto *while_exp = dynamic_cast<WhileExpression *>(exp)) {
            return analyze_while_expression(while_exp);
        }
        if (auto *function = dynamic_cast<FunctionLiteral *>(exp)) {
            return analyze_function_literal(function);
        }
        if (auto *call = dynamic_cast<CallExpression *>(exp)) {
            return analyze_call_expression(call);
        }
        if (auto *array = dynamic_cast<ArrayLiteral *>(exp)) {
            return analyze_array_literal(array);
        }
        if (auto *index = dynamic_cast<IndexExpression *>(exp)) {
            return analyze_index_expression(index);
        }
        if (auto *assign = dynamic_cast<AssignExpression *>(exp)) {
            return analyze_assign_expression(assign);
        }
        if (auto *hash = dynamic_cast<HashLiteral *>(exp)) {
            return analyze_hash_literal(hash);
        }

        return SemanticType::unknown();
    }

    SemanticType analyze_identifier(Identifier *ident, SemanticUseMode mode) {
        if (ident == nullptr) {
            return SemanticType::unknown();
        }

        if (is_builtin_identifier(ident->value)) {
            return builtin_type(ident->value);
        }

        auto *symbol = resolve(ident->value);
        if (symbol == nullptr) {
            add_error("undefined identifier: " + ident->value);
            return SemanticType::unknown();
        }

        if (symbol->is_moved) {
            add_error("use of moved value: " + ident->value);
            return symbol->type;
        }

        if (mode == SemanticUseMode::READ && symbol->mutable_borrows > 0) {
            add_error("cannot read mutably borrowed value: " + ident->value);
            return symbol->type;
        }

        if (mode == SemanticUseMode::MOVE && has_active_borrows(*symbol)) {
            add_error("cannot move borrowed value: " + ident->value);
            return symbol->type;
        }

        if (mode == SemanticUseMode::MOVE && !is_copy_type(symbol->type)) {
            symbol->is_moved = true;
        }

        return symbol->type;
    }

    SemanticType analyze_prefix_expression(PrefixExpression *exp) {
        auto right = analyze_expression(exp->right, SemanticUseMode::MOVE);

        if (exp->op == "-") {
            require_type(right, SemanticType::int_type(), "operand of prefix '-'");
            return SemanticType::int_type();
        }
        if (exp->op == "!") {
            return SemanticType::bool_type();
        }

        add_error("unsupported prefix operator: " + exp->op);
        return SemanticType::unknown();
    }

    SemanticType analyze_borrow_expression(BorrowExpression *exp) {
        if (exp == nullptr) {
            return SemanticType::unknown();
        }

        SemanticSymbol *target_symbol{};
        auto *ident = dynamic_cast<Identifier *>(exp->value);
        if (ident != nullptr) {
            target_symbol = resolve(ident->value);
        }

        if (exp->is_mutable) {
            if (ident == nullptr) {
                add_error("mutable borrow target must be an identifier");
            } else if (target_symbol != nullptr) {
                if (!target_symbol->is_mutable) {
                    add_error("cannot mutably borrow immutable binding: " + ident->value);
                }
                if (target_symbol->is_moved) {
                    add_error("use of moved value: " + ident->value);
                }
                if (has_active_borrows(*target_symbol)) {
                    add_error("cannot mutably borrow already borrowed value: " + ident->value);
                }
            }
        } else if (target_symbol != nullptr) {
            if (target_symbol->is_moved) {
                add_error("use of moved value: " + ident->value);
            }
            if (target_symbol->mutable_borrows > 0) {
                add_error("cannot immutably borrow mutably borrowed value: " + ident->value);
            }
        }

        auto referenced = analyze_expression(exp->value, SemanticUseMode::READ);
        if (target_symbol != nullptr && !target_symbol->is_moved) {
            bool can_register = exp->is_mutable ? !has_active_borrows(*target_symbol) : target_symbol->mutable_borrows == 0;
            if (exp->is_mutable && !target_symbol->is_mutable) {
                can_register = false;
            }
            if (can_register) {
                register_borrow(target_symbol, exp->is_mutable);
            }
        }
        return SemanticType::reference_type(referenced, exp->is_mutable);
    }

    SemanticType analyze_infix_expression(InfixExpression *exp) {
        auto left = analyze_expression(exp->left, SemanticUseMode::MOVE);
        auto right = analyze_expression(exp->right, SemanticUseMode::MOVE);

        if (exp->op == "+" || exp->op == "-" || exp->op == "*" || exp->op == "/") {
            if (exp->op == "+" && left.kind == SemanticTypeKind::STRING && right.kind == SemanticTypeKind::STRING) {
                return SemanticType::string_type();
            }

            require_type(left, SemanticType::int_type(), "left operand of '" + exp->op + "'");
            require_type(right, SemanticType::int_type(), "right operand of '" + exp->op + "'");
            return SemanticType::int_type();
        }

        if (exp->op == "<" || exp->op == ">" || exp->op == "<=" || exp->op == ">=") {
            require_type(left, SemanticType::int_type(), "left operand of '" + exp->op + "'");
            require_type(right, SemanticType::int_type(), "right operand of '" + exp->op + "'");
            return SemanticType::bool_type();
        }

        if (exp->op == "==" || exp->op == "!=") {
            if (!compatible(left, right)) {
                add_error("type mismatch in operands of '" + exp->op + "': " + left.to_string() + " vs " + right.to_string());
            }
            return SemanticType::bool_type();
        }

        if (exp->op == "&&" || exp->op == "||") {
            require_type(left, SemanticType::bool_type(), "left operand of '" + exp->op + "'");
            require_type(right, SemanticType::bool_type(), "right operand of '" + exp->op + "'");
            return SemanticType::bool_type();
        }

        add_error("unsupported infix operator: " + exp->op);
        return SemanticType::unknown();
    }

    SemanticType analyze_if_expression(IfExpression *exp) {
        auto condition = analyze_expression(exp->condition, SemanticUseMode::READ);
        require_type(condition, SemanticType::bool_type(), "if condition");

        auto consequence = analyze_block_statement(exp->consequence);
        auto alternative = exp->alternative == nullptr ? SemanticType::null_type() : analyze_block_statement(exp->alternative);
        if (alternative.kind == SemanticTypeKind::NULL_TYPE) {
            return consequence;
        }
        if (!compatible(consequence, alternative)) {
            add_error("type mismatch in if branches: " + consequence.to_string() + " vs " + alternative.to_string());
        }
        return consequence.is_unknown() ? alternative : consequence;
    }

    SemanticType analyze_while_expression(WhileExpression *exp) {
        auto condition = analyze_expression(exp->condition, SemanticUseMode::READ);
        require_type(condition, SemanticType::bool_type(), "while condition");

        analyze_block_statement(exp->body);
        return SemanticType::void_type();
    }

    SemanticType analyze_function_literal(FunctionLiteral *function) {
        auto declared_return = type_from_annotation(function->return_type_annotation, "function return");
        function_returns.push_back(declared_return);
        begin_scope();

        auto params = parameter_types(function);
        for (size_t i = 0; i < function->parameters.size(); i++) {
            auto *param = function->parameters[i];
            if (param != nullptr) {
                define(param->value, params[i]);
            }
        }

        auto body_result = analyze_block_contents(function->body);
        auto return_type = function_returns.back();
        function_returns.pop_back();

        if (body_result.kind != SemanticTypeKind::VOID) {
            merge_type(return_type, body_result, "function return");
        }

        end_scope();
        return SemanticType::function_type(params, return_type);
    }

    SemanticType analyze_call_expression(CallExpression *call) {
        auto function = analyze_expression(call->function, SemanticUseMode::READ);

        vector<SemanticType> args;
        auto borrow_mark = current_borrow_count();
        for (size_t i = 0; i < call->arguments.size(); i++) {
            auto mode = SemanticUseMode::MOVE;
            if (function.kind == SemanticTypeKind::FUNCTION && i < function.params.size() && is_reference_type(function.params[i])) {
                mode = SemanticUseMode::READ;
            }
            args.push_back(analyze_expression(call->arguments[i], mode));
        }
        release_current_borrows_from(borrow_mark);

        if (function.is_unknown()) {
            return SemanticType::unknown();
        }

        if (function.kind != SemanticTypeKind::FUNCTION) {
            add_error("not a function: " + function.to_string());
            return SemanticType::unknown();
        }

        if (args.size() != function.params.size()) {
            add_error("wrong number of arguments: expected " + std::to_string(function.params.size()) +
                      ", got " + std::to_string(args.size()));
        }

        if (function.params.size() == args.size()) {
            for (size_t i = 0; i < args.size(); i++) {
                if (!compatible(function.params[i], args[i])) {
                    add_error("type mismatch in argument " + std::to_string(i + 1) + ": " +
                              function.params[i].to_string() + " vs " + args[i].to_string());
                }
            }
        }

        if (function.return_type == nullptr) {
            return SemanticType::unknown();
        }
        return *function.return_type;
    }

    SemanticType analyze_array_literal(ArrayLiteral *array) {
        SemanticType element_type = SemanticType::unknown();
        for (auto &element : array->elements) {
            auto type = analyze_expression(element, SemanticUseMode::MOVE);
            merge_type(element_type, type, "array element");
        }
        return SemanticType::array_type(element_type);
    }

    SemanticType analyze_index_expression(IndexExpression *index) {
        auto left = analyze_expression(index->left, SemanticUseMode::READ);
        auto index_type = analyze_expression(index->index, SemanticUseMode::MOVE);
        auto indexed = left;
        if (indexed.kind == SemanticTypeKind::REFERENCE && indexed.referenced_type != nullptr) {
            indexed = *indexed.referenced_type;
        }

        if (indexed.kind == SemanticTypeKind::ARRAY) {
            require_type(index_type, SemanticType::int_type(), "array index");
            if (indexed.element_type != nullptr) {
                return *indexed.element_type;
            }
            return SemanticType::unknown();
        }

        if (indexed.kind == SemanticTypeKind::HASH) {
            return SemanticType::unknown();
        }

        if (!indexed.is_unknown()) {
            add_error("index operator not supported: " + left.to_string());
        }
        return SemanticType::unknown();
    }

    SemanticType analyze_assign_expression(AssignExpression *assign) {
        auto value = analyze_expression(assign->value, SemanticUseMode::MOVE);

        if (auto *ident = dynamic_cast<Identifier *>(assign->left)) {
            auto *symbol = resolve(ident->value);
            if (symbol == nullptr) {
                add_error("undefined identifier: " + ident->value);
                return SemanticType::unknown();
            }

            if (!symbol->is_mutable) {
                add_error("cannot assign to immutable binding: " + ident->value);
            }

            if (has_active_borrows(*symbol)) {
                add_error("cannot assign to borrowed value: " + ident->value);
            }

            merge_type(symbol->type, value, "assignment to '" + ident->value + "'");
            symbol->is_moved = false;
            return symbol->type;
        }

        if (auto *index = dynamic_cast<IndexExpression *>(assign->left)) {
            if (auto *root = root_identifier(index)) {
                auto *symbol = resolve(root->value);
                if (symbol != nullptr && has_active_borrows(*symbol)) {
                    add_error("cannot assign to borrowed value: " + root->value);
                }
            }
            auto target = analyze_index_expression(index);
            merge_type(target, value, "index assignment");
            return value;
        }

        add_error("invalid assignment target");
        return SemanticType::unknown();
    }

    SemanticType analyze_hash_literal(HashLiteral *hash) {
        for (auto &pair : hash->pairs) {
            auto key = analyze_expression(pair.first, SemanticUseMode::MOVE);
            if (key.kind != SemanticTypeKind::INT &&
                key.kind != SemanticTypeKind::BOOL &&
                key.kind != SemanticTypeKind::STRING &&
                !key.is_unknown()) {
                add_error("unusable as hash key: " + key.to_string());
            }
            analyze_expression(pair.second, SemanticUseMode::MOVE);
        }
        return SemanticType::hash_type();
    }
};

#endif //FIREFLY_COMPILER_SEMANTIC_HPP
