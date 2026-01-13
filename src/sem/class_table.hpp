#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <stdexcept>

#include "../ast/class.hpp"
#include "../ast/type.hpp"
#include "symbol.hpp"

namespace sem {

struct MethodSymbol {
    std::string name;
    ast::Type return_type;
    std::vector<ast::Type> param_types;
    bool is_virtual = false;
};

struct ClassSymbol {
    std::string name;
    std::string base_name; // empty if none

    std::unordered_map<std::string, ast::Type> fields; // only own fields (for now)
    std::unordered_map<std::string, std::vector<MethodSymbol>> methods; // overload set per name
};

struct ClassTable {
    std::unordered_map<std::string, ClassSymbol> classes;

    static bool same_params(const std::vector<ast::Type>& a, const std::vector<ast::Type>& b) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) if (a[i] != b[i]) return false;
        return true;
    }

    void add_class_name(const std::string& name) {
        if (classes.find(name) != classes.end())
            throw std::runtime_error("semantic error: class redefinition: " + name);
        ClassSymbol cs;
        cs.name = name;
        classes.emplace(name, std::move(cs));
    }

    ClassSymbol& get_class(const std::string& name) {
        auto it = classes.find(name);
        if (it == classes.end()) throw std::runtime_error("semantic error: unknown class: " + name);
        return it->second;
    }

    const ClassSymbol& get_class(const std::string& name) const {
        auto it = classes.find(name);
        if (it == classes.end()) throw std::runtime_error("semantic error: unknown class: " + name);
        return it->second;
    }

    bool has_class(const std::string& name) const {
        return classes.find(name) != classes.end();
    }

    // PASS 1: fill members + base links
    void fill_class_members(const ast::ClassDef& c) {
        ClassSymbol& cs = get_class(c.name);
        cs.base_name = c.base_name;

        // fields: unique within class
        for (const auto& f : c.fields) {
            if (cs.fields.find(f.name) != cs.fields.end())
                throw std::runtime_error("semantic error: field redefinition in class " + c.name + ": " + f.name);
            cs.fields.emplace(f.name, f.type);
        }

        // methods: overloads; no duplicate signature in same class
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

    // PASS 2: check base existence + cycles
    void check_inheritance() const {
        // base must exist
        for (const auto& [name, cs] : classes) {
            if (!cs.base_name.empty() && !has_class(cs.base_name)) {
                throw std::runtime_error("semantic error: unknown base class of " + name + ": " + cs.base_name);
            }
        }

        // cycle detection DFS
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
    }

    // helper: find a method in class chain (first match by exact signature)
    const MethodSymbol* find_in_chain(const std::string& class_name, const MethodSymbol& wanted) const {
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

    // PASS 3: override checks + virtual propagation rule
    void check_overrides_and_virtuals() const {
        for (const auto& [name, cs] : classes) {
            if (cs.base_name.empty()) continue;

            // for each method in derived, see if it matches something in base chain
            for (const auto& [mname, overloads] : cs.methods) {
                for (const auto& dm : overloads) {
                    const MethodSymbol* bm = find_in_chain(cs.base_name, dm);
                    if (!bm) continue; // not an override

                    // return type must match exactly for our subset
                    if (bm->return_type != dm.return_type) {
                        throw std::runtime_error("semantic error: override return type mismatch in class " + name + " for method " + mname);
                    }

                    // virtual propagation: if base is virtual, derived is effectively virtual
                    // (we do not mutate here; we just ensure consistency rules are ok)
                    // If derived says virtual but base not virtual: allow (C++ allows), but dispatch depends on static type anyway.
                }
            }
        }
    }
};

} // namespace sem
