#pragma once
#include <string>
#include <vector>
#include <memory>

#include "stmt.hpp"
#include "type.hpp"

namespace ast {

struct Param {
    std::string name;
    Type type;
};

struct FunctionDef {
    std::string name;
    Type return_type;
    std::vector<Param> params;
    std::unique_ptr<Stmt> body;
};

} // namespace ast
