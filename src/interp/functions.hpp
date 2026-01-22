#pragma once
// Verhindert mehrfaches Einbinden dieser Header-Datei

#include <unordered_map>   // std::unordered_map
#include <vector>          // std::vector
#include <string>          // std::string
#include <stdexcept>       // std::runtime_error

#include "../ast/program.hpp"   // AST-Wurzel (Program)
#include "../ast/function.hpp"  // Funktionsdefinitionen
#include "../ast/type.hpp"      // Typrepräsentation
#include "class_runtime.hpp"    // Laufzeitinformationen fuer Klassen

namespace interp {

// Prüft, ob zwei Funktionsdefinitionen exakt die gleiche Signatur haben
inline bool same_signature(const ast::FunctionDef& a, const ast::FunctionDef& b) {
    if (a.name != b.name) return false;                 // Funktionsname
    if (a.params.size() != b.params.size()) return false; // Anzahl Parameter
    for (size_t i = 0; i < a.params.size(); ++i) {
        if (a.params[i].type != b.params[i].type) return false; // Parametertypen
    }
    return true;
}

// Hilfsfunktion: entfernt Referenzinformation aus einem Typ
inline ast::Type base_type(ast::Type t) {
    t.is_ref = false;
    return t;
}

// Zentrale Runtime-Struktur fuer globale Funktionen und Klassen
struct FunctionTable {
    // Funktionsname -> Liste aller Overloads
    std::unordered_map<std::string, std::vector<ast::FunctionDef*>> functions;

    // Laufzeitinformationen fuer Klassen (Felder, VTables, Vererbung)
    ClassRuntime class_rt;

    // Setzt alle Runtime-Daten zurueck
    void clear() {
        functions.clear();
        class_rt.classes.clear();
        class_rt.prog = nullptr;
    }

    // Fuegt eine Funktionsdefinition hinzu (inkl. Duplikat-Check)
    void add(ast::FunctionDef& f) {
        auto& vec = functions[f.name];
        for (auto* existing : vec) {
            if (same_signature(*existing, f)) {
                throw std::runtime_error("duplicate function overload: " + f.name);
            }
        }
        vec.push_back(&f);
    }

    // Initialisiert die FunctionTable aus einem kompletten Programm
    void add_program(ast::Program& p) {
        clear();
        for (auto& f : p.functions) add(f);
        class_rt.build(p);
    }

    // Overload-Auflösung fuer freie Funktionen (Projekt-Semantik):
    // - exakte Übereinstimmung der Basistypen
    // - Referenzparameter erfordern LValue-Argumente
    // - KEIN "best match" wie in echtem C++ (keine Präferenzregeln)
    // - wenn mehrere Overloads passen => Fehler (ambiguous)
    ast::FunctionDef& resolve(const std::string& name,
                              const std::vector<ast::Type>& arg_base_types,
                              const std::vector<bool>& arg_is_lvalue) {
        auto it = functions.find(name);
        if (it == functions.end()) {
            throw std::runtime_error("unknown function: " + name);
        }

        ast::FunctionDef* best = nullptr;

        for (auto* f : it->second) {
            if (f->params.size() != arg_base_types.size()) continue;

            bool ok = true;

            for (size_t i = 0; i < arg_base_types.size(); ++i) {
                ast::Type at = base_type(arg_base_types[i]);
                ast::Type pt = f->params[i].type;

                // Basistyp muss exakt passen
                if (base_type(pt) != at) { ok = false; break; }

                // Referenzparameter: Argument muss LValue sein
                if (pt.is_ref) {
                    if (!arg_is_lvalue[i]) { ok = false; break; }
                }
            }

            if (!ok) continue;

            if (!best) {
                best = f;
            } else {
                throw std::runtime_error("ambiguous overload: " + name);
            }
        }

        if (!best) {
            throw std::runtime_error("no matching overload: " + name);
        }
        return *best;
    }
};

} // namespace interp
