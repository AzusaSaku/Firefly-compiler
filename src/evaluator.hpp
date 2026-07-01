/* 定义AST解释器 */

#ifndef FIREFLY_COMPILER_EVALUATOR_HPP
#define FIREFLY_COMPILER_EVALUATOR_HPP

#include "environment.hpp"
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace std;

inline ObjectPtr eval(Node *node, const shared_ptr<Environment> &env);

inline ObjectPtr native_bool_to_boolean_object(bool value) {
    static ObjectPtr true_object = make_shared<BooleanObject>(true);
    static ObjectPtr false_object = make_shared<BooleanObject>(false);
    return value ? true_object : false_object;
}

inline ObjectPtr null_object() {
    static ObjectPtr object = make_shared<NullObject>();
    return object;
}

inline ObjectPtr new_error(const string &message) {
    return make_shared<ErrorObject>(message);
}

inline bool is_error(const ObjectPtr &object) {
    return object != nullptr && object->type() == ObjectType::ERROR;
}

inline bool is_truthy(const ObjectPtr &object) {
    if (object == nullptr || object->type() == ObjectType::NULL_OBJ) {
        return false;
    }

    if (object->type() == ObjectType::BOOLEAN) {
        return dynamic_pointer_cast<BooleanObject>(object)->value;
    }

    return true;
}

inline ObjectPtr builtin_len(vector<ObjectPtr> args) {
    if (args.size() != 1) {
        return new_error("wrong number of arguments: expected 1, got " + to_string(args.size()));
    }

    if (args[0]->type() == ObjectType::STRING) {
        return make_shared<IntegerObject>(static_cast<int>(dynamic_pointer_cast<StringObject>(args[0])->value.length()));
    }

    if (args[0]->type() == ObjectType::ARRAY) {
        return make_shared<IntegerObject>(static_cast<int>(dynamic_pointer_cast<ArrayObject>(args[0])->elements.size()));
    }

    return new_error("argument to len not supported: " + object_type_to_string(args[0]->type()));
}

inline ObjectPtr builtin_first(vector<ObjectPtr> args) {
    if (args.size() != 1) {
        return new_error("wrong number of arguments: expected 1, got " + to_string(args.size()));
    }

    if (args[0]->type() != ObjectType::ARRAY) {
        return new_error("argument to first must be ARRAY, got " + object_type_to_string(args[0]->type()));
    }

    auto array = dynamic_pointer_cast<ArrayObject>(args[0]);
    if (array->elements.empty()) {
        return null_object();
    }

    return array->elements.front();
}

inline ObjectPtr builtin_last(vector<ObjectPtr> args) {
    if (args.size() != 1) {
        return new_error("wrong number of arguments: expected 1, got " + to_string(args.size()));
    }

    if (args[0]->type() != ObjectType::ARRAY) {
        return new_error("argument to last must be ARRAY, got " + object_type_to_string(args[0]->type()));
    }

    auto array = dynamic_pointer_cast<ArrayObject>(args[0]);
    if (array->elements.empty()) {
        return null_object();
    }

    return array->elements.back();
}

inline ObjectPtr builtin_rest(vector<ObjectPtr> args) {
    if (args.size() != 1) {
        return new_error("wrong number of arguments: expected 1, got " + to_string(args.size()));
    }

    if (args[0]->type() != ObjectType::ARRAY) {
        return new_error("argument to rest must be ARRAY, got " + object_type_to_string(args[0]->type()));
    }

    auto array = dynamic_pointer_cast<ArrayObject>(args[0]);
    if (array->elements.empty()) {
        return null_object();
    }

    vector<ObjectPtr> elements(array->elements.begin() + 1, array->elements.end());
    return make_shared<ArrayObject>(elements);
}

inline ObjectPtr builtin_push(vector<ObjectPtr> args) {
    if (args.size() != 2) {
        return new_error("wrong number of arguments: expected 2, got " + to_string(args.size()));
    }

    if (args[0]->type() != ObjectType::ARRAY) {
        return new_error("argument to push must be ARRAY, got " + object_type_to_string(args[0]->type()));
    }

    auto array = dynamic_pointer_cast<ArrayObject>(args[0]);
    vector<ObjectPtr> elements = array->elements;
    elements.push_back(args[1]);
    return make_shared<ArrayObject>(elements);
}

inline ObjectPtr builtin_puts(vector<ObjectPtr> args) {
    for (auto &arg : args) {
        cout << arg->inspect() << endl;
    }
    return null_object();
}

