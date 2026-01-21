#pragma once
// Verhindert mehrfaches Einbinden dieser Header-Datei

#include <string>         // std::string
#include <vector>         // std::vector
#include <unordered_map>  // std::unordered_map
#include <stdexcept>      // std::runtime_error

#include "symbol.hpp"     // VarSymbol, FuncSymbol, same_signature (und ast::Type via symbol.hpp)

namespace sem {

// Scope = verschachtelte Symboltabelle (Lexical Scoping):
// - Variablen: Name -> VarSymbol
// - Funktionen: Name -> Liste von Overloads (FuncSymbol)
// parent zeigt auf den umgebenden Scope (oder nullptr für global)
struct Scope {
    Scope* parent = nullptr;

    // Variablen im aktuellen Scope
    std::unordered_map<std::string, VarSymbol> vars;

    // Funktions-Overloads im aktuellen Scope (Name -> mehrere Signaturen)
    std::unordered_map<std::string, std::vector<FuncSymbol>> funcs;

    explicit Scope(Scope* p = nullptr) : parent(p) {}

    // -------- Variables --------

    // Definiert eine Variable im aktuellen Scope.
    // Fehler, wenn derselbe Name im selben Scope bereits existiert.
    void define_var(const std::string& name, const ast::Type& type) {
        if (vars.find(name) != vars.end())
            throw std::runtime_error("semantic error: variable redefinition: " + name);
        vars.emplace(name, VarSymbol{name, type});
    }

    // Sucht eine Variable in der Scope-Kette (aktueller Scope -> parent -> ...).
    // Fehler, wenn nicht gefunden.
    const VarSymbol& lookup_var(const std::string& name) const {
        auto it = vars.find(name);
        if (it != vars.end()) return it->second;
        if (parent) return parent->lookup_var(name);
        throw std::runtime_error("semantic error: unknown variable: " + name);
    }

    // Prüft nur den aktuellen Scope (ohne parent).
    bool has_var_local(const std::string& name) const {
        return vars.find(name) != vars.end();
    }

    // -------- Functions (Overloads) --------

    // Definiert eine Funktion/Overload im aktuellen Scope.
    // Fehler, wenn eine identische Signatur (Name + Parametertypen) bereits existiert.
    void define_func(const FuncSymbol& f) {
        auto& vec = funcs[f.name];
        for (const auto& existing : vec) {
            if (same_signature(existing, f))
                throw std::runtime_error("semantic error: function overload redefinition: " + f.name);
        }
        vec.push_back(f);
    }

    // Overload-Resolution für Funktionsaufrufe:
    // - sucht zuerst in diesem Scope, sonst in parents
    // - matcht *exakt* die Parametertypen (keine Konversionen)
    // - wenn mehrere exakt passen -> ambiguous
    const FuncSymbol& resolve_func(const std::string& name, const std::vector<ast::Type>& arg_types) const {
        const Scope* s = this;

        while (s) {
            auto it = s->funcs.find(name);
            if (it != s->funcs.end()) {
                const auto& vec = it->second;

                const FuncSymbol* match = nullptr;

                for (const auto& cand : vec) {
                    if (cand.param_types.size() != arg_types.size()) continue;

                    bool ok = true;
                    for (size_t i = 0; i < arg_types.size(); ++i) {
                        if (cand.param_types[i] != arg_types[i]) { ok = false; break; }
                    }

                    if (ok) {
                        if (match) throw std::runtime_error("semantic error: ambiguous overload: " + name);
                        match = &cand;
                    }
                }

                if (!match) throw std::runtime_error("semantic error: no matching overload: " + name);
                return *match;
            }

            s = s->parent;
        }

        throw std::runtime_error("semantic error: unknown function: " + name);
    }

    // Prüft, ob im aktuellen Scope irgendein Funktionsname existiert (ohne parent).
    bool has_func_local(const std::string& name) const {
        return funcs.find(name) != funcs.end();
    }
};

} // namespace sem
