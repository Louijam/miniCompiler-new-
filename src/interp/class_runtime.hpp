#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <stdexcept>
#include <algorithm>

#include "../ast/program.hpp"
#include "../ast/type.hpp"
#include "../ast/class.hpp"

namespace interp {

struct ClassRuntime {
    struct ClassInfo {
        std::string name;
        std::string base;

        // key: m(T1,T2,...) where Ti includes & if ref
        std::unordered_map<std::string, std::string> vtable_owner;   // key -> class that provides impl
        std::unordered_map<std::string, bool>        vtable_virtual; // key -> virtual?
    };

    std::unordered_map<std::string, ClassInfo> classes;

    static std::string sig_key(const std::string& mname, const std::vector<ast::Type>& param_types) {
        std::string k = mname;
        k += "(";
        for (size_t i = 0; i < param_types.size(); ++i) {
            if (i) k += ",";
            k += ast::to_string(param_types[i]);
        }
        k += ")";
        return k;
    }

    void build_from_program(const ast::Program& p) {
        classes.clear();

        // names
        for (const auto& c : p.classes) {
            ClassInfo ci;
            ci.name = c.name;
            ci.base = c.base_name;
            classes.emplace(ci.name, std::move(ci));
        }

        // vtables: for each class, build chain base->derived
        for (const auto& c : p.classes) {
            auto& ci = classes.at(c.name);

            std::vector<const ast::ClassDef*> chain;
            const ast::ClassDef* cur = nullptr;
            for (const auto& cd : p.classes) if (cd.name == c.name) { cur = &cd; break; }

            while (cur) {
                chain.push_back(cur);
                if (cur->base_name.empty()) break;
                const ast::ClassDef* next = nullptr;
                for (const auto& cd : p.classes) if (cd.name == cur->base_name) { next = &cd; break; }
                cur = next;
            }
            std::reverse(chain.begin(), chain.end());

            std::unordered_map<std::string, std::string> owner;
            std::unordered_map<std::string, bool> virt;

            // virtual flags (once virtual, always virtual)
            for (const auto* d : chain) {
                for (const auto& m : d->methods) {
                    std::vector<ast::Type> pts;
                    pts.reserve(m.params.size());
                    for (const auto& p2 : m.params) pts.push_back(p2.type);

                    std::string k = sig_key(m.name, pts);

                    if (virt.find(k) == virt.end()) virt[k] = m.is_virtual;
                    else if (m.is_virtual) virt[k] = true;
                }
            }

            // most derived owner
            for (const auto* d : chain) {
                for (const auto& m : d->methods) {
                    std::vector<ast::Type> pts;
                    pts.reserve(m.params.size());
                    for (const auto& p2 : m.params) pts.push_back(p2.type);

                    std::string k = sig_key(m.name, pts);
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

    std::string resolve_impl_class(const std::string& static_class,
                                  const std::string& dynamic_class,
                                  const std::string& method,
                                  const std::vector<ast::Type>& param_types,
                                  bool call_via_ref) const {
        const auto& st = get(static_class);
        std::string key = sig_key(method, param_types);

        auto itv = st.vtable_virtual.find(key);
        bool isvirt = (itv != st.vtable_virtual.end()) ? itv->second : false;

        if (!isvirt || !call_via_ref) {
            auto ito = st.vtable_owner.find(key);
            if (ito == st.vtable_owner.end())
                throw std::runtime_error("runtime error: unknown method: " + static_class + "." + method);
            return ito->second;
        }

        const auto& dyn = get(dynamic_class);
        auto ito = dyn.vtable_owner.find(key);
        if (ito == dyn.vtable_owner.end())
            throw std::runtime_error("runtime error: unknown method: " + dynamic_class + "." + method);
        return ito->second;
    }
};

} // namespace interp
