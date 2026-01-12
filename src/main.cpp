#include <iostream>
#include "interp/exec.hpp"

int main() {
    using namespace ast;
    using namespace interp;

    Env global;

    // int x = 1;
    VarDeclStmt decl;
    decl.name = "x";
    decl.init = std::make_unique<IntLiteral>(1);
    exec_stmt(global, decl);

    // x = 42;
    ExprStmt assign;
    auto a = std::make_unique<AssignExpr>();
    a->name = "x";
    a->value = std::make_unique<IntLiteral>(42);
    assign.expr = std::move(a);
    exec_stmt(global, assign);

    std::cout << "x=" << to_string(global.read_value("x")) << "\n";
    return 0;
}
