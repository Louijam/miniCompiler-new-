#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include "repl/repl.hpp"
#include "repl/preprocess.hpp"

static std::string read_file_or_throw(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Konnte Datei nicht oeffnen: " + path);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

int main(int argc, char** argv) {
    try {
        if (argc > 1) {
            std::string src = read_file_or_throw(argv[1]);
            std::string stripped = repl::strip_preprocessor_lines(src);

            // Spaeter: stripped -> parse -> sem -> exec
            std::cout << "TODO file parse+exec (nach preprocess). Laenge vorher/nachher: "
                      << src.size() << "/" << stripped.size() << "\n";
        }

        return repl::run_repl();
    } catch (const std::exception& ex) {
        std::cerr << "FEHLER: " << ex.what() << "\n";
        return 1;
    }
}
