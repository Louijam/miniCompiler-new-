#pragma once
#include <string>
#include <vector>
#include <memory>

#include "type.hpp"
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
    std::vector<Param> params;     // reuse Param from function.hpp
    StmtPtr body;                  // later used by interpreter
};

struct ClassDef {
    std::string name;
    std::string base_name;         // empty => no base class
    std::vector<FieldDecl> fields;
    std::vector<MethodDef> methods;
};

} // namespace ast
