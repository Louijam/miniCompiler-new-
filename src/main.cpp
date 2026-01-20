#include <iostream>
#include <stdexcept>

#include "interp/env.hpp"
#include "interp/exec.hpp"
#include "interp/functions.hpp"
#include "ast/expr.hpp"
#include "ast/type.hpp"

static ast::ExprPtr make_int(int v) { return std::make_unique<ast::IntLiteral>(v); }
static ast::ExprPtr make_bool(bool v) { return std::make_unique<ast::BoolLiteral>(v); }
static ast::ExprPtr make_char(char v) { return std::make_unique<ast::CharLiteral>(v); }
static ast::ExprPtr make_string(const std::string& v) { return std::make_unique<ast::StringLiteral>(v); }

static std::unique_ptr<ast::BinaryExpr> make_bin(ast::BinaryExpr::Op op, ast::ExprPtr lhs, ast::ExprPtr rhs) {
    auto b = std::make_unique<ast::BinaryExpr>();
    b->op = op;
    b->left = std::move(lhs);
    b->right = std::move(rhs);
    return b;
}

static std::unique_ptr<ast::UnaryExpr> make_un(ast::UnaryExpr::Op op, ast::ExprPtr x) {
    auto u = std::make_unique<ast::UnaryExpr>();
    u->op = op;
    u->expr = std::move(x);
    return u;
}

static void expect_int_eq(const interp::Value& v, int expected, const char* name) {
    auto* pi = std::get_if<int>(&v);
    if (!pi || *pi != expected) {
        std::cerr << "TEST FAIL: " << name << " (expected int " << expected << ")\n";
        std::exit(1);
    }
}

static void expect_bool_eq(const interp::Value& v, bool expected, const char* name) {
    auto* pb = std::get_if<bool>(&v);
    if (!pb || *pb != expected) {
        std::cerr << "TEST FAIL: " << name << " (expected bool " << (expected ? "true" : "false") << ")\n";
        std::exit(1);
    }
}

static void run_test(interp::Env& env, interp::FunctionTable& functions, const char* name, const ast::Expr& e, const interp::Value& expected) {
    try {
        interp::Value got = interp::eval_expr(env, e, functions);
        if (std::holds_alternative<int>(expected)) {
            expect_int_eq(got, std::get<int>(expected), name);
        } else if (std::holds_alternative<bool>(expected)) {
            expect_bool_eq(got, std::get<bool>(expected), name);
        } else {
            std::cerr << "TEST FAIL: " << name << " (unsupported expected type in harness)\n";
            std::exit(1);
        }
        std::cout << "OK: " << name << "\n";
    } catch (const std::exception& ex) {
        std::cerr << "TEST FAIL: " << name << " threw: " << ex.what() << "\n";
        std::exit(1);
    }
}

int main() {
    using namespace ast;
    using namespace interp;

    Env env(nullptr);
    FunctionTable functions;

    // 1) Arithmetic: 1 + (2 * 3) = 7
    {
        ExprPtr e = make_bin(
            BinaryExpr::Op::Add,
            make_int(1),
            make_bin(BinaryExpr::Op::Mul, make_int(2), make_int(3))
        );
        run_test(env, functions, "arith 1+2*3", *e, Value{7});
    }

    // 2) Relational: 3 < 4  => true
    {
        ExprPtr e = make_bin(BinaryExpr::Op::Lt, make_int(3), make_int(4));
        run_test(env, functions, "rel 3<4", *e, Value{true});
    }

    // 3) Equality: 'a' == 'a' => true, "x" != "y" => true
    {
        ExprPtr e1 = make_bin(BinaryExpr::Op::Eq, make_char('a'), make_char('a'));
        run_test(env, functions, "eq 'a'=='a'", *e1, Value{true});

        ExprPtr e2 = make_bin(BinaryExpr::Op::Ne, make_string("x"), make_string("y"));
        run_test(env, functions, "ne \"x\"!=\"y\"", *e2, Value{true});
    }

    // 4) Unary ! : !true => false
    {
        ExprPtr e = make_un(UnaryExpr::Op::Not, make_bool(true));
        run_test(env, functions, "unary !true", *e, Value{false});
    }

    // 5) Short-circuit && : false && (1/0 == 0) => false (MUSS nicht crashen)
    {
        ExprPtr div0 = make_bin(BinaryExpr::Op::Div, make_int(1), make_int(0));
        ExprPtr rhs = make_bin(BinaryExpr::Op::Eq, std::move(div0), make_int(0));
        ExprPtr e = make_bin(BinaryExpr::Op::AndAnd, make_bool(false), std::move(rhs));
        run_test(env, functions, "shortcircuit false&&...", *e, Value{false});
    }

    // 6) Short-circuit || : true || (1/0 == 0) => true (MUSS nicht crashen)
    {
        ExprPtr div0 = make_bin(BinaryExpr::Op::Div, make_int(1), make_int(0));
        ExprPtr rhs = make_bin(BinaryExpr::Op::Eq, std::move(div0), make_int(0));
        ExprPtr e = make_bin(BinaryExpr::Op::OrOr, make_bool(true), std::move(rhs));
        run_test(env, functions, "shortcircuit true||...", *e, Value{true});
    }

    std::cout << "ALLE TESTS OK\n";
    return 0;
}
