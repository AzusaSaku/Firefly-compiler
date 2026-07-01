/* 语义分析器 */

#ifndef FIREFLY_COMPILER_SEMANTIC_HPP
#define FIREFLY_COMPILER_SEMANTIC_HPP

#include "ast.hpp"
#include <map>
#include <string>
#include <vector>

using namespace std;

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
    vector<map<string, bool>> scopes;
    int function_depth{};

    void begin_scope() {
        scopes.emplace_back();
    }

    void end_scope() {
        if (!scopes.empty()) {
            scopes.pop_back();
        }
    }

    bool define(const string &name) {
        if (scopes.empty()) {
            begin_scope();
        }

        auto &scope = scopes.back();
        if (scope.find(name) != scope.end()) {
            add_error("identifier already defined: " + name);
            return false;
        }

        scope[name] = true;
        return true;
    }

    bool resolve(const string &name) const {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            if (it->find(name) != it->end()) {
                return true;
            }
        }
        return false;
    }

    void add_error(const string &msg) {
        errors.push_back(msg);
    }

    void analyze_statement(Statement *stmt) {
        if (stmt == nullptr) {
            return;
        }

        if (auto *var_stmt = dynamic_cast<VarStatement *>(stmt)) {
            analyze_var_statement(var_stmt);
        } else if (auto *return_stmt = dynamic_cast<ReturnStatement *>(stmt)) {
            analyze_return_statement(return_stmt);
        } else if (auto *expression_stmt = dynamic_cast<ExpressionStatement *>(stmt)) {
            analyze_expression(expression_stmt->expression);
        } else if (auto *block_stmt = dynamic_cast<BlockStatement *>(stmt)) {
            analyze_block_statement(block_stmt);
        }
    }

    void analyze_var_statement(VarStatement *stmt) {
        if (stmt->value != nullptr) {
            analyze_expression(stmt->value);
        }

        if (stmt->name != nullptr) {
            define(stmt->name->value);
        }
    }

    void analyze_return_statement(ReturnStatement *stmt) {
        if (function_depth == 0) {
            add_error("return statement outside function");
        }

        analyze_expression(stmt->return_value);
    }

    void analyze_block_statement(BlockStatement *stmt) {
        begin_scope();
        for (auto &block_stmt : stmt->statements) {
            analyze_statement(block_stmt);
        }
        end_scope();
    }

    void analyze_expression(Expression *exp) {
        if (exp == nullptr) {
            return;
        }

        if (auto *ident = dynamic_cast<Identifier *>(exp)) {
            analyze_identifier(ident);
        } else if (dynamic_cast<StringLiteral *>(exp) != nullptr) {
            return;
        } else if (auto *prefix_exp = dynamic_cast<PrefixExpression *>(exp)) {
            analyze_expression(prefix_exp->right);
        } else if (auto *infix_exp = dynamic_cast<InfixExpression *>(exp)) {
            analyze_expression(infix_exp->left);
            analyze_expression(infix_exp->right);
        } else if (auto *if_exp = dynamic_cast<IfExpression *>(exp)) {
            analyze_if_expression(if_exp);
        } else if (auto *while_exp = dynamic_cast<WhileExpression *>(exp)) {
            analyze_while_expression(while_exp);
        } else if (auto *function = dynamic_cast<FunctionLiteral *>(exp)) {
            analyze_function_literal(function);
        } else if (auto *call = dynamic_cast<CallExpression *>(exp)) {
            analyze_call_expression(call);
        } else if (auto *array = dynamic_cast<ArrayLiteral *>(exp)) {
            analyze_array_literal(array);
        } else if (auto *index = dynamic_cast<IndexExpression *>(exp)) {
            analyze_index_expression(index);
        } else if (auto *assign = dynamic_cast<AssignExpression *>(exp)) {
            analyze_assign_expression(assign);
        } else if (auto *hash = dynamic_cast<HashLiteral *>(exp)) {
            analyze_hash_literal(hash);
        }
    }

    bool is_builtin_identifier(const string &name) const {
        return name == "len" || name == "first" || name == "last" ||
               name == "rest" || name == "push" || name == "puts";
    }

    void analyze_identifier(Identifier *ident) {
        if (is_builtin_identifier(ident->value)) {
            return;
        }

        if (!resolve(ident->value)) {
            add_error("undefined identifier: " + ident->value);
        }
    }

    void analyze_if_expression(IfExpression *exp) {
        analyze_expression(exp->condition);

        if (exp->consequence != nullptr) {
            analyze_block_statement(exp->consequence);
        }

        if (exp->alternative != nullptr) {
            analyze_block_statement(exp->alternative);
        }
    }

    void analyze_while_expression(WhileExpression *exp) {
        analyze_expression(exp->condition);

        if (exp->body != nullptr) {
            analyze_block_statement(exp->body);
        }
    }

    void analyze_function_literal(FunctionLiteral *function) {
        function_depth++;
        begin_scope();

        for (auto &param : function->parameters) {
            if (param != nullptr) {
                define(param->value);
            }
        }

        if (function->body != nullptr) {
            analyze_block_statement(function->body);
        }

        end_scope();
        function_depth--;
    }

    void analyze_call_expression(CallExpression *call) {
        analyze_expression(call->function);

        for (auto &arg : call->arguments) {
            analyze_expression(arg);
        }
    }

    void analyze_array_literal(ArrayLiteral *array) {
        for (auto &element : array->elements) {
            analyze_expression(element);
        }
    }

    void analyze_index_expression(IndexExpression *index) {
        analyze_expression(index->left);
        analyze_expression(index->index);
    }

    void analyze_assign_expression(AssignExpression *assign) {
        if (auto *ident = dynamic_cast<Identifier *>(assign->left)) {
            analyze_identifier(ident);
        } else if (auto *index = dynamic_cast<IndexExpression *>(assign->left)) {
            analyze_index_expression(index);
        } else {
            add_error("invalid assignment target");
        }

        analyze_expression(assign->value);
    }

    void analyze_hash_literal(HashLiteral *hash) {
        for (auto &pair : hash->pairs) {
            analyze_expression(pair.first);
            analyze_expression(pair.second);
        }
    }
};

#endif //FIREFLY_COMPILER_SEMANTIC_HPP
