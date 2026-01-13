#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <stdexcept>
#include <optional>

#include "../ast/class.hpp"
#include "../ast/type.hpp"

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

    std::unordered_map<std::string, ast::Type> fields; // own fields
    std::unordered_map<std::string, std::vector<MethodSymbol>> methods; // own methods
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

    bool has_class(const std::string& name) const {
        return classes.find(name) != classes.end();
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

    void fill_class_members(const ast::ClassDef& c) {
        ClassSymbol& cs = get_class(c.name);
        cs.base_name = c.base_name;

        for (const auto& f : c.fields) {
            if (cs.fields.find(f.name) != cs.fields.end())
                throw std::runtime_error("semantic error: field redefinition in class " + c.name + ": " + f.name);
            cs.fields.emplace(f.name, f.type);
        }

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

    void check_inheritance() const {
        for (const auto& [name, cs] : classes) {
            if (!cs.base_name.empty() && !has_class(cs.base_name)) {
                throw std::runtime_error("semantic error: unknown base class of " + name + ": " + cs.base_name);
            }
        }

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

    void check_overrides_and_virtuals() const {
        for (const auto& [name, cs] : classes) {
            if (cs.base_name.empty()) continue;

            for (const auto& [mname, overloads] : cs.methods) {
                for (const auto& dm : overloads) {
                    const MethodSymbol* bm = find_in_chain(cs.base_name, dm);
                    if (!bm) continue;

                    if (bm->return_type != dm.return_type) {
                        throw std::runtime_error("semantic error: override return type mismatch in class " + name + " for method " + mname);
                    }
                }
            }
        }
    }

    // ---- NEW: field lookup for method name resolution ----
    bool has_field_in_chain(const std::string& class_name, const std::string& field) const {
        const ClassSymbol* cur = &get_class(class_name);
        while (cur) {
            if (cur->fields.find(field) != cur->fields.end()) return true;
            if (cur->base_name.empty()) break;
            cur = &get_class(cur->base_name);
        }
        return false;
    }

    ast::Type field_type_in_chain(const std::string& class_name, const std::string& field) const {
        const ClassSymbol* cur = &get_class(class_name);
        while (cur) {
            auto it = cur->fields.find(field);
            if (it != cur->fields.end()) return it->second;
            if (cur->base_name.empty()) break;
            cur = &get_class(cur->base_name);
        }
        throw std::runtime_error("semantic error: unknown field in class chain: " + class_name + "." + field);
    }

    // collect fields into a single map where derived wins over base (for member-scope building)
    std::unordered_map<std::string, ast::Type> merged_fields_derived_wins(const std::string& class_name) const {
        std::unordered_map<std::string, ast::Type> out;
        const ClassSymbol* cur = &get_class(class_name);
        while (cur) {
            // insert only if not present yet (derived inserted first)
            for (const auto& [fname, ftype] : cur->fields) {
                if (out.find(fname) == out.end()) out.emplace(fname, ftype);
            }
            if (cur->base_name.empty()) break;
            cur = &get_class(cur->base_name);
        }
        return out;
    }
};

} // namespace sem
