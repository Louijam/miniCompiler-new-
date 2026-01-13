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
    std::vector<ast::Param> params; // fully qualified
    StmtPtr body;
};

struct ClassDef {
    std::string name;
    std::string base_name; // empty => no base class
    std::vector<FieldDecl> fields;
    std::vector<MethodDef> methods;
};

} // namespace ast
