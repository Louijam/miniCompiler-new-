#pragma once
// Verhindert mehrfaches Einbinden dieser Header-Datei

#include <string>         // std::string
#include <unordered_map>  // std::unordered_map
#include <vector>         // std::vector
#include <stdexcept>      // std::runtime_error

#include "../ast/class.hpp" // AST: ClassDef/FieldDecl/MethodDef/ConstructorDef
#include "../ast/type.hpp"  // AST: Type

namespace sem {

// Symbol für eine Methode (nur Signatur/Meta, kein Body)
struct MethodSymbol {
    std::string name;              // Methodenname (ohne ClassPrefix)
    ast::Type return_type;         // Rückgabetyp
    std::vector<ast::Type> param_types; // Parametertypen (inkl. &)
    bool is_virtual = false;       // Virtual-Flag (wird ggf. durch Override propagiert)
};

// Symbol für einen Konstruktor (nur Parametertypen)
struct CtorSymbol {
    std::vector<ast::Type> param_types;
};

// Symboltabelle für eine Klasse (eigene Member; geerbte werden über Chain-Lookups gefunden)
struct ClassSymbol {
    std::string name;
    std::string base_name; // leer, wenn keine Basisklasse

    std::unordered_map<std::string, ast::Type> fields; // nur eigene Felder
    std::vector<CtorSymbol> ctors;                     // nur eigene Konstruktoren
    std::unordered_map<std::string, std::vector<MethodSymbol>> methods; // own methods: name -> overloads
};

// ClassTable: zentrale Semantik-Struktur für Klassenhierarchie, Felder/Methoden/Ctors
struct ClassTable {
    std::unordered_map<std::string, ClassSymbol> classes;

