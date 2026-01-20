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

    ClassDef D;
    D.name = "D";
    D.base_name = "B";

    MethodDef Dm;
    Dm.is_virtual = false;
    Dm.name = "m";
    Dm.return_type = Type::Int(false);
    {
        std::vector<StmtPtr> ss;
        ss.push_back(expr_stmt(call_print_int(2)));
        ss.push_back(ret_int(0));
        Dm.body = block(std::move(ss));
    }
    D.methods.push_back(std::move(Dm));

    Program p;
    p.classes.push_back(std::move(B));
    p.classes.push_back(std::move(D));

    interp::FunctionTable ft;
    ft.add_program(p);

    interp::Env env(nullptr);

    {
        auto tD = Type::Class("D", false);
        auto ce = std::make_unique<ConstructExpr>();
        ce->class_name = "D";
        interp::Value dv = interp::eval_expr(env, *ce, ft);
        env.define_value("d", dv, tD);
    }

    {
        auto tBRef = Type::Class("B", true);
        env.define_ref("b", env.resolve_lvalue("d"), tBRef);
    }

    {
        auto mc = std::make_unique<MethodCallExpr>();
        mc->object = std::make_unique<VarExpr>("b");
        mc->method = "m";
        interp::eval_expr(env, *mc, ft);
    }

    std::cout << "DISPATCH SELFTEST OK\n";
    return 0;
}

static int run_selftest_ctorchain() {
    using namespace ast;

    ClassDef B;
    B.name = "B";
    {
        ConstructorDef bc;
        std::vector<StmtPtr> ss;
        ss.push_back(expr_stmt(call_print_int(10)));
        bc.body = block(std::move(ss));
        B.ctors.push_back(std::move(bc));
    }

    ClassDef D;
    D.name = "D";
    D.base_name = "B";
    {
        ConstructorDef dc;
        std::vector<StmtPtr> ss;
        ss.push_back(expr_stmt(call_print_int(20)));
        dc.body = block(std::move(ss));
        D.ctors.push_back(std::move(dc));
    }

    Program p;
    p.classes.push_back(std::move(B));
    p.classes.push_back(std::move(D));

    interp::FunctionTable ft;
    ft.add_program(p);

    interp::Env env(nullptr);

    auto ce = std::make_unique<ConstructExpr>();
    ce->class_name = "D";
    (void)interp::eval_expr(env, *ce, ft);

    std::cout << "CTORCHAIN SELFTEST OK\n";
    return 0;
}

int main(int argc, char** argv) {
    try {
        if (argc > 1) {
            std::string arg = argv[1];
            if (arg == "--selftest-dispatch") return run_selftest_dispatch();
            if (arg == "--selftest-ctorchain") return run_selftest_ctorchain();
        }
        return repl::run_repl();
    } catch (const std::exception& ex) {
        std::cerr << "FEHLER: " << ex.what() << "\n";
        return 1;
    }
}
