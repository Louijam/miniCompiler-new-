#pragma once
// Verhindert mehrfaches Einbinden dieser Header-Datei

#include <cctype>        // std::isspace, std::isalpha, std::isalnum, std::isdigit
#include <stdexcept>     // std::runtime_error
#include <string>        // std::string
#include <string_view>   // std::string_view
#include <vector>        // std::vector

#include "token.hpp"     // Token, TokenKind

namespace lexer {

// Lexer: wandelt Eingabetext in eine Token-Sequenz um
class Lexer {
public:
    // Nimmt eine "View" auf den Input (keine Kopie)
    explicit Lexer(std::string_view input) : input_(input) {}

    // Tokenisiert bis End-Token (TokenKind::End) und gibt alle Tokens zurück
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
    std::string_view input_; // gesamter Input
    size_t pos_ = 0;         // aktueller Index im Input
    int line_ = 1;           // aktuelle Zeile (1-based)
    int col_ = 1;            // aktuelle Spalte (1-based)

    // true, wenn wir am Ende sind
    bool eof() const { return pos_ >= input_.size(); }

    // Schaut auf das Zeichen bei pos_ + off, ohne pos_ zu verändern
    char peek(size_t off = 0) const {
        if (pos_ + off >= input_.size()) return '\0';
        return input_[pos_ + off];
    }

    // Liest ein Zeichen und bewegt pos_ vorwärts, inkl. Zeilen/Spalten-Tracking
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

