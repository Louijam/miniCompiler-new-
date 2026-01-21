#pragma once
// Verhindert mehrfaches Einbinden dieser Header-Datei

#include <string> // std::string

namespace repl {

// Entfernt Präprozessorzeilen, indem alles ab einem '#' bis zum Zeilenende entfernt wird.
// Zweck: "#include ..." und ähnliche Zeilen in Dateien ignorieren (wie gefordert).
inline std::string strip_preprocessor_lines(const std::string& src) {
    std::string out;
    out.reserve(src.size()); // Performance: grob gleiche Größe wie Input

    bool at_line_start = true; // true, wenn wir am Anfang einer neuen Zeile sind

    for (size_t i = 0; i < src.size(); ++i) {
        char ch = src[i];

        if (at_line_start) {
            // Whitespace am Zeilenanfang übernehmen (damit Format/Zeilenstruktur stabil bleibt)
            size_t j = i;
            while (j < src.size() && (src[j] == ' ' || src[j] == '\t')) {
                out.push_back(src[j]);
                ++j;
            }

            // Wenn nach optionalem Leading-Whitespace ein '#' kommt: komplette Zeile überspringen
            if (j < src.size() && src[j] == '#') {
                // Alles bis zum newline verwerfen
                while (j < src.size() && src[j] != '\n') ++j;

                // Newline behalten (damit Zeilennummern später weiter passen)
                if (j < src.size() && src[j] == '\n') out.push_back('\n');

                // Loop-Index auf das Ende der Zeile setzen
                i = j;
                at_line_start = true;
                continue;
            }

            // Kein Präprozessor: ab jetzt normale Verarbeitung dieser Zeile
            i = j - 1;            // -1, weil die for-Schleife gleich wieder ++i macht
            at_line_start = false;
            continue;
        }

        // Normales Kopieren innerhalb einer Zeile
        out.push_back(ch);

        // Zeilenwechsel erkennen
        if (ch == '\n') at_line_start = true;
    }

    return out;
}

} // namespace repl
