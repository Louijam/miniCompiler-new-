#include <iostream>

#include "sem/program_analyzer.hpp"
#include "ast/program.hpp"
#include "ast/class.hpp"
#include "ast/function.hpp"
#include "ast/stmt.hpp"
#include "ast/expr.hpp"
#include "ast/type.hpp"

static ast::MethodDef make_method_virtual_m() {
    using namespace ast;
    MethodDef m;
    m.is_virtual = true;
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

static ast::MethodDef make_method_override_m() {
    using namespace ast;
    MethodDef m;
    m.is_virtual = false; // override still ok if base is virtual
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

int main() {
    using namespace ast;
    using namespace sem;

    try {
        Program p;

        ClassDef A;
        A.name = "A";
        A.methods.push_back(make_method_virtual_m());

        ClassDef D;
        D.name = "D";
        D.base_name = "A";
        D.methods.push_back(make_method_override_m());

        p.classes.push_back(std::move(A));
        p.classes.push_back(std::move(D));

        ProgramAnalyzer pa;
        pa.analyze(p);
        std::cout << "ok\n";
    } catch (const std::exception& ex) {
        std::cout << "error=" << ex.what() << "\n";
    }

    try {
        Program p2;

        ClassDef X; X.name = "X"; X.base_name = "Y";
        ClassDef Y; Y.name = "Y"; Y.base_name = "X";

        p2.classes.push_back(std::move(X));
        p2.classes.push_back(std::move(Y));

        ProgramAnalyzer pa;
        pa.analyze(p2);
        std::cout << "NO_ERROR\n";
    } catch (const std::exception& ex) {
        std::cout << "error2=" << ex.what() << "\n";
    }

    return 0;
}
