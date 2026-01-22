#pragma once
// Verhindert mehrfaches Einbinden dieser Header-Datei

#include <string>           // std::string
#include <unordered_map>   // Hashmaps fuer schnelle Namensauflösung
#include <vector>           // std::vector
#include <stdexcept>        // std::runtime_error
#include <algorithm>        // std::reverse

#include "../ast/program.hpp"   // AST-Wurzel (Program)
#include "../ast/type.hpp"      // Typrepräsentation
#include "../ast/class.hpp"     // Klassendefinitionen
#include "../ast/function.hpp"  // Funktionsdefinitionen

namespace interp {

// Metadaten zu einer Methode (zeigt direkt in den AST)
struct MethodInfo {
    const ast::MethodDef* def = nullptr; // Pointer in den AST (nach build stabil)
    std::string owner_class;             // Klasse, in der die Methode definiert ist
    bool is_virtual = false;             // Virtual-Flag der Methode
};

// Metadaten zu einem Konstruktor
struct CtorInfo {
    const ast::ConstructorDef* def = nullptr; // Pointer in den AST
    std::string owner_class;                  // Klasse, zu der der Konstruktor gehört
};

// Laufzeit-Informationen zu einer Klasse
struct ClassInfo {
    std::string name;                         // Klassenname
    std::string base;                         // Basisklasse (leer => keine)

    // Alle sichtbaren Felder inkl. Vererbung (abgeleitete Felder überschreiben Basisfelder)
    std::unordered_map<std::string, ast::Type> merged_fields;

    std::vector<CtorInfo> ctors;              // Alle Konstruktoren der Klasse

    // Methodenname -> Liste aller Overloads
    std::unordered_map<std::string, std::vector<MethodInfo>> methods;

    // VTable: Signatur -> Klasse, die die Implementierung besitzt
    std::unordered_map<std::string, std::string> vtable_owner;

    // VTable: Signatur -> ob Methode virtuell ist
    std::unordered_map<std::string, bool> vtable_virtual;
};

// Zentrale Runtime-Struktur fuer Klassen
struct ClassRuntime {
    std::unordered_map<std::string, ClassInfo> classes; // Alle Klasseninfos
    const ast::Program* prog = nullptr;                 // Referenz auf AST-Programm

    // Erzeugt einen eindeutigen Schlüssel fuer Methoden-Signaturen
    static std::string sig_key(const std::string& mname,
                               const std::vector<ast::Param>& params) {
        std::string k = mname;
        k += "(";
        for (size_t i = 0; i < params.size(); ++i) {
            if (i) k += ",";
            k += ast::to_string(params[i].type);
        }
        k += ")";
        return k;
    }

    // Erzeugt einen Schlüssel fuer Konstruktor-Signaturen
    static std::string ctor_key(const std::string& cname,
                                const std::vector<ast::Param>& params) {
        std::string k = cname;
        k += "(";
        for (size_t i = 0; i < params.size(); ++i) {
            if (i) k += ",";
            k += ast::to_string(params[i].type);
        }
        k += ")";
        return k;
    }

    // Sucht eine Klassendefinition im AST
    const ast::ClassDef* find_class_def(const std::string& name) const {
        if (!prog) return nullptr;
        for (const auto& c : prog->classes)
            if (c.name == name) return &c;
        return nullptr;
    }

