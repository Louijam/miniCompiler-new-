#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <stdexcept>

#include "../ast/program.hpp"
#include "../ast/type.hpp"
#include "../ast/class.hpp"
#include "../ast/function.hpp"

namespace interp {

struct MethodEntry {
    ast::FunctionDef* f = nullptr;
    bool is_virtual = false;
    std::string defined_in; // class name where this impl lives
};

struct ClassRuntime {
    struct ClassInfo {
        std::string name;
        std::string base;
        std::unordered_map<std::string, ast::Type> merged_fields;
        std::unordered_map<std::string, std::vector<MethodEntry>> methods; // name -> overloads
        std::unordered_map<std::string, std::string> vtable_owner; // key(sig) -> class that provides impl
        std::unordered_map<std::string, bool> vtable_is_virtual;    // key(sig) -> virtual?
    };

    std::unordered_map<std::string, ClassInfo> classes;

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

    static bool same_params(const std::vector<ast::Param>& a, const std::vector<ast::Param>& b) {
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i) if (a[i].type != b[i].type) return false;
        return true;
    }

    void build_from_program(const ast::Program& p, std::vector<ast::FunctionDef>& method_funcs_out) {
        classes.clear();
        method_funcs_out.clear();

        // register names
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
                const auto& cur_ci = classes.at(cur);
                const ast::ClassDef* def = nullptr;
                for (const auto& cd : p.classes) if (cd.name == cur) { def = &cd; break; }
                if (!def) break;

                for (const auto& f : def->fields) {
                    if (merged.find(f.name) == merged.end()) merged.emplace(f.name, f.type);
                }
                cur = def->base_name;
            }
            ci.merged_fields = std::move(merged);
        }

        // flatten methods into FunctionDef pool with mangled name Class::m
        for (const auto& c : p.classes) {
            auto& ci = classes.at(c.name);

            for (const auto& m : c.methods) {
                ast::FunctionDef f;
                f.name = c.name + "::" + m.name;
                f.return_type = m.return_type;
                f.params = m.params;
                f.body = std::unique_ptr<ast::Stmt>(m.body ? m.body->clone() : nullptr); // if you dont have clone, we will override below
                method_funcs_out.push_back(std::move(f));
            }
        }

        // NOTE: no clone support -> store pointers to original method bodies instead via a wrapper later
        // For now we will instead build vtable based on signatures and class chain only.
        // functions table will still be filled manually in main until parser exists.

        // build vtable mapping: for each class, for each method sig in chain, choose most-derived impl
        for (const auto& c : p.classes) {
            auto& ci = classes.at(c.name);

            // collect all defs in chain base->derived order
            std::vector<const ast::ClassDef*> chain;
            const ast::ClassDef* cur_def = nullptr;
            for (const auto& cd : p.classes) if (cd.name == c.name) { cur_def = &cd; break; }
            while (cur_def) {
                chain.push_back(cur_def);
                if (cur_def->base_name.empty()) break;
                const ast::ClassDef* next = nullptr;
                for (const auto& cd : p.classes) if (cd.name == cur_def->base_name) { next = &cd; break; }
                cur_def = next;
            }
            std::reverse(chain.begin(), chain.end());

            // map sig -> (owner, is_virtual if base declares virtual)
            std::unordered_map<std::string, std::string> owner;
            std::unordered_map<std::string, bool> isvirt;

            // first pass: base virtual flags
            for (const auto* d : chain) {
                for (const auto& m : d->methods) {
                    std::string k = sig_key(m.name, m.params);
                    if (isvirt.find(k) == isvirt.end()) isvirt[k] = m.is_virtual;
                    else if (isvirt[k]) {
                        // stays virtual
                    } else if (m.is_virtual) {
                        isvirt[k] = true;
                    }
                }
            }

            // second pass: most derived owner for each signature
            for (const auto* d : chain) {
                for (const auto& m : d->methods) {
                    std::string k = sig_key(m.name, m.params);
                    owner[k] = d->name;
                    if (m.is_virtual) isvirt[k] = true;
                }
            }

            ci.vtable_owner = std::move(owner);
            ci.vtable_is_virtual = std::move(isvirt);
        }
    }

    const ClassInfo& get(const std::string& name) const {
        auto it = classes.find(name);
        if (it == classes.end()) throw std::runtime_error("runtime error: unknown class: " + name);
        return it->second;
    }

    // resolve method impl class given static type, dynamic type, method name, params, and whether call is via ref
    std::string resolve_impl_class(const std::string& static_class,
                                  const std::string& dynamic_class,
                                  const std::string& method,
                                  const std::vector<ast::Param>& params,
                                  bool call_via_ref) const {
        const auto& st = get(static_class);
        std::string key = sig_key(method, params);

        auto itv = st.vtable_is_virtual.find(key);
        bool virt = (itv != st.vtable_is_virtual.end()) ? itv->second : false;

        if (!virt || !call_via_ref) {
            // static dispatch
            auto ito = st.vtable_owner.find(key);
            if (ito == st.vtable_owner.end()) throw std::runtime_error("runtime error: unknown method: " + static_class + "." + method);
            return ito->second;
        }

        // dynamic dispatch using dynamic type
        const auto& dyn = get(dynamic_class);
        auto ito = dyn.vtable_owner.find(key);
        if (ito == dyn.vtable_owner.end()) throw std::runtime_error("runtime error: unknown method: " + dynamic_class + "." + method);
        return ito->second;
    }
};

} // namespace interp
