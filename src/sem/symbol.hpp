#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>

#include "../ast/type.hpp"

namespace sem {

struct VarSymbol {
    std::string name;
    ast::Type type;
};

struct FuncSymbol {
    std::string name;
    ast::Type return_type;
    std::vector<ast::Type> param_types; // incl. & marker
};

inline bool same_signature(const FuncSymbol& a, const FuncSymbol& b) {
    if (a.name != b.name) return false;
    if (a.param_types.size() != b.param_types.size()) return false;
    for (size_t i = 0; i < a.param_types.size(); ++i) {
        if (a.param_types[i] != b.param_types[i]) return false;
    }
    return true;
}

} // namespace sem
