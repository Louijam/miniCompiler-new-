#include <iostream>

#include "sem/program_analyzer.hpp"
#include "ast/program.hpp"
#include "ast/class.hpp"
#include "ast/stmt.hpp"
#include "ast/expr.hpp"
#include "ast/type.hpp"

static ast::MethodDef make_ok_method_uses_field_y() {
    using namespace ast;
    MethodDef m;
    m.name = "m";
    m.return_type = Type::Int();

    // { return y; }  (y is a field)
    auto body = std::make_unique<BlockStmt>();
    auto ret = std::make_unique<ReturnStmt>();
    ret->value = std::make_unique<VarExpr>("y");
    body->statements.push_back(std::move(ret));
    m.body = std::move(body);
    return m;
}

static ast::MethodDef make_bad_method_param_shadows_field() {
    using namespace ast;
    MethodDef m;
    m.name = "bad";
    m.return_type = Type::Int();
    m.params.push_back(Param{"y", Type::Int(false)}); // shadows field y -> must error

    auto body = std::make_unique<BlockStmt>();
    auto ret = std::make_unique<ReturnStmt>();
    ret->value = std::make_unique<VarExpr>("y");
    body->statements.push_back(std::move(ret));
    m.body = std::move(body);
    return m;
}

int main() {
    using namespace ast;
    using namespace sem;

    try {
        Program p;

        ClassDef A;
        A.name = "A";
        A.fields.push_back(FieldDecl{Type::Int(), "y"});
        A.methods.push_back(make_ok_method_uses_field_y());

        p.classes.push_back(std::move(A));

        ProgramAnalyzer pa;
        pa.analyze(p);
        std::cout << "ok\n";
    } catch (const std::exception& ex) {
        std::cout << "error=" << ex.what() << "\n";
    }

    try {
        Program p2;

        ClassDef A;
        A.name = "A";
        A.fields.push_back(FieldDecl{Type::Int(), "y"});
        A.methods.push_back(make_bad_method_param_shadows_field());

        p2.classes.push_back(std::move(A));

        ProgramAnalyzer pa;
        pa.analyze(p2);
        std::cout << "NO_ERROR\n";
    } catch (const std::exception& ex) {
        std::cout << "error2=" << ex.what() << "\n";
    }

    return 0;
}
