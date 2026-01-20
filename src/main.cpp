#include <iostream>

#include "interp/env.hpp"
#include "interp/exec.hpp"
#include "interp/functions.hpp"
#include "ast/expr.hpp"

static std::unique_ptr<ast::BinaryExpr> make_bin(
    ast::BinaryExpr::Op op,
    ast::ExprPtr lhs,
    ast::ExprPtr rhs
) {
    auto b = std::make_unique<ast::BinaryExpr>();
    b->op = op;
    b->left = std::move(lhs);
    b->right = std::move(rhs);
    return b;
}

int main() {
    using namespace ast;
    using namespace interp;

    Env env(nullptr);
    FunctionTable functions;

    // AST: 1 + (2 * 3)
    ExprPtr expr = make_bin(
        BinaryExpr::Op::Add,
        std::make_unique<IntLiteral>(1),
        make_bin(
            BinaryExpr::Op::Mul,
            std::make_unique<IntLiteral>(2),
            std::make_unique<IntLiteral>(3)
        )
    );

    try {
        Value v = eval_expr(env, *expr, functions);
        std::cout << "Interpreter-Test OK, Ergebnis = " << std::get<int>(v) << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Interpreter-Test FEHLER: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