    // Überspringt Whitespace + Kommentare + Präprozessorzeilen (#include etc.)
    void skip_ws_and_comments() {
        while (true) {
            // Whitespace
            while (std::isspace((unsigned char)peek())) get();

            // Präprozessorzeile: alles bis Zeilenende ignorieren
            if (peek() == '#') {
                while (!eof() && get() != '\n') {}
                continue;
            }

            // Zeilenkommentar: // ...
            if (peek() == '/' && peek(1) == '/') {
                get(); get();
                while (!eof() && get() != '\n') {}
                continue;
            }

            // Blockkommentar: /* ... */
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

    // Erzeugt ein Token mit Startposition (line/col)
    Token make_at(TokenKind k, std::string lex, int start_line, int start_col) {
        return Token{k, std::move(lex), start_line, start_col};
    }

    // Wirft einen Lexer-Fehler mit Position
    [[noreturn]] void lex_error(int at_line, int at_col, const std::string& msg) {
        throw std::runtime_error("Lexer error at " + std::to_string(at_line) + ":" +
                                 std::to_string(at_col) + ": " + msg);
    }

    // Liest genau eine Escape-Sequenz nach Backslash und liefert das "echte" Zeichen
    // Unterstützt nur die minimalen C++-artigen Escapes, die ihr braucht
    char read_escape(int start_line, int start_col) {
        if (eof()) lex_error(start_line, start_col, "unfinished escape sequence");
        char e = get();
        switch (e) {
            case 'n':  return '\n';
            case 't':  return '\t';
            case 'r':  return '\r';
            case '0':  return '\0';
            case '\\': return '\\';
            case '\'': return '\'';
            case '"':  return '"';
            default:
                lex_error(start_line, start_col, std::string("unknown escape \\") + e);
        }
        return '\0';
    }

    // Liest das nächste Token aus dem Input
    Token next_token() {
        skip_ws_and_comments();

        int start_line = line_;
        int start_col  = col_;

        if (eof()) return make_at(TokenKind::End, "", start_line, start_col);

        char c = peek();

        // Identifier / Keywords
        if (std::isalpha((unsigned char)c) || c == '_') {
            std::string s;
            while (std::isalnum((unsigned char)peek()) || peek() == '_')
                s.push_back(get());

            // Keywords
            if (s == "int")     return make_at(TokenKind::KwInt, s, start_line, start_col);
            if (s == "bool")    return make_at(TokenKind::KwBool, s, start_line, start_col);
            if (s == "char")    return make_at(TokenKind::KwChar, s, start_line, start_col);
            if (s == "string")  return make_at(TokenKind::KwString, s, start_line, start_col);
            if (s == "void")    return make_at(TokenKind::KwVoid, s, start_line, start_col);
            if (s == "true")    return make_at(TokenKind::KwTrue, s, start_line, start_col);
            if (s == "false")   return make_at(TokenKind::KwFalse, s, start_line, start_col);
            if (s == "if")      return make_at(TokenKind::KwIf, s, start_line, start_col);
            if (s == "else")    return make_at(TokenKind::KwElse, s, start_line, start_col);
            if (s == "while")   return make_at(TokenKind::KwWhile, s, start_line, start_col);
            if (s == "return")  return make_at(TokenKind::KwReturn, s, start_line, start_col);
            if (s == "class")   return make_at(TokenKind::KwClass, s, start_line, start_col);
            if (s == "public")  return make_at(TokenKind::KwPublic, s, start_line, start_col);
            if (s == "virtual") return make_at(TokenKind::KwVirtual, s, start_line, start_col);

            return make_at(TokenKind::Identifier, s, start_line, start_col);
        }

        // Integer-Literal
        if (std::isdigit((unsigned char)c)) {
            std::string s;
            while (std::isdigit((unsigned char)peek()))
                s.push_back(get());
            return make_at(TokenKind::IntLit, s, start_line, start_col);
        }

        // Char-Literal: 'a' oder '\n' etc.
        if (c == '\'') {
            std::string raw;
            raw.push_back(get()); // opening '

            if (eof()) lex_error(start_line, start_col, "unfinished char literal");

            char ch = get();
            raw.push_back(ch);

            if (ch == '\\') {
                // Escape-Sequenz
                char esc = read_escape(start_line, start_col);

                // Raw-Repräsentation "wie getippt" konservieren:
                // Wir haben '\' bereits in raw, jetzt das Escape-Kürzel ergänzen.
                raw.push_back((esc == '\n') ? 'n' :
                              (esc == '\t') ? 't' :
                              (esc == '\r') ? 'r' :
                              (esc == '\0') ? '0' :
                              (esc == '\\') ? '\\' :
                              (esc == '\'') ? '\'' :
                              (esc == '"')  ? '"'  : '?');
            }

            if (eof()) lex_error(start_line, start_col, "unfinished char literal");

            char endq = get();
            raw.push_back(endq);

            if (endq != '\'')
                lex_error(start_line, start_col, "char literal must end with '");

            return make_at(TokenKind::CharLit, raw, start_line, start_col);
        }

        // String-Literal: "foo" (inkl. minimaler Escape-Unterstützung)
        if (c == '"') {
            std::string raw;
            raw.push_back(get()); // opening "

            while (true) {
                if (eof()) lex_error(start_line, start_col, "unfinished string literal");

                char ch = get();
                raw.push_back(ch);

                if (ch == '"') break; // Ende des Strings

                if (ch == '\n')
                    lex_error(start_line, start_col, "newline in string literal");

                if (ch == '\\') {
                    // Escape: genau ein weiteres Zeichen konsumieren
                    if (eof()) lex_error(start_line, start_col, "unfinished escape in string literal");
                    char esc_char = get();
                    raw.push_back(esc_char);

                    // Escape validieren
                    switch (esc_char) {
                        case 'n': case 't': case 'r': case '0':
                        case '\\': case '"': case '\'':
                            break;
                        default:
                            lex_error(start_line, start_col,
                                      std::string("unknown escape \\") + esc_char);
                    }
                }
            }

            return make_at(TokenKind::StringLit, raw, start_line, start_col);
        }

        // Zwei-Zeichen-Operatoren
        if (c == '&' && peek(1) == '&') { get(); get(); return make_at(TokenKind::AndAnd, "&&", start_line, start_col); }
        if (c == '|' && peek(1) == '|') { get(); get(); return make_at(TokenKind::OrOr,  "||", start_line, start_col); }
        if (c == '=' && peek(1) == '=') { get(); get(); return make_at(TokenKind::EqEq,  "==", start_line, start_col); }
        if (c == '!' && peek(1) == '=') { get(); get(); return make_at(TokenKind::NotEq, "!=", start_line, start_col); }
        if (c == '<' && peek(1) == '=') { get(); get(); return make_at(TokenKind::LessEq, "<=", start_line, start_col); }
        if (c == '>' && peek(1) == '=') { get(); get(); return make_at(TokenKind::GreaterEq, ">=", start_line, start_col); }

        // Ein-Zeichen-Tokens
        get(); // aktuelles Zeichen konsumieren
        switch (c) {
            case '(': return make_at(TokenKind::LParen, "(", start_line, start_col);
            case ')': return make_at(TokenKind::RParen, ")", start_line, start_col);
            case '{': return make_at(TokenKind::LBrace, "{", start_line, start_col);
            case '}': return make_at(TokenKind::RBrace, "}", start_line, start_col);
            case ';': return make_at(TokenKind::Semicolon, ";", start_line, start_col);
            case ',': return make_at(TokenKind::Comma, ",", start_line, start_col);
            case '.': return make_at(TokenKind::Dot, ".", start_line, start_col);
            case ':': return make_at(TokenKind::Colon, ":", start_line, start_col);
            case '&': return make_at(TokenKind::Amp, "&", start_line, start_col);
            case '=': return make_at(TokenKind::Assign, "=", start_line, start_col);
            case '+': return make_at(TokenKind::Plus, "+", start_line, start_col);
            case '-': return make_at(TokenKind::Minus, "-", start_line, start_col);
            case '*': return make_at(TokenKind::Star, "*", start_line, start_col);
            case '/': return make_at(TokenKind::Slash, "/", start_line, start_col);
            case '%': return make_at(TokenKind::Percent, "%", start_line, start_col);
            case '!': return make_at(TokenKind::Bang, "!", start_line, start_col);
            case '<': return make_at(TokenKind::Less, "<", start_line, start_col);
            case '>': return make_at(TokenKind::Greater, ">", start_line, start_col);
            default:
                lex_error(start_line, start_col,
                          std::string("unknown character '") + c + "'");
        }
    }
};

} // namespace lexer
