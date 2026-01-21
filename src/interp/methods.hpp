#pragma once
// Verhindert mehrfaches Einbinden dieser Header-Datei

#include <unordered_map>   // std::unordered_map
#include <vector>          // std::vector
#include <string>          // std::string
#include <stdexcept>       // std::runtime_error

#include "../ast/class.hpp"        // AST-Methodendefinitionen
#include "../ast/type.hpp"         // Typrepräsentation
#include "../sem/class_table.hpp"  // Semantische Klassentabelle

namespace interp {

// Vergleicht zwei Parameterlisten exakt (Typen + Reihenfolge)
inline bool same_params(const std::vector<ast::Type>& a,
                        const std::vector<ast::Type>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (a[i] != b[i]) return false;
    return true;
}

// Tabelle fuer Methoden (klassenspezifische Overloads)
struct MethodTable {

    // Key: "ClassName::methodName" -> Liste von Overloads
    std::unordered_map<std::string, std::vector<ast::MethodDef*>> methods;

    // Erzeugt einen eindeutigen Key fuer Klasse + Methodenname
    static std::string key(const std::string& cls, const std::string& name) {
        return cls + "::" + name;
    }

    // Fuegt eine Methode hinzu und prueft auf doppelte Overloads
    void add(const std::string& cls, ast::MethodDef& m) {
        auto& vec = methods[key(cls, m.name)];

        // Signatur der neuen Methode extrahieren
        std::vector<ast::Type> sig;
        sig.reserve(m.params.size());
        for (const auto& p : m.params)
            sig.push_back(p.type);

        // Auf doppelte Signaturen pruefen
        for (auto* existing : vec) {
            std::vector<ast::Type> ex_sig;
            ex_sig.reserve(existing->params.size());
            for (const auto& p : existing->params)
                ex_sig.push_back(p.type);

            if (same_params(ex_sig, sig))
                throw std::runtime_error("duplicate method overload: " +
                                         key(cls, m.name));
        }

        vec.push_back(&m);
    }

    // Statische Methodenauflösung (nur anhand des statischen Typs)
    ast::MethodDef& resolve_static(const sem::ClassTable& classes,
                                   const std::string& static_class,
                                   const std::string& method,
                                   const std::vector<ast::Type>& arg_types) {
        const sem::ClassSymbol* cur = &classes.get_class(static_class);

        ast::MethodDef* best = nullptr;

        // Suche entlang der Vererbungskette
        while (cur) {
            auto it = methods.find(key(cur->name, method));
            if (it != methods.end()) {
                for (auto* cand : it->second) {
                    if (cand->params.size() != arg_types.size()) continue;

                    bool ok = true;
                    for (size_t i = 0; i < arg_types.size(); ++i) {
                        if (cand->params[i].type != arg_types[i]) {
                            ok = false;
                            break;
                        }
                    }

                    if (ok) {
                        if (best)
                            throw std::runtime_error("ambiguous overload: " +
                                                     method);
                        best = cand;
                    }
                }
            }

            if (cur->base_name.empty()) break;
            cur = &classes.get_class(cur->base_name);
        }

        if (!best)
            throw std::runtime_error("no matching overload: " + method);

        return *best;
    }
};

} // namespace interp
