#include <iostream>
#include "interp/exec.hpp"

static std::unique_ptr<ast::FunctionDef> make_set_func() {
    using namespace ast;

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
    return setf;
}

int main() {
    using namespace ast;
    using namespace interp;

    Env global;
    FunctionTable functions;

    global.define_value("a", 0);
    auto setf = make_set_func();
    functions.add(*setf);

    // e1 = (0 && set(a))  -> short-circuit, a stays 0
    auto e1 = std::make_unique<BinaryExpr>();
    e1->op = BinaryExpr::Op::AndAnd;
    e1->left = std::make_unique<IntLiteral>(0);
    auto c1 = std::make_unique<CallExpr>();
    c1->callee = "set";
    c1->args.push_back(std::make_unique<VarExpr>("a"));
    e1->right = std::move(c1);

    // e2 = (1 || set(a)) -> short-circuit, a stays 0
    auto e2 = std::make_unique<BinaryExpr>();
    e2->op = BinaryExpr::Op::OrOr;
    e2->left = std::make_unique<IntLiteral>(1);
    auto c2 = std::make_unique<CallExpr>();
    c2->callee = "set";
    c2->args.push_back(std::make_unique<VarExpr>("a"));
    e2->right = std::move(c2);

    // e3 = (5 + 3*2 == 11)
    auto mul = std::make_unique<BinaryExpr>();
    mul->op = BinaryExpr::Op::Mul;
    mul->left = std::make_unique<IntLiteral>(3);
    mul->right = std::make_unique<IntLiteral>(2);

    auto add = std::make_unique<BinaryExpr>();
    add->op = BinaryExpr::Op::Add;
    add->left = std::make_unique<IntLiteral>(5);
    add->right = std::move(mul);

    auto e3 = std::make_unique<BinaryExpr>();
    e3->op = BinaryExpr::Op::Eq;
    e3->left = std::move(add);
    e3->right = std::make_unique<IntLiteral>(11);

    // e4 = !(5 == 5)
    auto eq55 = std::make_unique<BinaryExpr>();
    eq55->op = BinaryExpr::Op::Eq;
    eq55->left = std::make_unique<IntLiteral>(5);
    eq55->right = std::make_unique<IntLiteral>(5);

    auto e4 = std::make_unique<UnaryExpr>();
    e4->op = UnaryExpr::Op::Not;
    e4->expr = std::move(eq55);

    Value r1 = eval_expr(global, *e1, functions);
    Value r2 = eval_expr(global, *e2, functions);
    Value r3 = eval_expr(global, *e3, functions);
    Value r4 = eval_expr(global, *e4, functions);

    std::cout << "e1=" << to_string(r1) << "\n";
    std::cout << "e2=" << to_string(r2) << "\n";
    std::cout << "a=" << to_string(global.read_value("a")) << "\n";
    std::cout << "e3=" << to_string(r3) << "\n";
    std::cout << "e4=" << to_string(r4) << "\n";
    return 0;
}
