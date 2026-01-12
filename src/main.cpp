#include <iostream>

#include "interp/exec.hpp"

int main() {
    using namespace ast;
    using namespace interp;

    Env global;
    FunctionTable functions;

    global.define_value("a", 1);

    // int f(int x) { return x; }
    auto f_val = std::make_unique<FunctionDef>();
    f_val->name = "f";
    f_val->return_type = Type::Int();
    {
        Param p;
        p.name = "x";
        p.type = Type::Int(false); // int
        f_val->params.push_back(p);

        auto ret = std::make_unique<ReturnStmt>();
        ret->value = std::make_unique<VarExpr>("x");
        f_val->body = std::move(ret);
    }
    functions.add(*f_val);

    // int f(int& x) { x = 7; return x; }
    auto f_ref = std::make_unique<FunctionDef>();
    f_ref->name = "f";
    f_ref->return_type = Type::Int();
    {
        Param p;
        p.name = "x";
        p.type = Type::Int(true); // int&
        f_ref->params.push_back(p);

        auto body = std::make_unique<BlockStmt>();

        auto assign_stmt = std::make_unique<ExprStmt>();
        auto assign_expr = std::make_unique<AssignExpr>();
        assign_expr->name = "x";
        assign_expr->value = std::make_unique<IntLiteral>(7);
        assign_stmt->expr = std::move(assign_expr);
        body->statements.push_back(std::move(assign_stmt));

        auto ret = std::make_unique<ReturnStmt>();
        ret->value = std::make_unique<VarExpr>("x");
        body->statements.push_back(std::move(ret));

        f_ref->body = std::move(body);
    }
    functions.add(*f_ref);

    // call f(a) -> should pick f(int&) and modify a
    CallExpr c1;
    c1.callee = "f";
    c1.args.push_back(std::make_unique<VarExpr>("a"));
    Value r1 = eval_expr(global, c1, functions);

    // call f(5) -> should pick f(int)
    CallExpr c2;
    c2.callee = "f";
    c2.args.push_back(std::make_unique<IntLiteral>(5));
    Value r2 = eval_expr(global, c2, functions);

    std::cout << "f(a) = " << to_string(r1) << "\n";
    std::cout << "a = " << to_string(global.read_value("a")) << "\n";
    std::cout << "f(5) = " << to_string(r2) << "\n";
    return 0;
}
