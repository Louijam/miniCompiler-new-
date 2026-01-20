#pragma once
#include <unordered_map>
#include <vector>
#include <string>
#include <stdexcept>

#include "../ast/class.hpp"
#include "../ast/type.hpp"
#include "../sem/class_table.hpp"

namespace interp {

inline bool same_params(const std::vector<ast::Type>& a, const std::vector<ast::Type>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) if (a[i] != b[i]) return false;
    return true;
}

struct MethodTable {
    std::unordered_map<std::string, std::vector<ast::MethodDef*>> methods; // key: ClassName::method

    static std::string key(const std::string& cls, const std::string& name) {
        return cls + "::" + name;
    }

    void add(const std::string& cls, ast::MethodDef& m) {
        auto& vec = methods[key(cls, m.name)];
        std::vector<ast::Type> sig;
        sig.reserve(m.params.size());
        for (const auto& p : m.params) sig.push_back(p.type);

        for (auto* existing : vec) {
            std::vector<ast::Type> ex_sig;
            ex_sig.reserve(existing->params.size());
            for (const auto& p : existing->params) ex_sig.push_back(p.type);
            if (same_params(ex_sig, sig)) throw std::runtime_error("duplicate method overload: " + key(cls, m.name));
        }
        vec.push_back(&m);
    }

    ast::MethodDef& resolve_static(const sem::ClassTable& classes,
                                  const std::string& static_class,
                                  const std::string& method,
                                  const std::vector<ast::Type>& arg_types) {
        const sem::ClassSymbol* cur = &classes.get_class(static_class);

        ast::MethodDef* best = nullptr;

        while (cur) {
            auto it = methods.find(key(cur->name, method));
            if (it != methods.end()) {
                for (auto* cand : it->second) {
                    if (cand->params.size() != arg_types.size()) continue;

                    bool ok = true;
                    for (size_t i = 0; i < arg_types.size(); ++i) {
                        if (cand->params[i].type != arg_types[i]) { ok = false; break; }
                    }
                    if (ok) {
                        if (best) throw std::runtime_error("ambiguous overload: " + method);
                        best = cand;
                    }
                }
            }

            if (cur->base_name.empty()) break;
            cur = &classes.get_class(cur->base_name);
        }

        if (!best) throw std::runtime_error("no matching overload: " + method);
        return *best;
    }
};

} // namespace interp
