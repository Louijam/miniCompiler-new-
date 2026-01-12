#include <iostream>
#include "interp/exec.hpp"

int main() {
    using namespace ast;
    using namespace interp;

    Env global;
    FunctionTable functions;

    // a = 0
    global.define_value("a", 0);

    // function: int set(int& x) { x = 7; return x; }
    auto setf = std::make_unique<FunctionDef>();
    setf->name = "set";
    setf->return_type = Type::Int();

    Param px;
    px.name = "x";
    px.type = Type::Int(true); // int&
    setf->params.push_back(px);

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

    setf->body = std::move(body);
    functions.add(*setf);

    // expr: (0 && set(a))  -> must NOT call set(a), so a stays 0
    auto call_set = std::make_unique<CallExpr>();
    call_set->callee = "set";
    call_set->args.push_back(std::make_unique<VarExpr>("a"));

    auto and_expr = std::make_unique<BinaryExpr>();
    and_expr->op = BinaryExpr::Op::AndAnd;
    and_expr->left = std::make_unique<IntLiteral>(0);
    and_expr->right = std::move(call_set);

    Value r = eval_expr(global, *and_expr, functions);

    std::cout << "expr=" << to_string(r) << "\n";
    std::cout << "a=" << to_string(global.read_value("a")) << "\n";
    return 0;
}