    // Signaturvergleich: gleiche Parametertypen inkl. Ref-Flag
    static bool same_params(const std::vector<ast::Type>& a, const std::vector<ast::Type>& b) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) if (a[i] != b[i]) return false;
        return true;
    }

    // Entfernt Referenz-Flag: T& -> T (für "base type" Matching)
    static ast::Type base_type(ast::Type t) {
        t.is_ref = false;
        return t;
    }

    // Phase 1: nur Klassennamen registrieren (damit forward references möglich sind)
    void add_class_name(const std::string& name) {
        if (classes.find(name) != classes.end())
            throw std::runtime_error("semantic error: class redefinition: " + name);

        ClassSymbol cs;
        cs.name = name;
        classes.emplace(name, std::move(cs));
    }

    bool has_class(const std::string& name) const {
        return classes.find(name) != classes.end();
    }

    // Lookup (mutable)
    ClassSymbol& get_class(const std::string& name) {
        auto it = classes.find(name);
        if (it == classes.end()) throw std::runtime_error("semantic error: unknown class: " + name);
        return it->second;
    }

    // Lookup (const)
    const ClassSymbol& get_class(const std::string& name) const {
        auto it = classes.find(name);
        if (it == classes.end()) throw std::runtime_error("semantic error: unknown class: " + name);
        return it->second;
    }

    // Prüft: derived == base oder derived erbt (transitiv) von base
    bool is_same_or_derived(const std::string& derived, const std::string& base) const {
        if (derived == base) return true;

        const ClassSymbol* cur = &get_class(derived);
        while (cur && !cur->base_name.empty()) {
            if (cur->base_name == base) return true;
            cur = &get_class(cur->base_name);
        }
        return false;
    }

    // Phase 2: Member einer Klasse aus AST übernehmen (Felder, Ctors, Methoden)
    void fill_class_members(const ast::ClassDef& c) {
        ClassSymbol& cs = get_class(c.name);
        cs.base_name = c.base_name;

        // Felder: keine Redefinition innerhalb der Klasse
        for (const auto& f : c.fields) {
            if (cs.fields.find(f.name) != cs.fields.end())
                throw std::runtime_error("semantic error: field redefinition in class " + c.name + ": " + f.name);
            cs.fields.emplace(f.name, f.type);
        }

        // Konstruktoren: overloads nur nach Parametern unterscheiden
        cs.ctors.clear();
        for (const auto& ctor : c.ctors) {
            CtorSymbol sym;
            sym.param_types.reserve(ctor.params.size());
            for (const auto& p : ctor.params) sym.param_types.push_back(p.type);

            for (const auto& existing : cs.ctors) {
                if (same_params(existing.param_types, sym.param_types)) {
                    throw std::runtime_error("semantic error: constructor overload redefinition in class " + c.name);
                }
            }
            cs.ctors.push_back(std::move(sym));
        }

        // Wenn keine Ctors angegeben: Default-Ctor synthetisieren (leere Paramliste)
        if (cs.ctors.empty()) {
            CtorSymbol def;
            cs.ctors.push_back(std::move(def));
        }

        // Methoden: overloads nach Parametern; Virtual-Flag speichern
        for (const auto& m : c.methods) {
            MethodSymbol ms;
            ms.name = m.name;
            ms.return_type = m.return_type;
            ms.is_virtual = m.is_virtual;

            ms.param_types.reserve(m.params.size());
            for (const auto& p : m.params) ms.param_types.push_back(p.type);

            auto& vec = cs.methods[ms.name];
            for (const auto& existing : vec) {
                if (same_params(existing.param_types, ms.param_types)) {
                    throw std::runtime_error("semantic error: method overload redefinition in class " + c.name + ": " + m.name);
                }
            }
            vec.push_back(ms);
        }
    }

    // Validiert Vererbung:
    // - Basisklassen existieren
    // - keine Zyklen
    // - Basisklasse besitzt Default-Ctor (wie hier gefordert/angenommen)
    void check_inheritance() const {
        // Base muss existieren
        for (const auto& [name, cs] : classes) {
            if (!cs.base_name.empty() && !has_class(cs.base_name)) {
                throw std::runtime_error("semantic error: unknown base class of " + name + ": " + cs.base_name);
            }
        }

        // Cycle Check via DFS Marking
        enum class Mark { None, Temp, Perm };
        std::unordered_map<std::string, Mark> mark;
        for (const auto& [name, _] : classes) mark[name] = Mark::None;

        auto dfs = [&](auto&& self, const std::string& n) -> void {
            auto& m = mark[n];
            if (m == Mark::Temp) throw std::runtime_error("semantic error: inheritance cycle involving: " + n);
            if (m == Mark::Perm) return;

            m = Mark::Temp;

            const auto& cs = get_class(n);
            if (!cs.base_name.empty()) self(self, cs.base_name);

            m = Mark::Perm;
        };

        for (const auto& [name, _] : classes) dfs(dfs, name);

        // Basisklasse muss einen Default-Ctor haben (Paramliste leer)
        for (const auto& [name, cs] : classes) {
            if (cs.base_name.empty()) continue;
            const auto& base = get_class(cs.base_name);

            bool has_default = false;
            for (const auto& ctor : base.ctors) {
                if (ctor.param_types.empty()) { has_default = true; break; }
            }
            if (!has_default) {
                throw std::runtime_error("semantic error: base class has no default constructor: " + cs.base_name);
            }
        }
    }

    // Sucht eine exakt passende Signatur (name + param_types) in class_name und dessen Basiskette
    const MethodSymbol* find_exact_in_chain(const std::string& class_name, const MethodSymbol& wanted) const {
        const ClassSymbol* cur = &get_class(class_name);

        while (cur) {
            auto it = cur->methods.find(wanted.name);
            if (it != cur->methods.end()) {
                for (const auto& cand : it->second) {
                    if (same_params(cand.param_types, wanted.param_types)) return &cand;
                }
            }
            if (cur->base_name.empty()) break;
            cur = &get_class(cur->base_name);
        }

        return nullptr;
    }

    // Prüft Overrides:
    // - Return-Type muss matchen
    // - Virtual-ness wird nach C++-Regel propagiert (override eines virtual bleibt virtual)
    // Achtung: non-const, weil is_virtual in der abgeleiteten Methode ggf. gesetzt wird.
    void check_overrides_and_virtuals() {
        for (auto& [name, cs] : classes) {
            if (cs.base_name.empty()) continue;

            for (auto& [mname, overloads] : cs.methods) {
                for (auto& dm : overloads) {
                    const MethodSymbol* bm = find_exact_in_chain(cs.base_name, dm);
                    if (!bm) continue;

                    // Rückgabetyp muss gleich sein
                    if (bm->return_type != dm.return_type) {
                        throw std::runtime_error(
                            "semantic error: override return type mismatch in class " + name + " for method " + mname
                        );
                    }

                    // C++: override eines virtual ist automatisch virtual
                    if (bm->is_virtual) {
                        dm.is_virtual = true;
                    }
                }
            }
        }
    }

    // Feld existiert irgendwo in der Vererbungskette?
    bool has_field_in_chain(const std::string& class_name, const std::string& field) const {
        const ClassSymbol* cur = &get_class(class_name);
        while (cur) {
            if (cur->fields.find(field) != cur->fields.end()) return true;
            if (cur->base_name.empty()) break;
            cur = &get_class(cur->base_name);
        }
        return false;
    }

    // Feldtyp in Kette finden oder Fehler werfen
    ast::Type field_type_in_chain(const std::string& class_name, const std::string& field) const {
        const ClassSymbol* cur = &get_class(class_name);
        while (cur) {
            auto it = cur->fields.find(field);
            if (it != cur->fields.end()) return it->second;
            if (cur->base_name.empty()) break;
            cur = &get_class(cur->base_name);
        }
        throw std::runtime_error("semantic error: unknown field: " + class_name + "." + field);
    }

    // Liefert alle Felder der Kette (derived gewinnt bei gleichen Namen)
    // (Aus Sicht des Slicing/Runtime-Layouts: derived überschreibt base-Feldnamen)
    std::unordered_map<std::string, ast::Type> merged_fields_derived_wins(const std::string& class_name) const {
        std::unordered_map<std::string, ast::Type> out;

        const ClassSymbol* cur = &get_class(class_name);
        while (cur) {
            for (const auto& [fname, ftype] : cur->fields) {
                if (out.find(fname) == out.end()) out.emplace(fname, ftype);
            }
            if (cur->base_name.empty()) break;
            cur = &get_class(cur->base_name);
        }

        return out;
    }

    // Overload-Resolution für Methodenaufruf am *statischen* Typ:
    // - base type muss exakt matchen
    // - Ref-Parameter verlangen lvalue-Args
    // - Score: +1 pro gebundenem Ref-Param (tie-breaker)
    const MethodSymbol& resolve_method_call(const std::string& static_class,
                                           const std::string& method,
                                           const std::vector<ast::Type>& arg_base_types,
                                           const std::vector<bool>& arg_is_lvalue) const {
        const MethodSymbol* best = nullptr;
        int best_score = -1;

        const ClassSymbol* cur = &get_class(static_class);
        while (cur) {
            auto it = cur->methods.find(method);
            if (it != cur->methods.end()) {
                for (const auto& cand : it->second) {
                    if (cand.param_types.size() != arg_base_types.size()) continue;

                    bool ok = true;
                    int score = 0;

                    for (size_t i = 0; i < arg_base_types.size(); ++i) {
                        ast::Type par = cand.param_types[i];
                        ast::Type par_base = base_type(par);

                        // Param base type muss exakt passen
                        if (par_base != arg_base_types[i]) { ok = false; break; }

                        // Ref-Param braucht lvalue-Arg
                        if (par.is_ref) {
                            if (!arg_is_lvalue[i]) { ok = false; break; }
                            score += 1;
                        }
                    }

                    if (!ok) continue;

                    if (score > best_score) {
                        best_score = score;
                        best = &cand;
                    } else if (score == best_score) {
                        throw std::runtime_error("semantic error: ambiguous overload: " + method);
                    }
                }
            }

            if (cur->base_name.empty()) break;
            cur = &get_class(cur->base_name);
        }

        if (!best) throw std::runtime_error("semantic error: no matching overload: " + method);
        return *best;
    }

    // Overload-Resolution für Konstruktor-Aufruf innerhalb einer Klasse:
    // - base type exakt
    // - Ref-Params brauchen lvalue
    // - Score: +1 pro ref param
    const CtorSymbol& resolve_ctor_call(const std::string& class_name,
                                       const std::vector<ast::Type>& arg_base_types,
                                       const std::vector<bool>& arg_is_lvalue) const {
        const auto& cs = get_class(class_name);

        const CtorSymbol* best = nullptr;
        int best_score = -1;

        for (const auto& cand : cs.ctors) {
            if (cand.param_types.size() != arg_base_types.size()) continue;

            bool ok = true;
            int score = 0;

            for (size_t i = 0; i < arg_base_types.size(); ++i) {
                ast::Type par = cand.param_types[i];
                ast::Type par_base = base_type(par);

                if (par_base != arg_base_types[i]) { ok = false; break; }

                if (par.is_ref) {
                    if (!arg_is_lvalue[i]) { ok = false; break; }
                    score += 1;
                }
            }

            if (!ok) continue;

            if (score > best_score) {
                best_score = score;
                best = &cand;
            } else if (score == best_score) {
                throw std::runtime_error("semantic error: ambiguous constructor call: " + class_name);
            }
        }

        if (!best) throw std::runtime_error("semantic error: no matching constructor: " + class_name);
        return *best;
    }
};

} // namespace sem
