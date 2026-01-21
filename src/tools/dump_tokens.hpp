#pragma once

#include <fstream>   // std::ifstream zum Lesen der Datei
#include <iostream>  // std::cout / std::cerr fuer Ausgabe
#include <iterator>  // std::istreambuf_iterator fuer komplettes Datei-Lesen
#include <string>    // std::string

#include "../lexer/lexer.hpp" // Lexer und Token-Struktur

namespace mini_cpp {

// Wird frueh im main() aufgerufen.
// Wenn --dump-tokens <datei> uebergeben wurde, werden nur Tokens ausgegeben
// und das Programm soll danach NICHT normal weiterlaufen.
inline bool maybe_dump_tokens(int argc, char** argv) {
    // Erwartetes CLI-Format:
    //   --dump-tokens <pfad>
    if (argc == 3 && std::string(argv[1]) == "--dump-tokens") {
        const std::string path = argv[2];

        // Datei oeffnen
        std::ifstream in(path);
        if (!in) {
            // Fehlerfall: Datei nicht lesbar
            std::cerr << "error: cannot open file: " << path << "\n";
            return true; // true = Programm soll danach abbrechen
        }

        // Ganze Datei in einen String einlesen
        std::string src(
            (std::istreambuf_iterator<char>(in)),
             std::istreambuf_iterator<char>()
        );

        // Lexer mit kompletter Quelldatei initialisieren
        lexer::Lexer lx(src);

        // Tokenisierung starten
        auto toks = lx.tokenize();

        // Alle Tokens mit Typ, Lexem und Position ausgeben
        for (const auto& t : toks) {
            std::cout
                << lexer::to_string(t.kind) << "  " // Token-Art
                << "'" << t.lexeme << "'" << "  "   // Original-Lexem
                << t.line << ":" << t.col << "\n";  // Position im Source
        }

        return true; // true = Dump erledigt, main soll NICHT weiterlaufen
    }

    // false = kein --dump-tokens, normales Programm ausfuehren
    return false;
}

} // namespace mini_cpp
