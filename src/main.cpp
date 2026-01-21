#include <iostream>
#include <string>

#include "parser/parser.hpp"

int main() {
    try {
        const std::string source = R"(
            int add(int a, int b) {
                return a + b;
            }
        )";

        ast::Program prog = parser::Parser::parse_source(source);

        std::cout << "PARSER OK\n";
        std::cout << "Functions: " << prog.functions.size() << "\n";
        std::cout << "Classes: " << prog.classes.size() << "\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
    return 0;
}
