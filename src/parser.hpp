/* 语法分析器 */

#ifndef FIREFLY_INTERPRETER_PARSER_HPP
#define FIREFLY_INTERPRETER_PARSER_HPP

#include <memory>
#include <functional>
#include <utility>
#include <vector>
#include <string>
#include <map>
#include <stdexcept>
#include "lexer.hpp"
#include "ast.hpp"

enum Precedence {// 弱枚举类型
    LOWEST,
    EQUALS,      // ==
    LESSGREATER, // > or <
    SUM,         // +
    PRODUCT,     // *
    PREFIX,      // -X or !X
    CALL,        // myFunction(X)
};

const map<TokenType, Precedence> precedences = {
        {TokenType::EQ, Precedence::EQUALS},         // 等于
        {TokenType::NOT_EQ, Precedence::EQUALS},     // 不等于
        {TokenType::LT, Precedence::LESSGREATER},    // 小于
        {TokenType::GT, Precedence::LESSGREATER},    // 大于
        {TokenType::PLUS, Precedence::SUM},          // 加
        {TokenType::MINUS, Precedence::SUM},         // 减
        {TokenType::SLASH, Precedence::PRODUCT},     // 除
        {TokenType::ASTERISK, Precedence::PRODUCT},  // 乘
        {TokenType::BANG, Precedence::PREFIX},       // 前缀
        {TokenType::LPAREN, Precedence::CALL},       // 函数调用
};

Expression* prefix_parse_fn() {
    return {};
};

Expression* infix_parse_fn(Expression&) {
    return {};
};

class Parser {
public:
    Lexer *l;
    Token cur_token;
    Token peek_token;
    vector<string> errors;

    map<TokenType, function<Expression*()>> prefix_parse_fns;
    map<TokenType, function<Expression*(Expression&)>> infix_parse_fns;

    void register_prefix(TokenType type, function<Expression*()> fn) {
        prefix_parse_fns[type] = std::move(fn);
    }

    void register_infix(TokenType type, function<Expression*(Expression&)> fn) {
        infix_parse_fns[type] = std::move(fn);
    }

    // 解析器构造函数
    explicit Parser(Lexer *lexer) : l(lexer) {
        // 注册前缀解析函数
        this->register_prefix(TokenType::IDENT, [this]() { return this->parse_identifier(); });
        this->register_prefix(TokenType::INT, [this]() { return this->parse_integer_literal(); });
        this->register_prefix(TokenType::BANG, [this]() { return this->parse_prefix_expression(); });
        this->register_prefix(TokenType::MINUS, [this]() { return this->parse_prefix_expression(); });
        this->register_prefix(TokenType::TRUE, [this]() { return this->parse_boolean(); });
        this->register_prefix(TokenType::FALSE, [this]() { return this->parse_boolean(); });
        this->register_prefix(TokenType::LPAREN, [this]() { return this->parse_grouped_expression(); });
        this->register_prefix(TokenType::IF, [this]() { return this->parse_if_expression(); });
        this->register_prefix(TokenType::FUNC, [this]() { return this->parse_function_literal(); });
        
        // 注册中缀解析函数
        this->register_infix(TokenType::PLUS, [this](Expression &left) { return this->parse_infix_expression(left); });
        this->register_infix(TokenType::MINUS, [this](Expression &left) { return this->parse_infix_expression(left); });
        this->register_infix(TokenType::SLASH, [this](Expression &left) { return this->parse_infix_expression(left); });
        this->register_infix(TokenType::ASTERISK, [this](Expression &left) { return this->parse_infix_expression(left); });
        this->register_infix(TokenType::EQ, [this](Expression &left) { return this->parse_infix_expression(left); });
        this->register_infix(TokenType::NOT_EQ, [this](Expression &left) { return this->parse_infix_expression(left); });
        this->register_infix(TokenType::LT, [this](Expression &left) { return this->parse_infix_expression(left); });
        this->register_infix(TokenType::GT, [this](Expression &left) { return this->parse_infix_expression(left); });
        this->register_infix(TokenType::LPAREN, [this](Expression &left) { return this->parse_call_expression(&left); });

        // 读取两个token
        parser_next_token();
        parser_next_token();
    }

    void parser_next_token() {
        cur_token = peek_token;
        peek_token = next_token(l);
    }

