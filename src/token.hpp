/* 定义词法单元 */

#ifndef FIREFLY_COMPILER_TOKEN_H
#define FIREFLY_COMPILER_TOKEN_H

#include <string>
#include <map>

using namespace std;

// 词法单元类型
enum class TokenType {
    ILLEGAL = 0, END,
    IDENT, INT, STRING, // 标识符和字面量
    ASSIGN, PLUS, MINUS, ASTERISK, SLASH, BANG, AMP, LT, GT, // 运算符 -> = + - * / ! & < >
    LE, GE, AND, OR, // 复合运算符 -> <= >= && ||
    COMMA, SEMICOLON, COLON, // 分隔符 -> , ; :
    LPAREN, RPAREN, LBRACE, RBRACE, LBRACKET, RBRACKET, // 括号 -> ( ) { } [ ]
    EQ, NOT_EQ, // 分支比较 -> == !=
    FUNC, LET, VAR, TRUE, FALSE, IF, ELSE, WHILE, RETURN // 关键字 -> func let var true false if else while return
};

struct Token {
    string Literal;
    TokenType type;
    int line{1};
    int column{1};
};

// 关键字
inline const map<string, TokenType, less<>> keywords = {
        {"func",   TokenType::FUNC},
        {"let",    TokenType::LET},
        {"var",    TokenType::VAR},
        {"true",   TokenType::TRUE},
        {"false",  TokenType::FALSE},
        {"if",     TokenType::IF},
        {"else",   TokenType::ELSE},
        {"while",  TokenType::WHILE},
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
        case TokenType::STRING:
            return "STRING";
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
        case TokenType::AMP:
            return "AMP";
        case TokenType::LT:
            return "LT";
        case TokenType::GT:
            return "GT";
        case TokenType::LE:
            return "LE";
        case TokenType::GE:
            return "GE";
        case TokenType::AND:
            return "AND";
        case TokenType::OR:
            return "OR";
        case TokenType::COMMA:
            return "COMMA";
        case TokenType::SEMICOLON:
            return "SEMICOLON";
        case TokenType::COLON:
            return "COLON";
        case TokenType::LPAREN:
            return "LPAREN";
        case TokenType::RPAREN:
            return "RPAREN";
        case TokenType::LBRACE:
            return "LBRACE";
        case TokenType::RBRACE:
            return "RBRACE";
        case TokenType::LBRACKET:
            return "LBRACKET";
        case TokenType::RBRACKET:
            return "RBRACKET";
        case TokenType::EQ:
            return "EQ";
        case TokenType::NOT_EQ:
            return "NOT_EQ";
        case TokenType::FUNC:
            return "FUNC";
        case TokenType::LET:
            return "LET";
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
        case TokenType::WHILE:
            return "WHILE";
        case TokenType::RETURN:
            return "RETURN";
        default:
            return "UNKNOWN";
    }
}

inline string token_location_to_string(const Token &token) {
    return "line " + to_string(token.line) + ", column " + to_string(token.column);
}

#endif //FIREFLY_COMPILER_TOKEN_H
