#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <stdexcept>

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

    static ast::Type base_type(ast::Type t) {
        t.is_ref = false;
        return t;
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

    const MethodSymbol* find_exact_in_chain(const std::string& class_name,
                                            const std::string& method_name,
                                            const std::vector<ast::Type>& param_types) const {
        const ClassSymbol* cur = &get_class(class_name);
        while (cur) {
            auto it = cur->methods.find(method_name);
            if (it != cur->methods.end()) {
                for (const auto& cand : it->second) {
                    if (same_params(cand.param_types, param_types)) return &cand;
                }
            }
            if (cur->base_name.empty()) break;
            cur = &get_class(cur->base_name);
        }
        return nullptr;
    }

    bool is_virtual_in_chain(const std::string& class_name,
                             const std::string& method_name,
                             const std::vector<ast::Type>& param_types) const {
        const MethodSymbol* m = find_exact_in_chain(class_name, method_name, param_types);
        return m ? m->is_virtual : false;
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

            // virtual inheritance + override return type check
            if (!cs.base_name.empty()) {
                const MethodSymbol* bm = find_exact_in_chain(cs.base_name, ms);
                if (bm) {
                    if (bm->return_type != ms.return_type) {
                        throw std::runtime_error("semantic error: override return type mismatch in class " + c.name + " for method " + m.name);
                    }
                    if (bm->is_virtual) ms.is_virtual = true;
                }
            }

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

    void check_overrides_and_virtuals() const {
        for (const auto& [name, cs] : classes) {
            if (cs.base_name.empty()) continue;

            for (const auto& [mname, overloads] : cs.methods) {
                for (const auto& dm : overloads) {
                    const MethodSymbol* bm = find_exact_in_chain(cs.base_name, dm);
                    if (!bm) continue;

                    if (bm->return_type != dm.return_type) {
                        throw std::runtime_error("semantic error: override return type mismatch in class " + name + " for method " + mname);
                    }
                }
            }
        }
    }

    // ---- fields in chain ----
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
        throw std::runtime_error("semantic error: unknown field: " + class_name + "." + field);
    }

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

    // ---- resolve method call on static class type (with overload rules) ----
    // Rules: exact base-type match, ref params require lvalue args, prefer ref overload
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
};

} // namespace sem
