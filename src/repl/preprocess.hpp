#pragma once
#include <string>

// Entfernt alles ab einem # bis Zeilenende (wie Kommentar).
// Einfach, aber genau das was gefordert ist: "#include ..." ignorieren.
namespace repl {

inline std::string strip_preprocessor_lines(const std::string& src) {
    std::string out;
    out.reserve(src.size());

    bool at_line_start = true;
    for (size_t i = 0; i < src.size(); ++i) {
        char ch = src[i];

        if (at_line_start) {
            // Whitespace am Zeilenanfang ueberspringen, aber beibehalten
            size_t j = i;
            while (j < src.size() && (src[j] == ' ' || src[j] == '\t')) {
                out.push_back(src[j]);
                ++j;
            }
            if (j < src.size() && src[j] == '#') {
                // bis newline skippen
                while (j < src.size() && src[j] != '\n') ++j;
                if (j < src.size() && src[j] == '\n') out.push_back('\n');
                i = j;
                at_line_start = true;
                continue;
            }
            // kein # -> normal weiter
            i = j - 1;
            at_line_start = false;
            continue;
        }

        out.push_back(ch);
        if (ch == '\n') at_line_start = true;
    }

    return out;
}

} // namespace repl
