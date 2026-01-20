#pragma once

#include <string>

namespace lexer {

enum class TokenKind {
    End,

    Identifier,
    IntLit,
    CharLit,
    StringLit,

    // Keywords
    KwInt,
    KwBool,
    KwChar,
    KwString,
    KwVoid,
    KwTrue,
    KwFalse,
    KwIf,
    KwElse,
    KwWhile,
    KwReturn,
    KwClass,
    KwPublic,
    KwVirtual,

    // Punctuation
    LParen,     // (
    RParen,     // )
    LBrace,     // {
    RBrace,     // }
    Semicolon,  // ;
    Comma,      // ,
    Dot,        // .
    Colon,      // :
    Amp,        // &

    // Operators
    Assign,     // =
    Plus,       // +
    Minus,      // -
    Star,       // *
    Slash,      // /
    Percent,    // %
    Bang,       // !
    AndAnd,     // &&
    OrOr,       // ||
    EqEq,       // ==
    NotEq,      // !=
    Less,       // <
    LessEq,     // <=
    Greater,    // >
    GreaterEq   // >=
};

struct Token {
    TokenKind kind{TokenKind::End};
    std::string lexeme{};
    int line{1};
    int col{1};
};

inline const char* to_string(TokenKind k) {
    switch (k) {
        case TokenKind::End: return "End";
        case TokenKind::Identifier: return "Identifier";
        case TokenKind::IntLit: return "IntLit";
        case TokenKind::CharLit: return "CharLit";
        case TokenKind::StringLit: return "StringLit";

        case TokenKind::KwInt: return "KwInt";
        case TokenKind::KwBool: return "KwBool";
        case TokenKind::KwChar: return "KwChar";
        case TokenKind::KwString: return "KwString";
        case TokenKind::KwVoid: return "KwVoid";
        case TokenKind::KwTrue: return "KwTrue";
        case TokenKind::KwFalse: return "KwFalse";
        case TokenKind::KwIf: return "KwIf";
        case TokenKind::KwElse: return "KwElse";
        case TokenKind::KwWhile: return "KwWhile";
        case TokenKind::KwReturn: return "KwReturn";
        case TokenKind::KwClass: return "KwClass";
        case TokenKind::KwPublic: return "KwPublic";
        case TokenKind::KwVirtual: return "KwVirtual";

        case TokenKind::LParen: return "LParen";
        case TokenKind::RParen: return "RParen";
        case TokenKind::LBrace: return "LBrace";
        case TokenKind::RBrace: return "RBrace";
        case TokenKind::Semicolon: return "Semicolon";
        case TokenKind::Comma: return "Comma";
        case TokenKind::Dot: return "Dot";
        case TokenKind::Colon: return "Colon";
        case TokenKind::Amp: return "Amp";

        case TokenKind::Assign: return "Assign";
        case TokenKind::Plus: return "Plus";
        case TokenKind::Minus: return "Minus";
        case TokenKind::Star: return "Star";
        case TokenKind::Slash: return "Slash";
        case TokenKind::Percent: return "Percent";
        case TokenKind::Bang: return "Bang";
        case TokenKind::AndAnd: return "AndAnd";
        case TokenKind::OrOr: return "OrOr";
        case TokenKind::EqEq: return "EqEq";
        case TokenKind::NotEq: return "NotEq";
        case TokenKind::Less: return "Less";
        case TokenKind::LessEq: return "LessEq";
        case TokenKind::Greater: return "Greater";
        case TokenKind::GreaterEq: return "GreaterEq";
    }
    return "Unknown";
}

} 
