#pragma once
#include <unordered_map>
#include <vector>
#include <string>
#include <stdexcept>

#include "../ast/program.hpp"
#include "../ast/function.hpp"
#include "../ast/type.hpp"
#include "class_runtime.hpp"

namespace interp {

inline bool same_signature(const ast::FunctionDef& a, const ast::FunctionDef& b) {
    if (a.name != b.name) return false;
    if (a.params.size() != b.params.size()) return false;
    for (size_t i = 0; i < a.params.size(); ++i) {
        if (a.params[i].type != b.params[i].type) return false;
    }
    return true;
}

inline ast::Type base_type(ast::Type t) { t.is_ref = false; return t; }

// One place to carry runtime globals (functions + class runtime tables)
struct FunctionTable {
    std::unordered_map<std::string, std::vector<ast::FunctionDef*>> functions;

    // NEW: runtime info for classes (merged fields + vtable owner + virtual flags)
    ClassRuntime class_rt;

    void clear() {
        functions.clear();
        class_rt.classes.clear();
        class_rt.prog = nullptr;
    }

    void add(ast::FunctionDef& f) {
        auto& vec = functions[f.name];
        for (auto* existing : vec) {
            if (same_signature(*existing, f)) {
                throw std::runtime_error("duplicate function overload: " + f.name);
            }
        }
        vec.push_back(&f);
    }

    void add_program(ast::Program& p) {
        clear();
        for (auto& f : p.functions) add(f);
        class_rt.build(p);
    }

    // Overload resolution:
    // - exact base type match
    // - ref params require lvalue args
    // - ambiguity if equal "ref score"
    ast::FunctionDef& resolve(const std::string& name,
                              const std::vector<ast::Type>& arg_base_types,
                              const std::vector<bool>& arg_is_lvalue) {
        auto it = functions.find(name);
        if (it == functions.end()) {
            throw std::runtime_error("unknown function: " + name);
        }

        ast::FunctionDef* best = nullptr;
        int best_score = -1;

        for (auto* f : it->second) {
            if (f->params.size() != arg_base_types.size()) continue;

            bool ok = true;
            int score = 0;

            for (size_t i = 0; i < arg_base_types.size(); ++i) {
                ast::Type at = base_type(arg_base_types[i]);
                ast::Type pt = f->params[i].type;

                if (base_type(pt) != at) { ok = false; break; }

                if (pt.is_ref) {
                    if (!arg_is_lvalue[i]) { ok = false; break; }
                    score += 1;
                }
            }

            if (!ok) continue;

            if (score > best_score) {
                best_score = score;
                best = f;
            } else if (score == best_score) {
                throw std::runtime_error("ambiguous overload: " + name);
            }
        }

        if (!best) {
            throw std::runtime_error("no matching overload: " + name);
        }
        return *best;
    }
};

} // namespace interp