inline const map<string, ObjectPtr> &builtins() {
    static map<string, ObjectPtr> builtin_map = {
            {"len",   make_shared<BuiltinObject>(builtin_len)},
            {"first", make_shared<BuiltinObject>(builtin_first)},
            {"last",  make_shared<BuiltinObject>(builtin_last)},
            {"rest",  make_shared<BuiltinObject>(builtin_rest)},
            {"push",  make_shared<BuiltinObject>(builtin_push)},
            {"puts",  make_shared<BuiltinObject>(builtin_puts)}
    };
    return builtin_map;
}

inline ObjectPtr eval_program(Program *program, const shared_ptr<Environment> &env) {
    ObjectPtr result = null_object();

    for (auto *stmt : program->statements) {
        result = eval(stmt, env);

        if (result != nullptr && result->type() == ObjectType::RETURN_VALUE) {
            return dynamic_pointer_cast<ReturnValueObject>(result)->value;
        }

        if (is_error(result)) {
            return result;
        }
    }

    return result;
}

inline ObjectPtr eval_block_statement(BlockStatement *block, const shared_ptr<Environment> &env) {
    ObjectPtr result = null_object();

    for (auto *stmt : block->statements) {
        result = eval(stmt, env);

        if (result != nullptr &&
            (result->type() == ObjectType::RETURN_VALUE || result->type() == ObjectType::ERROR)) {
            return result;
        }
    }

    return result;
}

inline ObjectPtr eval_bang_operator_expression(const ObjectPtr &right) {
    if (right == native_bool_to_boolean_object(true)) {
        return native_bool_to_boolean_object(false);
    }

    if (right == native_bool_to_boolean_object(false) || right == null_object()) {
        return native_bool_to_boolean_object(true);
    }

    return native_bool_to_boolean_object(false);
}

inline ObjectPtr eval_minus_prefix_operator_expression(const ObjectPtr &right) {
    if (right == nullptr || right->type() != ObjectType::INTEGER) {
        return new_error("unknown operator: -" + object_type_to_string(right == nullptr ? ObjectType::NULL_OBJ : right->type()));
    }

    auto value = dynamic_pointer_cast<IntegerObject>(right)->value;
    return make_shared<IntegerObject>(-value);
}

inline ObjectPtr eval_prefix_expression(const string &op, const ObjectPtr &right) {
    if (op == "!") {
        return eval_bang_operator_expression(right);
    }

    if (op == "-") {
        return eval_minus_prefix_operator_expression(right);
    }

    return new_error("unknown operator: " + op + object_type_to_string(right->type()));
}

inline ObjectPtr eval_integer_infix_expression(const string &op, const ObjectPtr &left, const ObjectPtr &right) {
    int left_value = dynamic_pointer_cast<IntegerObject>(left)->value;
    int right_value = dynamic_pointer_cast<IntegerObject>(right)->value;

    if (op == "+") {
        return make_shared<IntegerObject>(left_value + right_value);
    }
    if (op == "-") {
        return make_shared<IntegerObject>(left_value - right_value);
    }
    if (op == "*") {
        return make_shared<IntegerObject>(left_value * right_value);
    }
    if (op == "/") {
        if (right_value == 0) {
            return new_error("division by zero");
        }
        return make_shared<IntegerObject>(left_value / right_value);
    }
    if (op == "<") {
        return native_bool_to_boolean_object(left_value < right_value);
    }
    if (op == ">") {
        return native_bool_to_boolean_object(left_value > right_value);
    }
    if (op == "<=") {
        return native_bool_to_boolean_object(left_value <= right_value);
    }
    if (op == ">=") {
        return native_bool_to_boolean_object(left_value >= right_value);
    }
    if (op == "==") {
        return native_bool_to_boolean_object(left_value == right_value);
    }
    if (op == "!=") {
        return native_bool_to_boolean_object(left_value != right_value);
    }

    return new_error("unknown operator: INTEGER " + op + " INTEGER");
}

inline ObjectPtr eval_string_infix_expression(const string &op, const ObjectPtr &left, const ObjectPtr &right) {
    auto left_value = dynamic_pointer_cast<StringObject>(left)->value;
    auto right_value = dynamic_pointer_cast<StringObject>(right)->value;

    if (op == "+") {
        return make_shared<StringObject>(left_value + right_value);
    }
    if (op == "==") {
        return native_bool_to_boolean_object(left_value == right_value);
    }
    if (op == "!=") {
        return native_bool_to_boolean_object(left_value != right_value);
    }

    return new_error("unknown operator: STRING " + op + " STRING");
}

