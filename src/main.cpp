#include <iostream>
#include <cstdlib>
#include <memory>
#include <vector>

#include "interp/env.hpp"
#include "interp/exec.hpp"
#include "interp/functions.hpp"
#include "ast/expr.hpp"
#include "ast/stmt.hpp"
#include "ast/type.hpp"
#include "ast/function.hpp"

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

static std::unique_ptr<ast::ReturnStmt> make_return(ast::ExprPtr e) {
    auto r = std::make_unique<ast::ReturnStmt>();
    r->value = std::move(e);
    return r;
}

static std::unique_ptr<ast::BlockStmt> make_block(std::vector<std::unique_ptr<ast::Stmt>> stmts) {
    auto b = std::make_unique<ast::BlockStmt>();
    b->statements = std::move(stmts);
    return b;
}

static std::unique_ptr<ast::CallExpr> make_call(const std::string& name, std::vector<ast::ExprPtr> args) {
    auto c = std::make_unique<ast::CallExpr>();
    c->callee = name;
    c->args = std::move(args);
    return c;
}

static void expect_int_var(interp::Env& env, const std::string& name, int expected, const char* testname) {
    interp::Value v = env.read_value(name);
    auto* pi = std::get_if<int>(&v);
    if (!pi || *pi != expected) {
        std::cerr << "TEST FAIL: " << testname << " expected " << name << " == " << expected << "\n";
        std::exit(1);
    }
}

static void expect_int_value(const interp::Value& v, int expected, const char* testname) {
    auto* pi = std::get_if<int>(&v);
    if (!pi || *pi != expected) {
        std::cerr << "TEST FAIL: " << testname << " expected value == " << expected << "\n";
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

    // ---------- Funktionen definieren (ohne Parser) ----------
    // int inc(int a) { return a + 1; }
    std::vector<std::unique_ptr<FunctionDef>> owned_funcs;

    {
        auto f = std::make_unique<FunctionDef>();
        f->name = "inc";
        f->return_type = t_int;
        f->params = { Param{"a", t_int} };

        std::vector<std::unique_ptr<Stmt>> body;
        body.push_back(make_return(
            make_bin(BinaryExpr::Op::Add, make_var("a"), make_int(1))
        ));
        f->body = make_block(std::move(body));

        functions.add(*f);
        owned_funcs.push_back(std::move(f));
    }

    // int inc(int& a) { a = a + 1; return a; }
    {
        auto f = std::make_unique<FunctionDef>();
        f->name = "inc";
        f->return_type = t_int;
        f->params = { Param{"a", t_int_ref} };

        std::vector<std::unique_ptr<Stmt>> body;
        body.push_back(make_exprstmt(
            make_assign("a", make_bin(BinaryExpr::Op::Add, make_var("a"), make_int(1)))
        ));
        body.push_back(make_return(make_var("a")));
        f->body = make_block(std::move(body));

        functions.add(*f);
        owned_funcs.push_back(std::move(f));
    }

    // ---------- Statement/Ref Tests ----------
    // int x = 1;
    {
        auto s = make_vardecl(t_int, "x", make_int(1));
        exec_stmt(env, *s, functions);
        expect_int_var(env, "x", 1, "vardecl int init");
        std::cout << "OK: vardecl int init\n";
    }

    // x = x + 2; => 3
    {
        auto e = make_assign("x", make_bin(BinaryExpr::Op::Add, make_var("x"), make_int(2)));
        (void)eval_expr(env, *e, functions);
        expect_int_var(env, "x", 3, "assign expr");
        std::cout << "OK: assign expr\n";
    }

    // int& r = x; r = 10; => x wird 10
    {
        auto s1 = make_vardecl(t_int_ref, "r", make_var("x"));
        exec_stmt(env, *s1, functions);

        auto e = make_assign("r", make_int(10));
        (void)eval_expr(env, *e, functions);

        expect_int_var(env, "x", 10, "ref assign writes through");
        std::cout << "OK: ref assign writes through\n";
    }

    // ---------- Funktionsaufruf Tests ----------
    // inc(5) -> 6  (Value-Arg => nimmt inc(int))
    {
        std::vector<ExprPtr> args;
        args.push_back(make_int(5));
        auto c = make_call("inc", std::move(args));
        Value v = eval_expr(env, *c, functions);
        expect_int_value(v, 6, "call inc(5)");
        std::cout << "OK: call inc(5)\n";
    }

    // inc(x) -> 11 und x wird 11 (LValue-Arg => nimmt inc(int&))
    {
        std::vector<ExprPtr> args;
        args.push_back(make_var("x"));
        auto c = make_call("inc", std::move(args));
        Value v = eval_expr(env, *c, functions);
        expect_int_value(v, 11, "call inc(x) returns 11");
        expect_int_var(env, "x", 11, "call inc(x) mutates x");
        std::cout << "OK: call inc(x) overload int&\n";
    }

    std::cout << "ALLE CALL-TESTS OK\n";
    return 0;
}