    Program *parse_program() {
        auto *program = new Program();
        program->statements = vector<Statement *>();

        while (!cur_token_is(TokenType::END)) {
            auto *stmt = parse_statement();
            if (stmt != nullptr) {
                program->statements.push_back(stmt);
            }
            parser_next_token();
        }
        return program;
    }

    Statement *parse_statement() {
        switch (cur_token.type) {
            case TokenType::VAR:
                return parse_var_statement();
            case TokenType::RETURN:
                return parse_return_statement();
            default:
                return parse_expression_statement();
        }
    }

    Expression *parse_expression(Precedence precedence) {
        auto prefix = prefix_parse_fns[cur_token.type];
        if (!prefix) {
            no_prefix_parse_fn_error(cur_token.type);
            return nullptr;
        }

        auto left_exp = prefix();
        if (left_exp == nullptr) {
            return nullptr;
        }

        while (!peek_token_is(TokenType::SEMICOLON) && precedence < peek_precedence()) {
            auto infix = infix_parse_fns[peek_token.type];
            if (!infix) {
                return left_exp;
            }

            parser_next_token();
            left_exp = infix(*left_exp);
        }

        return left_exp;
    }

    // 解析标识符
    Expression *parse_identifier() const {
        auto ident = new Identifier();
        ident->token = cur_token;
        ident->value = cur_token.Literal;
        return ident;
    }

    // 解析整型字面量
    Expression *parse_integer_literal() const {
        auto lit = new IntegerLiteral();
        lit->token = cur_token;

        int value;
        try {
            value = stoi(cur_token.Literal);
        } catch (const invalid_argument &e) {
            return nullptr;
        }
        lit->value = value;
        return lit;
    }

    // 解析前缀表达式
    Expression *parse_prefix_expression() {
        auto *expression = new PrefixExpression();
        expression->token = cur_token;
        expression->op = cur_token.Literal;

        parser_next_token();

        expression->right = parse_expression(Precedence::PREFIX);

        return expression;
    }

    // 解析中缀表达式
    Expression *parse_infix_expression(Expression &left) {
        auto *expression = new InfixExpression();
        expression->token = cur_token;
        expression->left = &left;
        expression->op = cur_token.Literal;

        int precedence = cur_precedence();
        parser_next_token();
        expression->right = parse_expression(Precedence(precedence));

        return expression;
    }

    // 解析布尔表达式
    Expression *parse_boolean() const {
        auto *boolean = new Boolean();
        boolean->token = cur_token;
        boolean->value = cur_token_is(TokenType::TRUE);
        return boolean;
    }

    // 解析分组表达式
    Expression *parse_grouped_expression() {
        parser_next_token();
        auto exp = parse_expression(Precedence::LOWEST);
        if (!expect_peek(TokenType::RPAREN)) {
            return nullptr;
        }
        return exp;
    }

    // 解析语句块
    BlockStatement *parse_block_statement() {
        auto *block = new BlockStatement();
        block->token = cur_token;
        block->statements = vector<Statement *>();

        parser_next_token();

        while (!cur_token_is(TokenType::RBRACE) && !cur_token_is(TokenType::END)) {
            auto *stmt = parse_statement();
            if (stmt != nullptr) {
                block->statements.push_back(stmt);
            }
            parser_next_token();
        }

        return block;
    }

    // 解析IF表达式
    Expression *parse_if_expression() {
        auto *expression = new IfExpression();
        expression->token = cur_token;

        if (!expect_peek(TokenType::LPAREN)) {
            return nullptr;
        }

        parser_next_token();
        expression->condition = parse_expression(Precedence::LOWEST);

        if (!expect_peek(TokenType::RPAREN)) {
            return nullptr;
        }

        if (!expect_peek(TokenType::LBRACE)) {
            return nullptr;
        }

        expression->consequence = parse_block_statement();

        if (peek_token_is(TokenType::ELSE)) {
            parser_next_token();
            if (!expect_peek(TokenType::LBRACE)) {
                return nullptr;
            }
            expression->alternative = parse_block_statement();
        }

        return expression;
    }

    // 解析函数字面量表达式
    Expression *parse_function_literal() {
        auto *function = new FunctionLiteral();
        function->token = cur_token;

        if (!expect_peek(TokenType::LPAREN)) {
            return nullptr;
        }

        function->parameters = parse_function_parameters();

        if (!expect_peek(TokenType::LBRACE)) {
            return nullptr;
        }

        function->body = parse_block_statement();

        return function;
    }

