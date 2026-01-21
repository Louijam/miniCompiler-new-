#pragma once

#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#include "../lexer/lexer.hpp"

namespace mini_cpp {

inline bool maybe_dump_tokens(int argc, char** argv) {
    if (argc == 3 && std::string(argv[1]) == "--dump-tokens") {
        const std::string path = argv[2];
        std::ifstream in(path);
        if (!in) {
            std::cerr << "error: cannot open file: " << path << "\n";
            return true;
        }

        std::string src((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

        lexer::Lexer lx(src);
        auto toks = lx.tokenize();
        for (const auto& t : toks) {
            std::cout << lexer::to_string(t.kind) << "  "
                      << "'" << t.lexeme << "'" << "  "
                      << t.line << ":" << t.col << "\n";
        }
        return true;
    }
    return false;
}

} // namespace mini_cpp
