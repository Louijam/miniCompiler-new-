#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <stdexcept>
#include <algorithm>

#include "../ast/program.hpp"
#include "../ast/type.hpp"
#include "../ast/class.hpp"
#include "../ast/function.hpp"

namespace interp {

struct MethodInfo {
    const ast::MethodDef* def = nullptr; // points into Program (stable after build)
    std::string owner_class;
    bool is_virtual = false;
};

struct CtorInfo {
    const ast::ConstructorDef* def = nullptr; // points into Program (stable after build)
    std::string owner_class;
};

struct ClassInfo {
    std::string name;
    std::string base;

    std::unordered_map<std::string, ast::Type> merged_fields; // derived wins
    std::vector<CtorInfo> ctors;

    std::unordered_map<std::string, std::vector<MethodInfo>> methods;
    std::unordered_map<std::string, std::string> vtable_owner;
    std::unordered_map<std::string, bool> vtable_virtual;
};

struct ClassRuntime {
    std::unordered_map<std::string, ClassInfo> classes;
    const ast::Program* prog = nullptr;

    static std::string sig_key(const std::string& mname, const std::vector<ast::Param>& params) {
        std::string k = mname;
        k += "(";
        for (size_t i = 0; i < params.size(); ++i) {
            if (i) k += ",";
            k += ast::to_string(params[i].type);
        }
        k += ")";
        return k;
    }

    static std::string ctor_key(const std::string& cname, const std::vector<ast::Param>& params) {
        std::string k = cname;
        k += "(";
        for (size_t i = 0; i < params.size(); ++i) {
            if (i) k += ",";
            k += ast::to_string(params[i].type);
        }
        k += ")";
        return k;
    }

    const ast::ClassDef* find_class_def(const std::string& name) const {
        if (!prog) return nullptr;
        for (const auto& c : prog->classes) if (c.name == name) return &c;
        return nullptr;
    }

