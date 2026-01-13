#pragma once
#include <vector>

#include "function.hpp"
#include "class.hpp"

namespace ast {

struct Program {
    std::vector<ClassDef> classes;
    std::vector<FunctionDef> functions;
};

} // namespace ast
