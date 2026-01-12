#pragma once
#include <unordered_map>
#include <string>
#include <stdexcept>

#include "../ast/function.hpp"

namespace interp {

struct FunctionTable {
    std::unordered_map<std::string, ast::FunctionDef*> functions;

    void add(ast::FunctionDef& f) {
        if (functions.find(f.name) != functions.end())
            throw std::runtime_error("duplicate function: " + f.name);
        functions[f.name] = &f;
    }

    ast::FunctionDef& get(const std::string& name) {
        auto it = functions.find(name);
        if (it == functions.end())
            throw std::runtime_error("unknown function: " + name);
        return *it->second;
    }
};

} // namespace interp
