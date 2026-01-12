#include <iostream>
#include <string>

int main(int argc, char** argv) {
    std::cout << "mini_cpp interpreter skeleton\n";
    if (argc > 1) {
        std::cout << "Would load file: " << argv[1] << "\n";
    }
    std::cout << "REPL not implemented yet\n";
    return 0;
}
