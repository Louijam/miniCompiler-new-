#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>

#include "repl/repl.hpp"

#include "ast/program.hpp"
#include "ast/class.hpp"
#include "ast/function.hpp"
#include "ast/stmt.hpp"
#include "ast/expr.hpp"
#include "ast/type.hpp"

#include "interp/env.hpp"
#include "interp/functions.hpp"
#include "interp/exec.hpp"

static std::unique_ptr<ast::BlockStmt> block(std::vector<ast::StmtPtr> s) {
    auto b = std::make_unique<ast::BlockStmt>();
    b->statements = std::move(s);
    return b;
}

static ast::ExprPtr call_print_int(int v) {
    auto c = std::make_unique<ast::CallExpr>();
    c->callee = "print_int";
    c->args.push_back(std::make_unique<ast::IntLiteral>(v));
    return c;
}

static ast::StmtPtr expr_stmt(ast::ExprPtr e) {
    auto s = std::make_unique<ast::ExprStmt>();
    s->expr = std::move(e);
    return s;
}

static ast::StmtPtr ret_int(int v) {
    auto r = std::make_unique<ast::ReturnStmt>();
    r->value = std::make_unique<ast::IntLiteral>(v);
    return r;
}

static int run_selftest_dispatch() {
    using namespace ast;

    // class B { public: virtual int m(){ print_int(1); return 0; } }
    ClassDef B;
    B.name = "B";

    MethodDef Bm;
    Bm.is_virtual = true;
    Bm.name = "m";
    Bm.return_type = Type::Int(false);
    {
        std::vector<StmtPtr> ss;
        ss.push_back(expr_stmt(call_print_int(1)));
        ss.push_back(ret_int(0));
        Bm.body = block(std::move(ss));
    }
    B.methods.push_back(std::move(Bm));

    // class D : public B { public: int m(){ print_int(2); return 0; } }
    ClassDef D;
    D.name = "D";
    D.base_name = "B";

    MethodDef Dm;
    Dm.is_virtual = false; // override should still be treated virtual at runtime (via vtable)
    Dm.name = "m";
    Dm.return_type = Type::Int(false);
    {
        std::vector<StmtPtr> ss;
        ss.push_back(expr_stmt(call_print_int(2)));
        ss.push_back(ret_int(0));
        Dm.body = block(std::move(ss));
    }
    D.methods.push_back(std::move(Dm));

    // program
    Program p;
    p.classes.push_back(std::move(B));
    p.classes.push_back(std::move(D));

    // build runtime tables
    interp::FunctionTable ft;
    ft.add_program(p);

    // simulate:
    // D d; B& b = d; b.m();
    interp::Env env(nullptr);

    // D d;
    {
        auto tD = Type::Class("D", false);
        env.define_value("d", interp::default_value_for_type(tD, ft), tD);
    }

    // B& b = d;
    {
        auto tBRef = Type::Class("B", true);
        env.define_ref("b", env.resolve_lvalue("d"), tBRef);
    }

    // b.m();
    {
        auto mc = std::make_unique<MethodCallExpr>();
        mc->object = std::make_unique<VarExpr>("b");
        mc->method = "m";
        interp::eval_expr(env, *mc, ft);
    }

    std::cout << "DISPATCH SELFTEST OK\n";
    return 0;
}

int main(int argc, char** argv) {
    try {
        if (argc > 1) {
            std::string arg = argv[1];
            if (arg == "--selftest-dispatch") {
                return run_selftest_dispatch();
            }
        }
        return repl::run_repl();
    } catch (const std::exception& ex) {
        std::cerr << "FEHLER: " << ex.what() << "\n";
        return 1;
    }
}
