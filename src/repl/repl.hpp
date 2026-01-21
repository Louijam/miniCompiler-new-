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

inline int run_repl() {
    std::cout << "mini_cpp REPL (:q zum Beenden)\n";

    // Global state persists across the whole session.
    interp::FunctionTable functions;
    interp::Env global(nullptr);
    // Session scope (branches off global) stays alive across REPL inputs.
    interp::Env session(&global);

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

        // Collect input
        buf += line;
        buf += "\n";

        for (char ch : line) update_balance(paren, brace, bracket, ch);

        // Not complete yet? keep collecting
        if (!is_complete_input(paren, brace, bracket)) continue;

        // Ignore whitespace-only input
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

            auto starts_with = [&](const char* kw) {
                size_t i = 0;
                while (i < src.size() && (src[i] == ' ' || src[i] == '\t' || src[i] == '\n' || src[i] == '\r')) ++i;
                size_t k = 0;
                while (kw[k] && i + k < src.size() && src[i + k] == kw[k]) ++k;
                if (!kw[k]) {
                    char next = (i + k < src.size() ? src[i + k] : '\0');
                    if (!std::isalnum((unsigned char)next) && next != '_') return true;
                }
                return false;
            };

            bool is_toplevel = starts_with("class");

            // heuristic: "type name(...){...}" => function def
            if (!is_toplevel) {
                if (starts_with("int") || starts_with("bool") || starts_with("char") || starts_with("string") || starts_with("void")) {
                    if (src.find('{') != std::string::npos) is_toplevel = true;
                }
            }

            if (is_toplevel) {
                ast::Program p = parser::Parser::parse_source(src);
                functions.add_program(p);
            } else {
                // Wrap statements into a synthetic function so we can reuse the normal parser.
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
                        interp::Value v = interp::eval_expr(session, *es->expr, functions);
                        std::cout << interp::to_string(v) << "\n";
                    } else {
                        interp::exec_stmt(session, *st, functions);
                    }
                }
            }
        } catch (const std::exception& ex) {
            std::cerr << "FEHLER: " << ex.what() << "\n";
        }

        // Reset
        buf.clear();
        paren = brace = bracket = 0;
    }
}

} // namespace repl
