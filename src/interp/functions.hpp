#pragma once
#include <unordered_map>
#include <vector>
#include <string>
#include <stdexcept>

#include "../ast/function.hpp"
#include "../ast/type.hpp"

namespace interp {

inline bool same_signature(const ast::FunctionDef& a, const ast::FunctionDef& b) {
    if (a.name != b.name) return false;
    if (a.params.size() != b.params.size()) return false;
    for (size_t i = 0; i < a.params.size(); ++i) {
        if (a.params[i].type != b.params[i].type) return false;
    }
    return true;
}

struct FunctionTable {
    std::unordered_map<std::string, std::vector<ast::FunctionDef*>> functions;

    void add(ast::FunctionDef& f) {
        auto& vec = functions[f.name];
        for (auto* existing : vec) {
            if (same_signature(*existing, f)) {
                throw std::runtime_error("duplicate function overload: " + f.name);
            }
        }
        vec.push_back(&f);
    }

    ast::FunctionDef& resolve(const std::string& name, const std::vector<ast::Type>& arg_types) {
        auto it = functions.find(name);
        if (it == functions.end()) {
            throw std::runtime_error("unknown function: " + name);
        }

        std::vector<ast::FunctionDef*> candidates;

        for (auto* f : it->second) {
            if (f->params.size() != arg_types.size()) continue;

            bool ok = true;
            for (size_t i = 0; i < arg_types.size(); ++i) {
                if (f->params[i].type != arg_types[i]) { ok = false; break; }
            }
            if (ok) candidates.push_back(f);
        }

        if (candidates.empty()) {
            throw std::runtime_error("no matching overload: " + name);
        }
        if (candidates.size() > 1) {
            throw std::runtime_error("ambiguous overload: " + name);
        }

        return *candidates[0];
    }
};

} // namespace interp
