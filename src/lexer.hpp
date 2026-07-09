/* 定义词法分析器 */

#ifndef FIREFLY_COMPILER_LEXER_HPP
#define FIREFLY_COMPILER_LEXER_HPP

#include "token.hpp"
#include <string>
#include <utility>

struct Lexer {
    string input;
    int position{}; // 当前字符
    int read_position{}; // 将要读取的字符
    char ch{}; // 当前字符
    int line{1};
    int column{};
    int next_line{1};
    int next_column{};
};

void read_char(Lexer *l) {
    if (l->read_position >= l->input.length()) {
        l->ch = 0;
        l->line = l->next_line;
        l->column = l->next_column + 1;
    } else {
        l->ch = l->input[l->read_position];
        l->line = l->next_line;
        l->column = l->next_column + 1;
        if (l->ch == '\n') {
            l->next_line++;
            l->next_column = 0;
        } else {
            l->next_column = l->column;
        }
    }
    l->position = l->read_position;
    l->read_position += 1;
}

char peek_char(Lexer *l) {
    if (l->read_position >= l->input.length()) {
        return 0;
    } else {
        return l->input[l->read_position];
    }
}

// 识别字符 -> 字母 下划线
bool isletter(char ch) {
    return 'a' <= ch && ch <= 'z' || 'A' <= ch && ch <= 'Z' || ch == '_';
}

// 识别数字 -> 整型
bool isdigit(char ch) {
    return '0' <= ch && ch <= '9';
}

// 跳过空白字符
void eat_whitespace(Lexer *l) {
    while (l->ch == ' ' || l->ch == '\t' || l->ch == '\n' || l->ch == '\r') {
        read_char(l);
    }
}

string read_identifier(Lexer *l) {
    int position = l->position;
    while (isletter(l->ch) || isdigit(l->ch)) {
        read_char(l);
    }
    return l->input.substr(position, (l->position - position));
}

string read_number(Lexer *l) {
    int position = l->position;
    while (isdigit(l->ch)) {
        read_char(l);
    }
    return l->input.substr(position, (l->position - position));
}

string read_string(Lexer *l) {
    string result;

    while (true) {
        read_char(l);
        if (l->ch == '"' || l->ch == 0) {
            break;
        }

        if (l->ch == '\\') {
            char next = peek_char(l);
            switch (next) {
                case 'n':
                    result += '\n';
                    break;
                case 't':
                    result += '\t';
                    break;
                case '"':
                    result += '"';
                    break;
                case '\\':
                    result += '\\';
                    break;
                default:
                    result += next;
                    break;
            }
            read_char(l);
        } else {
            result += l->ch;
        }
    }

    return result;
}

// 创建token时如需要将char转换为string，使用该函数
Token new_token(Lexer *l, char ch, TokenType type) {
    return Token{.Literal = string(1, ch), .type = type, .line = l->line, .column = l->column};
}

Token next_token(Lexer *l) {
    Token tok;
    eat_whitespace(l);
    int token_line = l->line;
    int token_column = l->column;
    switch (l->ch) {
        case '=' :
            if (peek_char(l) == '=') {
                char ch = l->ch;
                read_char(l);
                string literal = string(1, ch) + string(1, l->ch);
                tok = Token{.Literal =  literal, .type =  TokenType::EQ, .line = token_line, .column = token_column};
            } else {
                tok = new_token(l, l->ch, TokenType::ASSIGN);
            }
            break;
        case '+' :
            tok = new_token(l, l->ch, TokenType::PLUS);
            break;
        case '-' :
            tok = new_token(l, l->ch, TokenType::MINUS);
            break;
        case '*' :
            tok = new_token(l, l->ch, TokenType::ASTERISK);
            break;
        case '/' :
            tok = new_token(l, l->ch, TokenType::SLASH);
            break;
        case '!' :
            if (peek_char(l) == '=') {
                char ch = l->ch;
                read_char(l);
                string literal = string(1, ch) + string(1, l->ch);
                tok = Token{.Literal = literal, .type = TokenType::NOT_EQ, .line = token_line, .column = token_column};
            } else {
                tok = new_token(l, l->ch, TokenType::BANG);
            }
            break;
        case '<' :
            if (peek_char(l) == '=') {
                char ch = l->ch;
                read_char(l);
                string literal = string(1, ch) + string(1, l->ch);
                tok = Token{.Literal = literal, .type = TokenType::LE, .line = token_line, .column = token_column};
            } else {
                tok = new_token(l, l->ch, TokenType::LT);
            }
            break;
        case '>' :
            if (peek_char(l) == '=') {
                char ch = l->ch;
                read_char(l);
                string literal = string(1, ch) + string(1, l->ch);
                tok = Token{.Literal = literal, .type = TokenType::GE, .line = token_line, .column = token_column};
            } else {
                tok = new_token(l, l->ch, TokenType::GT);
            }
            break;
        case '&' :
            if (peek_char(l) == '&') {
                char ch = l->ch;
                read_char(l);
                string literal = string(1, ch) + string(1, l->ch);
                tok = Token{.Literal = literal, .type = TokenType::AND, .line = token_line, .column = token_column};
            } else {
                tok = new_token(l, l->ch, TokenType::AMP);
            }
            break;
        case '|' :
            if (peek_char(l) == '|') {
                char ch = l->ch;
                read_char(l);
                string literal = string(1, ch) + string(1, l->ch);
                tok = Token{.Literal = literal, .type = TokenType::OR, .line = token_line, .column = token_column};
            } else {
                tok = new_token(l, l->ch, TokenType::ILLEGAL);
            }
            break;
        case ',' :
            tok = new_token(l, l->ch, TokenType::COMMA);
            break;
        case ';' :
            tok = new_token(l, l->ch, TokenType::SEMICOLON);
            break;
        case ':' :
            tok = new_token(l, l->ch, TokenType::COLON);
            break;
        case '(' :
            tok = new_token(l, l->ch, TokenType::LPAREN);
            break;
        case ')' :
            tok = new_token(l, l->ch, TokenType::RPAREN);
            break;
        case '{' :
            tok = new_token(l, l->ch, TokenType::LBRACE);
            break;
        case '}' :
            tok = new_token(l, l->ch, TokenType::RBRACE);
            break;
        case '[' :
            tok = new_token(l, l->ch, TokenType::LBRACKET);
            break;
        case ']' :
            tok = new_token(l, l->ch, TokenType::RBRACKET);
            break;
        case '"' :
            tok.Literal = read_string(l);
            tok.type = TokenType::STRING;
            tok.line = token_line;
            tok.column = token_column;
            break;
        case 0 :
            tok = new_token(l, l->ch, TokenType::END);
            break;
        default:
            if (isletter(l->ch)) {
                tok.Literal = read_identifier(l);
                tok.type = lookup_ident(tok.Literal);
                tok.line = token_line;
                tok.column = token_column;
                return tok;
            } else if (isdigit(l->ch)) {
                tok.Literal = read_number(l);
                tok.type = TokenType::INT;
                tok.line = token_line;
                tok.column = token_column;
                return tok;
            } else {
                tok = new_token(l, l->ch, TokenType::ILLEGAL);
            }
    }
    read_char(l);
    return tok;
}

Lexer *new_lex(string input) {
    auto *l = new Lexer();
    l->input = std::move(input);
    read_char(l);
    return l;
}

#endif //FIREFLY_COMPILER_LEXER_HPP
