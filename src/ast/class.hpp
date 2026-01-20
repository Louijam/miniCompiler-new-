#pragma once
#include <string>
#include <vector>
#include <memory>

#include "type.hpp"
#include "function.hpp" // defines ast::Param
#include "stmt.hpp"

namespace ast {

struct FieldDecl {
    Type type;
    std::string name;
};

struct MethodDef {
    bool is_virtual = false;
    std::string name;
    Type return_type;
    std::vector<ast::Param> params;
    StmtPtr body;
};

// NEW: Constructors are separate from methods (no return type).
struct ConstructorDef {
    std::vector<ast::Param> params;
    StmtPtr body;
};

struct ClassDef {
    std::string name;
    std::string base_name; // empty => no base class

    std::vector<FieldDecl> fields;
    std::vector<ConstructorDef> ctors; // NEW
    std::vector<MethodDef> methods;
};

} // namespace ast
