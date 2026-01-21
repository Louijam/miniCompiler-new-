#pragma once
// Verhindert mehrfaches Einbinden dieser Header-Datei

#include <cctype>    // std::isalnum, std::isalpha
#include <iostream>  // std::cout, std::cerr
#include <string>    // std::string

#include "preprocess.hpp"     // strip_preprocessor_lines()

#include "../parser/parser.hpp" // Parser::parse_source()

#include "../interp/env.hpp"      // Env (Scopes/Variablen)
#include "../interp/exec.hpp"     // exec_stmt(), eval_expr()
#include "../interp/functions.hpp"// FunctionTable

namespace repl {

// Erlaubte Exit-Kommandos fuer die REPL
inline bool is_exit_cmd(const std::string& s) {
    return s == ":q" || s == ":quit" || s == "exit" || s == "quit";
}

// Aktualisiert Klammer-Bilanz fuer Multi-Line Eingaben
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

// Input ist "vollständig", wenn keine offenen Klammern mehr existieren
inline bool is_complete_input(int paren, int brace, int bracket) {
    return paren <= 0 && brace <= 0 && bracket <= 0;
}

// Schneidet führende Whitespaces ab (kopierend)
inline std::string ltrim_copy(const std::string& s) {
    size_t i = 0;
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) ++i;
    return s.substr(i);
}

// Prüft, ob src (nach trim) mit einem Keyword beginnt und danach kein Identifier-Zeichen folgt
inline bool starts_with_kw(const std::string& src, const char* kw) {
    std::string t = ltrim_copy(src);
    size_t k = 0;
    while (kw[k] && k < t.size() && t[k] == kw[k]) ++k;

    if (!kw[k]) {
        // Keyword komplett gematcht: nächstes Zeichen darf kein Ident-Char sein
        char next = (k < t.size() ? t[k] : '\0');
        if (!std::isalnum((unsigned char)next) && next != '_') return true;
    }
    return false;
}

// Heuristik: erkennt Funktionsdefinition im REPL-Input
// Muster: <type-ish> <ident> ( ... ) { ...
// Auch Klassentyp als ReturnType möglich: Identifier Identifier '(' ... '{'
inline bool looks_like_function_def(const std::string& src) {
    std::string t = ltrim_copy(src);

    // Quick reject: muss '(' enthalten und danach irgendwo '{'
    size_t p = t.find('(');
    size_t b = t.find('{');
    if (p == std::string::npos || b == std::string::npos) return false;
    if (b < p) return false;

    // Sehr einfache "Tokenisierung": liest Identifier-artige Tokens
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
    std::string tok0 = read_ident(i); // "type"
    if (tok0.empty()) return false;

    std::string tok1 = read_ident(i); // "name"
    if (tok1.empty()) return false;

    // Erwartet direkt '(' danach (nach Whitespace)
    while (i < t.size() && (t[i] == ' ' || t[i] == '\t' || t[i] == '\n' || t[i] == '\r')) ++i;
    if (i >= t.size() || t[i] != '(') return false;

    return true;
}

// Global Definitionen sind:
// - class ...
// - function def ...
inline bool is_global_definition(const std::string& src) {
    if (starts_with_kw(src, "class")) return true;
    if (looks_like_function_def(src)) return true;
    return false;
}

// Baut die Runtime-Tabellen aus dem global_program neu auf
inline void rebuild(ast::Program& global_program, interp::FunctionTable& functions) {
    functions.add_program(global_program);
}

// Haupt-REPL: teilt Input in "global" (klassen/funktionen) und "session" (statements) auf
inline int run_repl(ast::Program& global_program,
                    interp::FunctionTable& functions,
                    interp::Env& global_env,
                    interp::Env& session_env) {
    (void)global_env; // global_env bleibt als Parent fuer session_env erhalten

    std::cout << "mini_cpp REPL (:q zum Beenden)\n";

    std::string buf;               // sammelt ggf. Multi-Line Input
    int paren = 0, brace = 0, bracket = 0; // Balance-Zaehler

    while (true) {
        std::cout << (buf.empty() ? "> " : "... ") << std::flush;

        std::string line;
        if (!std::getline(std::cin, line)) {
            // EOF (Ctrl-D)
            std::cout << "\n";
            return 0;
        }

        // Exit nur, wenn gerade kein Multi-Line Buffer offen ist
        if (buf.empty() && is_exit_cmd(line)) {
            std::cout << "Bye.\n";
            return 0;
        }

        // Zeile in Buffer übernehmen
        buf += line;
        buf += "\n";

        // Klammerbilanz anhand der neuen Zeile updaten
        for (char ch : line) update_balance(paren, brace, bracket, ch);

        // Wenn noch Klammern offen sind: weiter lesen
        if (!is_complete_input(paren, brace, bracket)) continue;

        // Wenn Buffer nur Whitespace ist: Reset und weiter
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
            // Präprozessorzeilen (z.B. #include) entfernen
            std::string src = repl::strip_preprocessor_lines(buf);

            if (is_global_definition(src)) {
                // Global: Klassen + Funktionen (kein Zugriff auf Session-Variablen)
                ast::Program p = parser::Parser::parse_source(src);

                // In das globale Programm "anhängen"
                for (auto& c : p.classes)    global_program.classes.push_back(std::move(c));
                for (auto& f : p.functions)  global_program.functions.push_back(std::move(f));

                // Runtime neu bauen
                rebuild(global_program, functions);
            } else {
                // Session: einzelne Statements/Exprs ausführen.
                // Trick: in eine Wrapper-Funktion packen, damit Parser nur "programm" parsen muss.
                std::string wrapped = "void __repl__() {\n";
                wrapped += src;
                wrapped += "\n}\n";

                ast::Program p = parser::Parser::parse_source(wrapped);
                if (p.functions.empty())
                    throw std::runtime_error("internal: REPL wrapper produced no function");

                const ast::FunctionDef& f = p.functions.front();
                auto* body = dynamic_cast<const ast::BlockStmt*>(f.body.get());
                if (!body)
                    throw std::runtime_error("internal: REPL wrapper body is not a block");

                // Statements aus dem Block nacheinander ausführen
                for (const auto& st : body->statements) {
                    // ExprStmt: Wert ausgeben (REPL-typisch)
                    if (auto* es = dynamic_cast<const ast::ExprStmt*>(st.get())) {
                        interp::Value v = interp::eval_expr(session_env, *es->expr, functions);
                        std::cout << interp::to_string(v) << "\n";
                    } else {
                        // "normale" Statements
                        interp::exec_stmt(session_env, *st, functions);
                    }
                }
            }
        } catch (const std::exception& ex) {
            std::cerr << "FEHLER: " << ex.what() << "\n";
        }

        // Buffer reset nach kompletter Verarbeitung
        buf.clear();
        paren = brace = bracket = 0;
    }
}

// Convenience: erstellt globale Strukturen und startet REPL
inline int run_repl() {
    ast::Program global_program;        // speichert dauerhaft alle globalen class/function defs
    interp::FunctionTable functions;    // runtime tables fuer overloads + class runtime
    interp::Env global_env(nullptr);    // globaler Scope
    interp::Env session_env(&global_env); // Session-Scope (mit global als Parent)

    rebuild(global_program, functions); // initial: leere Tabellen ok
    return run_repl(global_program, functions, global_env, session_env);
}

} // namespace repl
