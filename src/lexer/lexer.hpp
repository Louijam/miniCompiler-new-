#pragma once

#include <cctype>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "lexer/token.hpp"

namespace lexer {

class Lexer {
public:
    explicit Lexer(std::string_view input) : input_(input) {}

    std::vector<Token> tokenize() {
        std::vector<Token> tokens;
        while (true) {
            Token t = next_token();
            tokens.push_back(t);
            if (t.kind == TokenKind::End) break;
        }
        return tokens;
    }

private:
    std::string_view input_;
    size_t pos_ = 0;
    int line_ = 1;
    int col_ = 1;

    bool eof() const { return pos_ >= input_.size(); }

    char peek(size_t off = 0) const {
        if (pos_ + off >= input_.size()) return '\0';
        return input_[pos_ + off];
    }

    char get() {
        if (eof()) return '\0';
        char c = input_[pos_++];
        if (c == '\n') {
            line_++;
            col_ = 1;
        } else {
            col_++;
        }
        return c;
    }

    void skip_ws_and_comments() {
        while (true) {
            while (std::isspace((unsigned char)peek())) get();

            // preprocessor line
            if (peek() == '#') {
                while (!eof() && get() != '\n') {}
                continue;
            }

            // line comment
            if (peek() == '/' && peek(1) == '/') {
                get(); get();
                while (!eof() && get() != '\n') {}
                continue;
            }

            // block comment
            if (peek() == '/' && peek(1) == '*') {
                get(); get();
                while (!eof()) {
                    if (peek() == '*' && peek(1) == '/') {
                        get(); get();
                        break;
                    }
                    get();
                }
                continue;
            }

            break;
        }
    }

    Token make(TokenKind k, std::string lex) {
        return Token{k, lex, line_, col_};
    }

    Token next_token() {
        skip_ws_and_comments();

        if (eof()) return make(TokenKind::End, "");

        char c = peek();

        // identifiers / keywords
        if (std::isalpha((unsigned char)c) || c == '_') {
            std::string s;
            while (std::isalnum((unsigned char)peek()) || peek() == '_')
                s.push_back(get());

            if (s == "int") return make(TokenKind::KwInt, s);
            if (s == "bool") return make(TokenKind::KwBool, s);
            if (s == "char") return make(TokenKind::KwChar, s);
            if (s == "string") return make(TokenKind::KwString, s);
            if (s == "void") return make(TokenKind::KwVoid, s);
            if (s == "true") return make(TokenKind::KwTrue, s);
            if (s == "false") return make(TokenKind::KwFalse, s);
            if (s == "if") return make(TokenKind::KwIf, s);
            if (s == "else") return make(TokenKind::KwElse, s);
            if (s == "while") return make(TokenKind::KwWhile, s);
            if (s == "return") return make(TokenKind::KwReturn, s);
            if (s == "class") return make(TokenKind::KwClass, s);
            if (s == "public") return make(TokenKind::KwPublic, s);
            if (s == "virtual") return make(TokenKind::KwVirtual, s);

            return make(TokenKind::Identifier, s);
        }

        // numbers
        if (std::isdigit((unsigned char)c)) {
            std::string s;
            while (std::isdigit((unsigned char)peek()))
                s.push_back(get());
            return make(TokenKind::IntLit, s);
        }

        // two-char operators
        if (c == '&' && peek(1) == '&') { get(); get(); return make(TokenKind::AndAnd, "&&"); }
        if (c == '|' && peek(1) == '|') { get(); get(); return make(TokenKind::OrOr, "||"); }
        if (c == '=' && peek(1) == '=') { get(); get(); return make(TokenKind::EqEq, "=="); }
        if (c == '!' && peek(1) == '=') { get(); get(); return make(TokenKind::NotEq, "!="); }
        if (c == '<' && peek(1) == '=') { get(); get(); return make(TokenKind::LessEq, "<="); }
        if (c == '>' && peek(1) == '=') { get(); get(); return make(TokenKind::GreaterEq, ">="); }

        // single-char
        get();
        switch (c) {
            case '(': return make(TokenKind::LParen, "(");
            case ')': return make(TokenKind::RParen, ")");
            case '{': return make(TokenKind::LBrace, "{");
            case '}': return make(TokenKind::RBrace, "}");
            case ';': return make(TokenKind::Semicolon, ";");
            case ',': return make(TokenKind::Comma, ",");
            case '.': return make(TokenKind::Dot, ".");
            case ':': return make(TokenKind::Colon, ":");
            case '&': return make(TokenKind::Amp, "&");
            case '=': return make(TokenKind::Assign, "=");
            case '+': return make(TokenKind::Plus, "+");
            case '-': return make(TokenKind::Minus, "-");
            case '*': return make(TokenKind::Star, "*");
            case '/': return make(TokenKind::Slash, "/");
            case '%': return make(TokenKind::Percent, "%");
            case '!': return make(TokenKind::Bang, "!");
            case '<': return make(TokenKind::Less, "<");
            case '>': return make(TokenKind::Greater, ">");
            default:
                throw std::runtime_error("Lexer error: unknown character");
        }
    }
};

} // namespace lexer
