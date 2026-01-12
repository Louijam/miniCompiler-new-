#include <iostream>

#include "interp/exec.hpp"

int main() {
    using namespace ast;
    using namespace interp;

    Env global;
    FunctionTable functions;

    // define: int a = 1;
    global.define_value("a", 1);

    // function: int set(int& x) { x = 7; return x; }
    auto setf = std::make_unique<FunctionDef>();
    setf->name = "set";
    setf->return_type = Type::Int();

    Param px;
    px.name = "x";
    px.type = Type::Int(true); // int&
    setf->params.push_back(px);

    auto body = std::make_unique<BlockStmt>();

    // x = 7;
    auto assign_stmt = std::make_unique<ExprStmt>();
    auto assign_expr = std::make_unique<AssignExpr>();
    assign_expr->name = "x";
    assign_expr->value = std::make_unique<IntLiteral>(7);
    assign_stmt->expr = std::move(assign_expr);
    body->statements.push_back(std::move(assign_stmt));

    // return x;
    auto ret = std::make_unique<ReturnStmt>();
    ret->value = std::make_unique<VarExpr>("x");
    body->statements.push_back(std::move(ret));

    setf->body = std::move(body);
    functions.add(*setf);

    // call: set(a)
    CallExpr call;
    call.callee = "set";
    call.args.push_back(std::make_unique<VarExpr>("a"));

    Value rv = eval_expr(global, call, functions);

    std::cout << "return=" << to_string(rv) << "\n";
    std::cout << "a=" << to_string(global.read_value("a")) << "\n";
    return 0;
}
