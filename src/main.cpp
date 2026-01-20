#include <iostream>

#include "sem/program_analyzer.hpp"
#include "ast/program.hpp"
#include "ast/class.hpp"
#include "ast/stmt.hpp"
#include "ast/expr.hpp"
#include "ast/type.hpp"

static ast::MethodDef make_m_int_returns_int() {
    using namespace ast;
    MethodDef m;
    m.name = "m";
    m.return_type = Type::Int();
    m.params.push_back(Param{"x", Type::Int(false)});

    auto body = std::make_unique<BlockStmt>();
    auto ret = std::make_unique<ReturnStmt>();
    ret->value = std::make_unique<VarExpr>("x");
    body->statements.push_back(std::move(ret));
    m.body = std::move(body);
    return m;
}

static ast::FunctionDef make_test_func() {
    using namespace ast;

    // int test() { A a; int t = a.y; return a.m(5); }
    FunctionDef f;
    f.name = "test";
    f.return_type = Type::Int();

    auto body = std::make_unique<BlockStmt>();

    auto decl_a = std::make_unique<VarDeclStmt>();
    decl_a->decl_type = Type::Class("A"); // requires your Type supports Class(name)
    decl_a->name = "a";
    body->statements.push_back(std::move(decl_a));

    auto mem = std::make_unique<MemberAccessExpr>();
    mem->object = std::make_unique<VarExpr>("a");
    mem->field = "y";

    auto decl_t = std::make_unique<VarDeclStmt>();
    decl_t->decl_type = Type::Int();
    decl_t->name = "t";
    decl_t->init = std::move(mem);
    body->statements.push_back(std::move(decl_t));

    auto call = std::make_unique<MethodCallExpr>();
    call->object = std::make_unique<VarExpr>("a");
    call->method = "m";
    call->args.push_back(std::make_unique<IntLiteral>(5));

    auto ret = std::make_unique<ReturnStmt>();
    ret->value = std::move(call);
    body->statements.push_back(std::move(ret));

    f.body = std::move(body);
    return f;
}

int main() {
    using namespace ast;
    using namespace sem;

    try {
        Program p;

        ClassDef A;
        A.name = "A";
        A.fields.push_back(FieldDecl{Type::Int(), "y"});
        A.methods.push_back(make_m_int_returns_int());

        p.classes.push_back(std::move(A));
        p.functions.push_back(make_test_func());

        ProgramAnalyzer pa;
        pa.analyze(p);
        std::cout << "ok\n";
    } catch (const std::exception& ex) {
        std::cout << "error=" << ex.what() << "\n";
    }

    return 0;
}