inline ObjectPtr eval_infix_expression(const string &op, const ObjectPtr &left, const ObjectPtr &right) {
    if (left->type() == ObjectType::INTEGER && right->type() == ObjectType::INTEGER) {
        return eval_integer_infix_expression(op, left, right);
    }

    if (left->type() == ObjectType::STRING && right->type() == ObjectType::STRING) {
        return eval_string_infix_expression(op, left, right);
    }

    if (op == "==") {
        return native_bool_to_boolean_object(left == right);
    }
    if (op == "!=") {
        return native_bool_to_boolean_object(left != right);
    }

    if (left->type() != right->type()) {
        return new_error("type mismatch: " + object_type_to_string(left->type()) + " " + op + " " +
                         object_type_to_string(right->type()));
    }

    return new_error("unknown operator: " + object_type_to_string(left->type()) + " " + op + " " +
                     object_type_to_string(right->type()));
}

inline ObjectPtr eval_if_expression(IfExpression *expression, const shared_ptr<Environment> &env) {
    auto condition = eval(expression->condition, env);
    if (is_error(condition)) {
        return condition;
    }

    if (is_truthy(condition)) {
        return eval(expression->consequence, new_enclosed_environment(env));
    }

    if (expression->alternative != nullptr) {
        return eval(expression->alternative, new_enclosed_environment(env));
    }

    return null_object();
}

inline ObjectPtr eval_while_expression(WhileExpression *expression, const shared_ptr<Environment> &env) {
    ObjectPtr result = null_object();

    while (true) {
        auto condition = eval(expression->condition, env);
        if (is_error(condition)) {
            return condition;
        }

        if (!is_truthy(condition)) {
            break;
        }

        result = eval(expression->body, new_enclosed_environment(env));
        if (result != nullptr &&
            (result->type() == ObjectType::RETURN_VALUE || result->type() == ObjectType::ERROR)) {
            return result;
        }
    }

    return result;
}

inline ObjectPtr eval_logical_expression(InfixExpression *expression, const shared_ptr<Environment> &env) {
    auto left = eval(expression->left, env);
    if (is_error(left)) {
        return left;
    }

    if (expression->op == "&&") {
        if (!is_truthy(left)) {
            return native_bool_to_boolean_object(false);
        }

        auto right = eval(expression->right, env);
        if (is_error(right)) {
            return right;
        }
        return native_bool_to_boolean_object(is_truthy(right));
    }

    if (is_truthy(left)) {
        return native_bool_to_boolean_object(true);
    }

    auto right = eval(expression->right, env);
    if (is_error(right)) {
        return right;
    }
    return native_bool_to_boolean_object(is_truthy(right));
}

inline ObjectPtr eval_identifier(Identifier *ident, const shared_ptr<Environment> &env) {
    auto value = env->get(ident->value);
    if (value != nullptr) {
        return value;
    }

    auto builtin = builtins().find(ident->value);
    if (builtin != builtins().end()) {
        return builtin->second;
    }

    return new_error("identifier not found: " + ident->value);
}

inline vector<ObjectPtr> eval_expressions(const vector<Expression *> &expressions, const shared_ptr<Environment> &env) {
    vector<ObjectPtr> result;

    for (auto *expression : expressions) {
        auto evaluated = eval(expression, env);
        if (is_error(evaluated)) {
            return {evaluated};
        }
        result.push_back(evaluated);
    }

    return result;
}

inline shared_ptr<Environment> extend_function_env(const shared_ptr<FunctionObject> &function,
                                                   const vector<ObjectPtr> &args) {
    auto env = new_enclosed_environment(function->env);

    for (size_t i = 0; i < function->parameters.size(); i++) {
        env->set(function->parameters[i]->value, args[i]);
    }

    return env;
}

inline ObjectPtr unwrap_return_value(const ObjectPtr &object) {
    if (object != nullptr && object->type() == ObjectType::RETURN_VALUE) {
        return dynamic_pointer_cast<ReturnValueObject>(object)->value;
    }

    return object;
}

inline ObjectPtr apply_function(const ObjectPtr &fn, const vector<ObjectPtr> &args) {
    if (fn != nullptr && fn->type() == ObjectType::BUILTIN) {
        return dynamic_pointer_cast<BuiltinObject>(fn)->fn(args);
    }

    if (fn == nullptr || fn->type() != ObjectType::FUNCTION) {
        return new_error("not a function: " + object_type_to_string(fn == nullptr ? ObjectType::NULL_OBJ : fn->type()));
    }

    auto function = dynamic_pointer_cast<FunctionObject>(fn);
    if (args.size() != function->parameters.size()) {
        return new_error("wrong number of arguments: expected " + to_string(function->parameters.size()) +
                         ", got " + to_string(args.size()));
    }

    auto extended_env = extend_function_env(function, args);
    auto evaluated = eval(function->body, extended_env);
    return unwrap_return_value(evaluated);
}

