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

static ast::ExprPtr make_var(const std::string& name) { return std::make_unique<ast::VarExpr>(name); }

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

static std::unique_ptr<ast::ConstructExpr> make_ctor(const std::string& cls, std::vector<ast::ExprPtr> args) {
    auto c = std::make_unique<ast::ConstructExpr>();
    c->class_name = cls;
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

static void expect_obj_class(interp::Env& env, const std::string& name, const std::string& cls, const char* testname) {
    interp::Value v = env.read_value(name);
    auto* po = std::get_if<interp::ObjectPtr>(&v);
    if (!po || !*po || (*po)->dynamic_class != cls) {
        std::cerr << "TEST FAIL: " << testname << " expected " << name << " dynamic_class == " << cls << "\n";
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

    // --- overloads: inc(int) und inc(int&) ---
    std::vector<std::unique_ptr<FunctionDef>> owned;

    {
        auto f = std::make_unique<FunctionDef>();
        f->name = "inc";
        f->return_type = t_int;
        f->params = { Param{"a", t_int} };

        std::vector<std::unique_ptr<Stmt>> body;
        body.push_back(make_return(make_bin(BinaryExpr::Op::Add, make_var("a"), make_int(1))));
        f->body = make_block(std::move(body));

        functions.add(*f);
        owned.push_back(std::move(f));
    }

    {
        auto f = std::make_unique<FunctionDef>();
        f->name = "inc";
        f->return_type = t_int;
        f->params = { Param{"a", t_int_ref} };

        std::vector<std::unique_ptr<Stmt>> body;
        body.push_back(make_exprstmt(make_assign("a", make_bin(BinaryExpr::Op::Add, make_var("a"), make_int(1)))));
        body.push_back(make_return(make_var("a")));
        f->body = make_block(std::move(body));

        functions.add(*f);
        owned.push_back(std::move(f));
    }

    // --- statement/ref tests ---
    { exec_stmt(env, *make_vardecl(t_int, "x", make_int(1)), functions); expect_int_var(env, "x", 1, "vardecl"); std::cout << "OK: vardecl\n"; }
    { (void)eval_expr(env, *make_assign("x", make_bin(BinaryExpr::Op::Add, make_var("x"), make_int(2))), functions); expect_int_var(env, "x", 3, "assign"); std::cout << "OK: assign\n"; }
    { exec_stmt(env, *make_vardecl(t_int_ref, "r", make_var("x")), functions); (void)eval_expr(env, *make_assign("r", make_int(10)), functions); expect_int_var(env, "x", 10, "ref"); std::cout << "OK: ref\n"; }

    // --- call tests ---
    {
        std::vector<ExprPtr> args; args.push_back(make_int(5));
        Value v = eval_expr(env, *make_call("inc", std::move(args)), functions);
        expect_int_value(v, 6, "inc(5)");
        std::cout << "OK: inc(5)\n";
    }
    {
        std::vector<ExprPtr> args; args.push_back(make_var("x"));
        Value v = eval_expr(env, *make_call("inc", std::move(args)), functions);
        expect_int_value(v, 11, "inc(x)");
        expect_int_var(env, "x", 11, "inc(x) mut");
        std::cout << "OK: inc(x)\n";
    }

    // --- class value tests (minimal runtime) ---
    {
        // D d;  -> default_value_for(Class) muss ein Objekt erzeugen
        exec_stmt(env, *make_vardecl(Type::Class("D", false), "d", nullptr), functions);
        expect_obj_class(env, "d", "D", "default class value");
        std::cout << "OK: default class value D d;\n";
    }
    {
        // E e = E(123); -> ConstructExpr erzeugt Objekt mit dynamic_class
        std::vector<ExprPtr> args; args.push_back(make_int(123));
        exec_stmt(env, *make_vardecl(Type::Class("E", false), "e", make_ctor("E", std::move(args))), functions);
        expect_obj_class(env, "e", "E", "construct expr");
        std::cout << "OK: construct expr E(123)\n";
    }

    std::cout << "ALLE TESTS OK\n";
    return 0;
}