    // Baut alle Runtime-Strukturen aus dem AST auf
    void build(const ast::Program& p) {
        prog = &p;
        classes.clear();

        // Leere ClassInfo-Strukturen anlegen
        for (const auto& c : p.classes) {
            ClassInfo ci;
            ci.name = c.name;
            ci.base = c.base_name;
            classes.emplace(ci.name, std::move(ci));
        }

        // Felder inkl. Vererbung zusammenführen (derived gewinnt)
        for (const auto& c : p.classes) {
            auto& ci = classes.at(c.name);

            std::unordered_map<std::string, ast::Type> merged;
            std::string cur = c.name;

            while (!cur.empty()) {
                const ast::ClassDef* def = find_class_def(cur);
                if (!def) break;

                for (const auto& f : def->fields) {
                    if (merged.find(f.name) == merged.end())
                        merged.emplace(f.name, f.type);
                }
                cur = def->base_name;
            }
            ci.merged_fields = std::move(merged);
        }

        // Konstruktoren und Methoden sammeln
        for (const auto& c : p.classes) {
            auto& ci = classes.at(c.name);

            ci.ctors.clear();
            for (const auto& ctor : c.ctors) {
                CtorInfo ci2;
                ci2.def = &ctor;
                ci2.owner_class = c.name;
                ci.ctors.push_back(ci2);
            }

            for (const auto& m : c.methods) {
                MethodInfo mi;
                mi.def = &m;
                mi.owner_class = c.name;
                mi.is_virtual = m.is_virtual;
                ci.methods[m.name].push_back(mi);
            }
        }

        // Aufbau von VTable-Informationen
        for (const auto& c : p.classes) {
            auto& ci = classes.at(c.name);

            // Vererbungskette (Base -> Derived)
            std::vector<const ast::ClassDef*> chain;
            const ast::ClassDef* cur = find_class_def(c.name);
            while (cur) {
                chain.push_back(cur);
                if (cur->base_name.empty()) break;
                cur = find_class_def(cur->base_name);
            }
            std::reverse(chain.begin(), chain.end());

            std::unordered_map<std::string, bool> virt;
            std::unordered_map<std::string, std::string> owner;

            // Virtual-Flags bestimmen
            for (const auto* d : chain) {
                for (const auto& m : d->methods) {
                    std::string k = sig_key(m.name, m.params);
                    if (virt.find(k) == virt.end())
                        virt[k] = m.is_virtual;
                    else if (!virt[k] && m.is_virtual)
                        virt[k] = true;
                }
            }

            // Owner der Methoden bestimmen
            for (const auto* d : chain) {
                for (const auto& m : d->methods) {
                    std::string k = sig_key(m.name, m.params);
                    owner[k] = d->name;
                    if (m.is_virtual) virt[k] = true;
                }
            }

            ci.vtable_owner = std::move(owner);
            ci.vtable_virtual = std::move(virt);
        }
    }

    // Liefert Runtime-Infos einer Klasse oder wirft Fehler
    const ClassInfo& get(const std::string& name) const {
        auto it = classes.find(name);
        if (it == classes.end())
            throw std::runtime_error("runtime error: unknown class: " + name);
        return it->second;
    }

    // Entfernt Referenzinformation aus einem Typ
    static ast::Type base_type(ast::Type t) {
        t.is_ref = false;
        return t;
    }

    // --- Konstruktor-Overload-Auflösung ---
    const ast::ConstructorDef& resolve_ctor(const std::string& class_name,
                                            const std::vector<ast::Type>& arg_types,
                                            const std::vector<bool>& arg_is_lvalue) const {
        const auto& ci = get(class_name);

        // Falls keine Konstruktoren existieren: synthetischer Default
        if (ci.ctors.empty()) {
            static ast::ConstructorDef synth;
            return synth;
        }

        const ast::ConstructorDef* best = nullptr;

        for (const auto& cti : ci.ctors) {
            const auto& ctor = *cti.def;
            if (ctor.params.size() != arg_types.size()) continue;

            bool ok = true;

            for (size_t i = 0; i < arg_types.size(); ++i) {
                ast::Type at = base_type(arg_types[i]);
                ast::Type pt = ctor.params[i].type;

                if (base_type(pt) != at) { ok = false; break; }
                if (pt.is_ref) {
                    if (!arg_is_lvalue[i]) { ok = false; break; }
                }
            }

            if (!ok) continue;

            if (!best) best = &ctor;
            else throw std::runtime_error("runtime error: ambiguous constructor call: " + class_name);
        }

        if (!best)
            throw std::runtime_error("runtime error: no matching constructor: " + class_name);
        return *best;
    }

