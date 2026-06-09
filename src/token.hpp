/* 定义词法单元 */

#ifndef FIREFLY_INTERPRETER_TOKEN_H
#define FIREFLY_INTERPRETER_TOKEN_H

#include <string>
#include <map>

using namespace std;

// 词法单元类型
enum class TokenType {
    ILLEGAL = 0, END,
    IDENT, INT, // 标识符
    ASSIGN, PLUS, MINUS, ASTERISK, SLASH, BANG, LT, GT, // 运算符 -> = + - * / ! < >
    COMMA, SEMICOLON, // 分隔符 -> , ;
    LPAREN, RPAREN, LBRACE, RBRACE, // 括号 -> ( ) { }
    EQ, NOT_EQ, // 分支比较 -> == !=
    FUNC, VAR, TRUE, FALSE, IF, ELSE, RETURN // 关键字 -> func var true false if else return
};

struct Token {
    string Literal;
    TokenType type;
};

// 关键字
inline const map<string, TokenType, less<>> keywords = {
        {"func",   TokenType::FUNC},
        {"var",    TokenType::VAR},
        {"true",   TokenType::TRUE},
        {"false",  TokenType::FALSE},
        {"if",     TokenType::IF},
        {"else",   TokenType::ELSE},
        {"return", TokenType::RETURN}
};

inline TokenType lookup_ident(const string &ident) {
    auto it = keywords.find(ident);
    if (it != keywords.end()) {
        return it->second;
    }
    return TokenType::IDENT;
}

inline string token_type_to_string(TokenType type) {
    switch (type) {
        case TokenType::ILLEGAL:
            return "ILLEGAL";
        case TokenType::END:
            return "END";
        case TokenType::IDENT:
            return "IDENT";
        case TokenType::INT:
            return "INT";
        case TokenType::ASSIGN:
            return "ASSIGN";
        case TokenType::PLUS:
            return "PLUS";
        case TokenType::MINUS:
            return "MINUS";
        case TokenType::ASTERISK:
            return "ASTERISK";
        case TokenType::SLASH:
            return "SLASH";
        case TokenType::BANG:
            return "BANG";
        case TokenType::LT:
            return "LT";
        case TokenType::GT:
            return "GT";
        case TokenType::COMMA:
            return "COMMA";
        case TokenType::SEMICOLON:
            return "SEMICOLON";
        case TokenType::LPAREN:
            return "LPAREN";
        case TokenType::RPAREN:
            return "RPAREN";
        case TokenType::LBRACE:
            return "LBRACE";
        case TokenType::RBRACE:
            return "RBRACE";
        case TokenType::EQ:
            return "EQ";
        case TokenType::NOT_EQ:
            return "NOT_EQ";
        case TokenType::FUNC:
            return "FUNC";
        case TokenType::VAR:
            return "VAR";
        case TokenType::TRUE:
            return "TRUE";
        case TokenType::FALSE:
            return "FALSE";
        case TokenType::IF:
            return "IF";
        case TokenType::ELSE:
            return "ELSE";
        case TokenType::RETURN:
            return "RETURN";
        default:
            return "UNKNOWN";
    }
}

#endif //FIREFLY_INTERPRETER_TOKEN_H
