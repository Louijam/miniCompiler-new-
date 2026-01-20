#include <iostream>

#include "sem/program_analyzer.hpp"
#include "ast/program.hpp"
#include "ast/class.hpp"
#include "ast/stmt.hpp"
#include "ast/expr.hpp"
#include "ast/type.hpp"

#include "interp/exec.hpp"
#include "interp/functions.hpp"
#include "interp/class_runtime.hpp"

static ast::MethodDef make_B_m_virtual_returns_1() {
    using namespace ast;
    MethodDef m;
    m.is_virtual = true;
    m.name = "m";
    m.return_type = Type::Int();

    auto body = std::make_unique<BlockStmt>();
    auto ret = std::make_unique<ReturnStmt>();
    ret->value = std::make_unique<IntLiteral>(1);
    body->statements.push_back(std::move(ret));
    m.body = std::move(body);
    return m;
}

static ast::MethodDef make_D_m_override_returns_2() {
    using namespace ast;
    MethodDef m;
    m.is_virtual = false; // override stays virtual via base
    m.name = "m";
    m.return_type = Type::Int();

    auto body = std::make_unique<BlockStmt>();
    auto ret = std::make_unique<ReturnStmt>();
    ret->value = std::make_unique<IntLiteral>(2);
    body->statements.push_back(std::move(ret));
    m.body = std::move(body);
    return m;
}

static ast::FunctionDef make_run_dispatch_test() {
    using namespace ast;

    // int run() { D d; B& b = d; return b.m(); }
    FunctionDef f;
    f.name = "run";
    f.return_type = Type::Int();

    auto body = std::make_unique<BlockStmt>();

    auto decl_d = std::make_unique<VarDeclStmt>();
    decl_d->decl_type = Type::Class("D");
    decl_d->name = "d";
    body->statements.push_back(std::move(decl_d));

    auto decl_b = std::make_unique<VarDeclStmt>();
    decl_b->decl_type = Type::Class("B", true);
    decl_b->name = "b";
    decl_b->init = std::make_unique<VarExpr>("d");
    body->statements.push_back(std::move(decl_b));

    auto call = std::make_unique<MethodCallExpr>();
    call->object = std::make_unique<VarExpr>("b");
    call->method = "m";

    auto ret = std::make_unique<ReturnStmt>();
    ret->value = std::move(call);
    body->statements.push_back(std::move(ret));

    f.body = std::move(body);
    return f;
}

int main() {
    using namespace ast;

    try {
        Program p;

        ClassDef B;
        B.name = "B";
        B.methods.push_back(make_B_m_virtual_returns_1());

        ClassDef D;
        D.name = "D";
        D.base_name = "B";
        D.methods.push_back(make_D_m_override_returns_2());

        p.classes.push_back(std::move(B));
        p.classes.push_back(std::move(D));
        p.functions.push_back(make_run_dispatch_test());

        // sem check
        sem::ProgramAnalyzer pa;
        pa.analyze(p);

        // runtime class table
        interp::ClassRuntime rt;
        rt.build_from_program(p);

        // function table: add both method impls + run()
        interp::FunctionTable ft;

        // add run()
        ft.add(p.functions[0]);

        // add methods as functions "B::m" and "D::m"
        // we reuse bodies from AST methods by copying minimal FunctionDef wrappers
        ast::FunctionDef bm;
        bm.name = "B::m";
        bm.return_type = p.classes[0].methods[0].return_type;
        bm.params = p.classes[0].methods[0].params;
        bm.body = std::move(p.classes[0].methods[0].body);
        ft.add(bm);

        ast::FunctionDef dm;
        dm.name = "D::m";
        dm.return_type = p.classes[1].methods[0].return_type;
        dm.params = p.classes[1].methods[0].params;
        dm.body = std::move(p.classes[1].methods[0].body);
        ft.add(dm);

        // execute run()
        interp::Env env(nullptr);
        env.define_value_typed("dummy", Type::Int(), 0); // keep env non-empty

        ast::CallExpr call_run;
        call_run.callee = "run";

        interp::Value out = interp::eval_expr(env, call_run, ft, rt);
        std::cout << "dispatch=" << std::get<int>(out) << "\n";
    } catch (const std::exception& ex) {
        std::cout << "error=" << ex.what() << "\n";
    }

    return 0;
}
