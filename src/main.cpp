#include <iostream>
#include <cstdlib>

#include "interp/env.hpp"
#include "interp/exec.hpp"
#include "interp/functions.hpp"
#include "ast/expr.hpp"
#include "ast/stmt.hpp"
#include "ast/type.hpp"

static ast::ExprPtr make_int(int v) { return std::make_unique<ast::IntLiteral>(v); }

static std::unique_ptr<ast::BinaryExpr> make_bin(ast::BinaryExpr::Op op, ast::ExprPtr lhs, ast::ExprPtr rhs) {
    auto b = std::make_unique<ast::BinaryExpr>();
    b->op = op;
    b->left = std::move(lhs);
    b->right = std::move(rhs);
    return b;
}

static ast::ExprPtr make_var(const std::string& name) {
    return std::make_unique<ast::VarExpr>(name);
}

static std::unique_ptr<ast::AssignExpr> make_assign(const std::string& name, ast::ExprPtr rhs) {
    auto a = std::make_unique<ast::AssignExpr>();
    a->name = name;
    a->value = std::move(rhs);
    return a;
}

static std::unique_ptr<ast::VarDeclStmt> make_vardecl(const ast::Type& t, const std::string& name, ast::ExprPtr init) {
    auto s = std::make_unique<ast::VarDeclStmt>();
    s->decl_type = t;
    s->name = name;
    s->init = std::move(init);
    return s;
}

static std::unique_ptr<ast::ExprStmt> make_exprstmt(ast::ExprPtr e) {
    auto s = std::make_unique<ast::ExprStmt>();
    s->expr = std::move(e);
    return s;
}

static std::unique_ptr<ast::WhileStmt> make_while(ast::ExprPtr cond, std::unique_ptr<ast::Stmt> body) {
    auto w = std::make_unique<ast::WhileStmt>();
    w->cond = std::move(cond);
    w->body = std::move(body);
    return w;
}

static std::unique_ptr<ast::BlockStmt> make_block(std::vector<std::unique_ptr<ast::Stmt>> stmts) {
    auto b = std::make_unique<ast::BlockStmt>();
    b->statements = std::move(stmts);
    return b;
}

static void expect_int_var(interp::Env& env, const std::string& name, int expected, const char* testname) {
    interp::Value v = env.read_value(name);
    auto* pi = std::get_if<int>(&v);
    if (!pi || *pi != expected) {
        std::cerr << "TEST FAIL: " << testname << " expected " << name << " == " << expected << "\n";
        std::exit(1);
    }
}

int main() {
    using namespace ast;
    using namespace interp;

    Env env(nullptr);
    FunctionTable functions;

    Type t_int = Type::Int(false);
    Type t_int_ref = Type::Int(true);

    // 1) int x = 1;
    {
        auto s = make_vardecl(t_int, "x", make_int(1));
        exec_stmt(env, *s, functions);
        expect_int_var(env, "x", 1, "vardecl int init");
        std::cout << "OK: vardecl int init\n";
    }

    // 2) x = x + 2;  => 3
    {
        auto e = make_assign("x", make_bin(BinaryExpr::Op::Add, make_var("x"), make_int(2)));
        (void)eval_expr(env, *e, functions);
        expect_int_var(env, "x", 3, "assign expr");
        std::cout << "OK: assign expr\n";
    }

    // 3) int& r = x; r = 10;  => x wird 10
    {
        auto s1 = make_vardecl(t_int_ref, "r", make_var("x"));
        exec_stmt(env, *s1, functions);

        auto e = make_assign("r", make_int(10));
        (void)eval_expr(env, *e, functions);

        expect_int_var(env, "x", 10, "ref assign writes through");
        std::cout << "OK: ref assign writes through\n";
    }

    // 4) while(x) { x = x - 1; }  => endet bei 0 (bool-like-cpp: int 0 false)
    {
        std::vector<std::unique_ptr<Stmt>> body;
        body.push_back(make_exprstmt(
            make_assign("x", make_bin(BinaryExpr::Op::Sub, make_var("x"), make_int(1)))
        ));

        auto loop = make_while(make_var("x"), make_block(std::move(body)));
        exec_stmt(env, *loop, functions);

        expect_int_var(env, "x", 0, "while bool-like int");
        std::cout << "OK: while bool-like int\n";
    }

    std::cout << "ALLE STMT-TESTS OK\n";
    return 0;
}