    void build(const ast::Program& p) {
        prog = &p;
        classes.clear();

        for (const auto& c : p.classes) {
            ClassInfo ci;
            ci.name = c.name;
            ci.base = c.base_name;
            classes.emplace(ci.name, std::move(ci));
        }

        // merged fields (derived wins)
        for (const auto& c : p.classes) {
            auto& ci = classes.at(c.name);

            std::unordered_map<std::string, ast::Type> merged;
            std::string cur = c.name;
            while (!cur.empty()) {
                const ast::ClassDef* def = find_class_def(cur);
                if (!def) break;

                for (const auto& f : def->fields) {
                    if (merged.find(f.name) == merged.end()) merged.emplace(f.name, f.type);
                }
                cur = def->base_name;
            }
            ci.merged_fields = std::move(merged);
        }

        // ctors + methods
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

        // vtable_owner / vtable_virtual
        for (const auto& c : p.classes) {
            auto& ci = classes.at(c.name);

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

            for (const auto* d : chain) {
                for (const auto& m : d->methods) {
                    std::string k = sig_key(m.name, m.params);
                    if (virt.find(k) == virt.end()) virt[k] = m.is_virtual;
                    else if (!virt[k] && m.is_virtual) virt[k] = true;
                }
            }

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

    const ClassInfo& get(const std::string& name) const {
        auto it = classes.find(name);
        if (it == classes.end()) throw std::runtime_error("runtime error: unknown class: " + name);
        return it->second;
    }

    static ast::Type base_type(ast::Type t) { t.is_ref = false; return t; }

    // --- ctor overload resolution (only inside the same class) ---
    const ast::ConstructorDef& resolve_ctor(const std::string& class_name,
                                            const std::vector<ast::Type>& arg_types,
                                            const std::vector<bool>& arg_is_lvalue) const {
        const auto& ci = get(class_name);

        // If no ctors exist (should not happen after sema), synth default empty.
        if (ci.ctors.empty()) {
            static ast::ConstructorDef synth;
            return synth;
        }

        const ast::ConstructorDef* best = nullptr;
        int best_score = -1;

        for (const auto& cti : ci.ctors) {
            const auto& ctor = *cti.def;
            if (ctor.params.size() != arg_types.size()) continue;

            bool ok = true;
            int score = 0;

            for (size_t i = 0; i < arg_types.size(); ++i) {
                ast::Type at = base_type(arg_types[i]);
                ast::Type pt = ctor.params[i].type;

                if (base_type(pt) != at) { ok = false; break; }
                if (pt.is_ref) {
                    if (!arg_is_lvalue[i]) { ok = false; break; }
                    score += 1;
                }
            }

            if (!ok) continue;

            if (score > best_score) {
                best_score = score;
                best = &ctor;
            } else if (score == best_score) {
                throw std::runtime_error("runtime error: ambiguous constructor call: " + class_name);
            }
        }

        if (!best) throw std::runtime_error("runtime error: no matching constructor: " + class_name);
        return *best;
    }

    // --- method resolution (unchanged) ---
    const ast::MethodDef& pick_overload_in_class(const std::string& cls,
                                                 const std::string& method,
                                                 const std::vector<ast::Type>& arg_types,
                                                 const std::vector<bool>& arg_is_lvalue) const {
        const auto& ci = get(cls);

        auto it = ci.methods.find(method);
        if (it == ci.methods.end()) throw std::runtime_error("runtime error: no matching overload: " + method);

        const ast::MethodDef* best = nullptr;
        int best_score = -1;

        for (const auto& mi : it->second) {
            const auto& m = *mi.def;
            if (m.params.size() != arg_types.size()) continue;

            bool ok = true;
            int score = 0;

            for (size_t i = 0; i < arg_types.size(); ++i) {
                ast::Type at = base_type(arg_types[i]);
                ast::Type pt = m.params[i].type;
                if (base_type(pt) != at) { ok = false; break; }
                if (pt.is_ref) {
                    if (!arg_is_lvalue[i]) { ok = false; break; }
                    score += 1;
                }
            }

            if (!ok) continue;

            if (score > best_score) {
                best_score = score;
                best = &m;
            } else if (score == best_score) {
                throw std::runtime_error("runtime error: ambiguous overload: " + method);
            }
        }

        if (!best) throw std::runtime_error("runtime error: no matching overload: " + method);
        return *best;
    }

    std::string resolve_owner(const std::string& static_class,
                              const std::string& dynamic_class,
                              const ast::MethodDef& picked_sig,
                              bool call_via_ref) const {
        std::string key = sig_key(picked_sig.name, picked_sig.params);

        const auto& st = get(static_class);
        auto itv = st.vtable_virtual.find(key);
        bool virt = (itv != st.vtable_virtual.end()) ? itv->second : false;

        if (!virt || !call_via_ref) {
            auto ito = st.vtable_owner.find(key);
            if (ito == st.vtable_owner.end()) throw std::runtime_error("runtime error: unknown method: " + static_class + "." + picked_sig.name);
            return ito->second;
        }

        const auto& dyn = get(dynamic_class);
        auto ito = dyn.vtable_owner.find(key);
        if (ito == dyn.vtable_owner.end()) throw std::runtime_error("runtime error: unknown method: " + dynamic_class + "." + picked_sig.name);
        return ito->second;
    }

    const ast::MethodDef& resolve_method(const std::string& static_class,
                                         const std::string& dynamic_class,
                                         const std::string& method,
                                         const std::vector<ast::Type>& arg_types,
                                         const std::vector<bool>& arg_is_lvalue,
                                         bool call_via_ref) const {
        std::string cur = static_class;

        std::vector<std::string> chain;
        while (!cur.empty()) {
            chain.push_back(cur);
            const auto& ci = get(cur);
            if (ci.base.empty()) break;
            cur = ci.base;
        }

        const ast::MethodDef* picked = nullptr;

        for (const auto& c : chain) {
            try {
                const auto& m = pick_overload_in_class(c, method, arg_types, arg_is_lvalue);
                picked = &m;
                break;
            } catch (...) {
            }
        }
        if (!picked) throw std::runtime_error("runtime error: no matching overload: " + method);

        std::string owner = resolve_owner(static_class, dynamic_class, *picked, call_via_ref);

        const auto& owner_ci = get(owner);
        auto it = owner_ci.methods.find(method);
        if (it == owner_ci.methods.end()) throw std::runtime_error("runtime error: missing owner method: " + owner + "." + method);

        std::string target_key = sig_key(picked->name, picked->params);

        for (const auto& mi : it->second) {
            const auto& m = *mi.def;
            if (sig_key(m.name, m.params) == target_key) return m;
        }

        throw std::runtime_error("runtime error: missing override body: " + owner + "." + method);
    }
};

} // namespace interp