    // 解析函数参数
    vector<Identifier *> parse_function_parameters() {
        vector<Identifier *> identifiers;

        if (peek_token_is(TokenType::RPAREN)) {
            parser_next_token();
            return identifiers;
        }

        parser_next_token();

        auto *ident = new Identifier();
        ident->token = cur_token;
        ident->value = cur_token.Literal;
        identifiers.push_back(ident);

        while (peek_token_is(TokenType::COMMA)) {
            parser_next_token();
            parser_next_token();

            ident = new Identifier();
            ident->token = cur_token;
            ident->value = cur_token.Literal;
            identifiers.push_back(ident);
        }

        if (!expect_peek(TokenType::RPAREN)) {
            return {};
        }

        return identifiers;
    }

    // 解析函数调用表达式
    Expression *parse_call_expression(Expression *function) {
        auto *exp = new CallExpression();
        exp->token = cur_token;
        exp->function = function;
        exp->arguments = parse_call_arguments();
        return exp;
    }

    // 解析函数调用参数
    vector<Expression *> parse_call_arguments() {
        vector<Expression *> args;

        if (peek_token_is(TokenType::RPAREN)) {
            parser_next_token();
            return args;
        }

        parser_next_token();
        args.push_back(parse_expression(Precedence::LOWEST));

        while (peek_token_is(TokenType::COMMA)) {
            parser_next_token();
            parser_next_token();
            args.push_back(parse_expression(Precedence::LOWEST));
        }

        if (!expect_peek(TokenType::RPAREN)) {
            return {};
        }

        return args;
    }

    // 解析表达式语句
    ExpressionStatement *parse_expression_statement() {
        auto *stmt = new ExpressionStatement();
        stmt->token = cur_token;
        stmt->expression = parse_expression(Precedence::LOWEST);

        if (peek_token_is(TokenType::SEMICOLON)) {
            parser_next_token();
        }
        return stmt;
    }

    // 解析var语句
    VarStatement *parse_var_statement() {
        auto *stmt = new VarStatement();
        stmt->token = cur_token;

        if (!expect_peek(TokenType::IDENT)) {
            return nullptr;
        }

        stmt->name = new Identifier;
        stmt->name->token = cur_token;
        stmt->name->value = cur_token.Literal;

        if (!expect_peek(TokenType::ASSIGN)) {
            return nullptr;
        }

        parser_next_token();

        stmt->value = parse_expression(Precedence::LOWEST);

        if (cur_token_is(TokenType::SEMICOLON)) {
            return stmt;
        }

        if (!peek_token_is(TokenType::SEMICOLON)) {
            peek_error(TokenType::SEMICOLON);
            parser_next_token();
        } else {
            parser_next_token();
        }

        return stmt;
    }

    // 解析return语句
    ReturnStatement *parse_return_statement() {
        auto *stmt = new ReturnStatement();
        stmt->token = cur_token;

        parser_next_token();

        stmt->return_value = parse_expression(Precedence::LOWEST);

        while (!cur_token_is(TokenType::SEMICOLON) && !cur_token_is(TokenType::END)) {
            parser_next_token();
        }

        return stmt;
    }

    bool cur_token_is(TokenType type) const {
        return cur_token.type == type;
    }

    bool peek_token_is(TokenType type) const {
        return peek_token.type == type;
    }

    void peek_error(TokenType type) {
        string msg = "expected next token to be " + token_type_to_string(type) + ", got " + token_type_to_string(peek_token.type) + " instead";
        errors.push_back(msg);
    }

    void no_prefix_parse_fn_error(TokenType type) {
        string msg = "no prefix parse function for " + token_type_to_string(type) + " found";
        errors.push_back(msg);
    }

    bool expect_peek(TokenType type) {
        if (peek_token_is(type)) {
            parser_next_token();
            return true;
        } else {
            peek_error(type);
            return false;
        }
    }

    int cur_precedence() const {
        if (precedences.find(cur_token.type) != precedences.end()) {
            return precedences.at(cur_token.type);
        }
        return Precedence::LOWEST;
    }

    int peek_precedence() const {
        if (precedences.find(peek_token.type) != precedences.end()) {
            return precedences.at(peek_token.type);
        }
        return Precedence::LOWEST;
    }
};

#endif //FIREFLY_INTERPRETER_PARSER_HPP