    // --- Methodenauflösung ---
    const ast::MethodDef& pick_overload_in_class(const std::string& cls,
                                                 const std::string& method,
                                                 const std::vector<ast::Type>& arg_types,
                                                 const std::vector<bool>& arg_is_lvalue) const {
        const auto& ci = get(cls);

        auto it = ci.methods.find(method);
        if (it == ci.methods.end())
            throw std::runtime_error("runtime error: no matching overload: " + method);

        const ast::MethodDef* best = nullptr;

        for (const auto& mi : it->second) {
            const auto& m = *mi.def;
            if (m.params.size() != arg_types.size()) continue;

            bool ok = true;

            for (size_t i = 0; i < arg_types.size(); ++i) {
                ast::Type at = base_type(arg_types[i]);
                ast::Type pt = m.params[i].type;

                if (base_type(pt) != at) { ok = false; break; }
                if (pt.is_ref) {
                    if (!arg_is_lvalue[i]) { ok = false; break; }
                }
            }

            if (!ok) continue;

            if (!best) best = &m;
            else throw std::runtime_error("runtime error: ambiguous overload: " + method);
        }

        if (!best)
            throw std::runtime_error("runtime error: no matching overload: " + method);
        return *best;
    }

    // Bestimmt die Klasse, aus der die Methode effektiv aufgerufen wird
    std::string resolve_owner(const std::string& static_class,
                              const std::string& dynamic_class,
                              const ast::MethodDef& picked_sig,
                              bool call_via_ref) const {
        std::string key = sig_key(picked_sig.name, picked_sig.params);

        const auto& st = get(static_class);
        auto itv = st.vtable_virtual.find(key);
        bool virt = (itv != st.vtable_virtual.end()) ? itv->second : false;

        // Nicht virtuell oder Aufruf nicht ueber Referenz
        if (!virt || !call_via_ref) {
            auto ito = st.vtable_owner.find(key);
            if (ito == st.vtable_owner.end())
                throw std::runtime_error("runtime error: unknown method: " +
                                         static_class + "." + picked_sig.name);
            return ito->second;
        }

        // Virtueller Aufruf: dynamischer Typ entscheidet
        const auto& dyn = get(dynamic_class);
        auto ito = dyn.vtable_owner.find(key);
        if (ito == dyn.vtable_owner.end())
            throw std::runtime_error("runtime error: unknown method: " +
                                     dynamic_class + "." + picked_sig.name);
        return ito->second;
    }

    // Vollständige Methodenauflösung (Overload + Virtual Dispatch)
    const ast::MethodDef& resolve_method(const std::string& static_class,
                                         const std::string& dynamic_class,
                                         const std::string& method,
                                         const std::vector<ast::Type>& arg_types,
                                         const std::vector<bool>& arg_is_lvalue,
                                         bool call_via_ref) const {
        std::string cur = static_class;
        std::vector<std::string> chain;

        // Vererbungskette sammeln
        while (!cur.empty()) {
            chain.push_back(cur);
            const auto& ci = get(cur);
            if (ci.base.empty()) break;
            cur = ci.base;
        }

        const ast::MethodDef* picked = nullptr;

        // Overload in Kette suchen
        for (const auto& c : chain) {
            try {
                picked = &pick_overload_in_class(c, method, arg_types, arg_is_lvalue);
                break;
            } catch (...) {
            }
        }

        if (!picked)
            throw std::runtime_error("runtime error: no matching overload: " + method);

        // Effektiven Owner bestimmen
        std::string owner = resolve_owner(static_class, dynamic_class,
                                          *picked, call_via_ref);

        const auto& owner_ci = get(owner);
        auto it = owner_ci.methods.find(method);
        if (it == owner_ci.methods.end())
            throw std::runtime_error("runtime error: missing owner method: " +
                                     owner + "." + method);

        std::string target_key = sig_key(picked->name, picked->params);

        // Konkrete Implementierung finden
        for (const auto& mi : it->second) {
            const auto& m = *mi.def;
            if (sig_key(m.name, m.params) == target_key)
                return m;
        }

        throw std::runtime_error("runtime error: missing override body: " +
                                 owner + "." + method);
    }
};

} // namespace interp
