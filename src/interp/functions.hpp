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

// One place to carry "runtime globals" needed by the interpreter.
struct FunctionTable {
    std::unordered_map<std::string, std::vector<ast::FunctionDef*>> functions;

    // NEW: runtime info for classes (fields + vtable resolution)
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

    // NEW: build all runtime tables from a parsed/analyzed program
    void add_program(ast::Program& p) {
        clear();

        for (auto& f : p.functions) add(f);

        // builds merged fields + vtable ownership + virtual flags
        class_rt.build(p);
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