inline bool hash_key(const ObjectPtr &object, HashKey &key) {
    if (object->type() == ObjectType::INTEGER) {
        key = HashKey{object->type(), to_string(dynamic_pointer_cast<IntegerObject>(object)->value)};
        return true;
    }

    if (object->type() == ObjectType::BOOLEAN) {
        key = HashKey{object->type(), dynamic_pointer_cast<BooleanObject>(object)->value ? "true" : "false"};
        return true;
    }

    if (object->type() == ObjectType::STRING) {
        key = HashKey{object->type(), dynamic_pointer_cast<StringObject>(object)->value};
        return true;
    }

    return false;
}

inline ObjectPtr eval_hash_literal(HashLiteral *node, const shared_ptr<Environment> &env) {
    map<HashKey, HashPair> pairs;

    for (auto &node_pair : node->pairs) {
        auto key = eval(node_pair.first, env);
        if (is_error(key)) {
            return key;
        }

        HashKey hashed;
        if (!hash_key(key, hashed)) {
            return new_error("unusable as hash key: " + object_type_to_string(key->type()));
        }

        auto value = eval(node_pair.second, env);
        if (is_error(value)) {
            return value;
        }

        pairs[hashed] = HashPair{key, value};
    }

    return make_shared<HashObject>(pairs);
}

inline ObjectPtr eval_array_index_expression(const ObjectPtr &array, const ObjectPtr &index) {
    auto array_object = dynamic_pointer_cast<ArrayObject>(array);
    int idx = dynamic_pointer_cast<IntegerObject>(index)->value;
    int max = static_cast<int>(array_object->elements.size()) - 1;

    if (idx < 0 || idx > max) {
        return null_object();
    }

    return array_object->elements[idx];
}

inline ObjectPtr eval_hash_index_expression(const ObjectPtr &hash, const ObjectPtr &index) {
    HashKey key;
    if (!hash_key(index, key)) {
        return new_error("unusable as hash key: " + object_type_to_string(index->type()));
    }

    auto hash_object = dynamic_pointer_cast<HashObject>(hash);
    auto pair = hash_object->pairs.find(key);
    if (pair == hash_object->pairs.end()) {
        return null_object();
    }

    return pair->second.value;
}

inline ObjectPtr eval_index_expression(const ObjectPtr &left, const ObjectPtr &index) {
    if (left->type() == ObjectType::ARRAY && index->type() == ObjectType::INTEGER) {
        return eval_array_index_expression(left, index);
    }

    if (left->type() == ObjectType::HASH) {
        return eval_hash_index_expression(left, index);
    }

    return new_error("index operator not supported: " + object_type_to_string(left->type()));
}

inline ObjectPtr assign_array_index(const ObjectPtr &array, const ObjectPtr &index, const ObjectPtr &value) {
    if (index->type() != ObjectType::INTEGER) {
        return new_error("array index must be INTEGER, got " + object_type_to_string(index->type()));
    }

    auto array_object = dynamic_pointer_cast<ArrayObject>(array);
    int idx = dynamic_pointer_cast<IntegerObject>(index)->value;
    int max = static_cast<int>(array_object->elements.size()) - 1;

    if (idx < 0 || idx > max) {
        return new_error("array index out of bounds: " + to_string(idx));
    }

    array_object->elements[idx] = value;
    return value;
}

inline ObjectPtr assign_hash_index(const ObjectPtr &hash, const ObjectPtr &index, const ObjectPtr &value) {
    HashKey key;
    if (!hash_key(index, key)) {
        return new_error("unusable as hash key: " + object_type_to_string(index->type()));
    }

    auto hash_object = dynamic_pointer_cast<HashObject>(hash);
    hash_object->pairs[key] = HashPair{index, value};
    return value;
}

