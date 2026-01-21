#pragma once
#include <cctype>
#include <iostream>
#include <string>

#include "preprocess.hpp"

#include "../parser/parser.hpp"

#include "../interp/env.hpp"
#include "../interp/exec.hpp"
#include "../interp/functions.hpp"

namespace repl {

inline bool is_exit_cmd(const std::string& s) {
    return s == ":q" || s == ":quit" || s == "exit" || s == "quit";
}

inline void update_balance(int& paren, int& brace, int& bracket, char ch) {
    switch (ch) {
        case '(': ++paren; break;
        case ')': --paren; break;
        case '{': ++brace; break;
        case '}': --brace; break;
        case '[': ++bracket; break;
        case ']': --bracket; break;
        default: break;
    }
}

inline bool is_complete_input(int paren, int brace, int bracket) {
    return paren <= 0 && brace <= 0 && bracket <= 0;
}

inline std::string ltrim_copy(const std::string& s) {
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) ++i;
    return s.substr(i);
}

inline bool starts_with_kw(const std::string& src, const char* kw) {
    std::string t = ltrim_copy(src);
    size_t k = 0;
    while (kw[k] && k < t.size() && t[k] == kw[k]) ++k;
    if (!kw[k]) {
        char next = (k < t.size() ? t[k] : '\0');
        if (!std::isalnum((unsigned char)next) && next != '_') return true;
    }
    return false;
}

// Detect: <type> <ident> ( ... ) {   (function def)
// We ignore return type being class name too: Identifier Identifier '(' ... '{'
inline bool looks_like_function_def(const std::string& src) {
    std::string t = ltrim_copy(src);

    // quick reject: must contain '(' and '{' after it
    size_t p = t.find('(');
    size_t b = t.find('{');
    if (p == std::string::npos || b == std::string::npos) return false;
    if (b < p) return false;

    // tokenize first ~3 tokens (very simple)
    // token0: type-ish, token1: name-ish, token2 should be '('
    auto read_ident = [&](size_t& i) -> std::string {
        while (i < t.size() && (t[i] == ' ' || t[i] == '\t' || t[i] == '\n' || t[i] == '\r')) ++i;
        size_t start = i;
        if (i < t.size() && (std::isalpha((unsigned char)t[i]) || t[i] == '_')) {
            ++i;
            while (i < t.size() && (std::isalnum((unsigned char)t[i]) || t[i] == '_')) ++i;
            return t.substr(start, i - start);
        }
        return "";
    };

    size_t i = 0;
    std::string tok0 = read_ident(i);
    if (tok0.empty()) return false;

    std::string tok1 = read_ident(i);
    if (tok1.empty()) return false;

    while (i < t.size() && (t[i] == ' ' || t[i] == '\t' || t[i] == '\n' || t[i] == '\r')) ++i;
    if (i >= t.size() || t[i] != '(') return false;

    // reject variable decl "int x = ..." (has no '{' usually) -> already rejected by missing '{'
    // also reject "int x;" etc.

    return true;
}

inline bool is_global_definition(const std::string& src) {
    if (starts_with_kw(src, "class")) return true;
    if (looks_like_function_def(src)) return true;
    return false;
}

inline void rebuild(ast::Program& global_program, interp::FunctionTable& functions) {
    functions.add_program(global_program);
}

inline int run_repl(ast::Program& global_program,
                    interp::FunctionTable& functions,
                    interp::Env& global_env,
                    interp::Env& session_env) {
    (void)global_env;

    std::cout << "mini_cpp REPL (:q zum Beenden)\n";

    std::string buf;
    int paren = 0, brace = 0, bracket = 0;

    while (true) {
        std::cout << (buf.empty() ? "> " : "... ") << std::flush;

        std::string line;
        if (!std::getline(std::cin, line)) {
            std::cout << "\n";
            return 0;
        }

        if (buf.empty() && is_exit_cmd(line)) {
            std::cout << "Bye.\n";
            return 0;
        }

        buf += line;
        buf += "\n";

        for (char ch : line) update_balance(paren, brace, bracket, ch);

        if (!is_complete_input(paren, brace, bracket)) continue;

        bool only_ws = true;
        for (char ch : buf) {
            if (!(ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r')) { only_ws = false; break; }
        }
        if (only_ws) {
            buf.clear();
            paren = brace = bracket = 0;
            continue;
        }

        try {
            std::string src = repl::strip_preprocessor_lines(buf);

            if (is_global_definition(src)) {
                // Global: classes + functions. Must NOT see session vars.
                ast::Program p = parser::Parser::parse_source(src);

                for (auto& c : p.classes) global_program.classes.push_back(std::move(c));
                for (auto& f : p.functions) global_program.functions.push_back(std::move(f));

                rebuild(global_program, functions);
            } else {
                // Session: statements / exprstmts / var decls etc.
                std::string wrapped = "void __repl__() {\n";
                wrapped += src;
                wrapped += "\n}\n";

                ast::Program p = parser::Parser::parse_source(wrapped);
                if (p.functions.empty()) throw std::runtime_error("internal: REPL wrapper produced no function");

                const ast::FunctionDef& f = p.functions.front();
                auto* body = dynamic_cast<const ast::BlockStmt*>(f.body.get());
                if (!body) throw std::runtime_error("internal: REPL wrapper body is not a block");

                for (const auto& st : body->statements) {
                    if (auto* es = dynamic_cast<const ast::ExprStmt*>(st.get())) {
                        interp::Value v = interp::eval_expr(session_env, *es->expr, functions);
                        std::cout << interp::to_string(v) << "\n";
                    } else {
                        interp::exec_stmt(session_env, *st, functions);
                    }
                }
            }
        } catch (const std::exception& ex) {
            std::cerr << "FEHLER: " << ex.what() << "\n";
        }

        buf.clear();
        paren = brace = bracket = 0;
    }
}

inline int run_repl() {
    ast::Program global_program;
    interp::FunctionTable functions;
    interp::Env global_env(nullptr);
    interp::Env session_env(&global_env);

    rebuild(global_program, functions);
    return run_repl(global_program, functions, global_env, session_env);
}

} // namespace repl
