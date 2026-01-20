#pragma once
#include <iostream>
#include <string>

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

        // Eingabe sammeln
        buf += line;
        buf += "\n";

        for (char ch : line) update_balance(paren, brace, bracket, ch);

        // Noch nicht komplett? weiter sammeln
        if (!is_complete_input(paren, brace, bracket)) continue;

        // Leere/Whitespace-only Eingabe ignorieren
        bool only_ws = true;
        for (char ch : buf) {
            if (!(ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r')) { only_ws = false; break; }
        }
        if (only_ws) {
            buf.clear();
            paren = brace = bracket = 0;
            continue;
        }

        // Platzhalter bis Parser+ASTBuilder da sind:
        std::cout << "TODO parse+exec:\n" << buf;

        // Reset
        buf.clear();
        paren = brace = bracket = 0;
    }
}

} // namespace repl