inline ObjectPtr eval_assign_expression(AssignExpression *expression, const shared_ptr<Environment> &env) {
    auto value = eval(expression->value, env);
    if (is_error(value)) {
        return value;
    }

    if (auto *ident = dynamic_cast<Identifier *>(expression->left)) {
        if (!env->assign(ident->value, value)) {
            return new_error("identifier not found: " + ident->value);
        }
        return value;
    }

    if (auto *index_expression = dynamic_cast<IndexExpression *>(expression->left)) {
        auto left = eval(index_expression->left, env);
        if (is_error(left)) {
            return left;
        }

        auto index = eval(index_expression->index, env);
        if (is_error(index)) {
            return index;
        }

        if (left->type() == ObjectType::ARRAY) {
            return assign_array_index(left, index, value);
        }

        if (left->type() == ObjectType::HASH) {
            return assign_hash_index(left, index, value);
        }

        return new_error("index assignment not supported: " + object_type_to_string(left->type()));
    }

    return new_error("invalid assignment target");
}

inline ObjectPtr eval(Node *node, const shared_ptr<Environment> &env) {
    if (node == nullptr) {
        return null_object();
    }

    if (auto *program = dynamic_cast<Program *>(node)) {
        return eval_program(program, env);
    }

    if (auto *block = dynamic_cast<BlockStatement *>(node)) {
        return eval_block_statement(block, env);
    }

    if (auto *stmt = dynamic_cast<ExpressionStatement *>(node)) {
        return eval(stmt->expression, env);
    }

    if (auto *stmt = dynamic_cast<VarStatement *>(node)) {
        auto value = eval(stmt->value, env);
        if (is_error(value)) {
            return value;
        }
        env->set(stmt->name->value, value);
        return value;
    }

    if (auto *stmt = dynamic_cast<ReturnStatement *>(node)) {
        auto value = eval(stmt->return_value, env);
        if (is_error(value)) {
            return value;
        }
        return make_shared<ReturnValueObject>(value);
    }

    if (auto *expression = dynamic_cast<IntegerLiteral *>(node)) {
        return make_shared<IntegerObject>(expression->value);
    }

    if (auto *expression = dynamic_cast<StringLiteral *>(node)) {
        return make_shared<StringObject>(expression->value);
    }

    if (auto *expression = dynamic_cast<Boolean *>(node)) {
        return native_bool_to_boolean_object(expression->value);
    }

    if (auto *expression = dynamic_cast<Identifier *>(node)) {
        return eval_identifier(expression, env);
    }

    if (auto *expression = dynamic_cast<PrefixExpression *>(node)) {
        auto right = eval(expression->right, env);
        if (is_error(right)) {
            return right;
        }
        return eval_prefix_expression(expression->op, right);
    }

    if (auto *expression = dynamic_cast<InfixExpression *>(node)) {
        if (expression->op == "&&" || expression->op == "||") {
            return eval_logical_expression(expression, env);
        }

        auto left = eval(expression->left, env);
        if (is_error(left)) {
            return left;
        }

        auto right = eval(expression->right, env);
        if (is_error(right)) {
            return right;
        }

        return eval_infix_expression(expression->op, left, right);
    }

    if (auto *expression = dynamic_cast<IfExpression *>(node)) {
        return eval_if_expression(expression, env);
    }

    if (auto *expression = dynamic_cast<WhileExpression *>(node)) {
        return eval_while_expression(expression, env);
    }

    if (auto *expression = dynamic_cast<AssignExpression *>(node)) {
        return eval_assign_expression(expression, env);
    }

    if (auto *expression = dynamic_cast<FunctionLiteral *>(node)) {
        return make_shared<FunctionObject>(expression->parameters, expression->body, env);
    }

    if (auto *expression = dynamic_cast<CallExpression *>(node)) {
        auto function = eval(expression->function, env);
        if (is_error(function)) {
            return function;
        }

        auto args = eval_expressions(expression->arguments, env);
        if (args.size() == 1 && is_error(args[0])) {
            return args[0];
        }

        return apply_function(function, args);
    }

    if (auto *expression = dynamic_cast<ArrayLiteral *>(node)) {
        auto elements = eval_expressions(expression->elements, env);
        if (elements.size() == 1 && is_error(elements[0])) {
            return elements[0];
        }
        return make_shared<ArrayObject>(elements);
    }

    if (auto *expression = dynamic_cast<IndexExpression *>(node)) {
        auto left = eval(expression->left, env);
        if (is_error(left)) {
            return left;
        }

        auto index = eval(expression->index, env);
        if (is_error(index)) {
            return index;
        }

        return eval_index_expression(left, index);
    }

    if (auto *expression = dynamic_cast<HashLiteral *>(node)) {
        return eval_hash_literal(expression, env);
    }

    return null_object();
}

#endif //FIREFLY_COMPILER_EVALUATOR_HPP
