#include <iostream>
#include "sem/analyzer.hpp"
#include "ast/function.hpp"
#include "ast/stmt.hpp"
#include "ast/expr.hpp"
#include "ast/type.hpp"

int main() {
    using namespace ast;
    using namespace sem;

    try {
        Scope global;
        Analyzer az;

        FunctionDef g;
        g.name = "g";
        g.return_type = Type::Int();
        g.params.push_back(Param{"x", Type::Int(false)});

        auto body = std::make_unique<BlockStmt>();

        // int y = x;
        auto decl = std::make_unique<VarDeclStmt>();
        decl->decl_type = Type::Int();
        decl->name = "y";
        decl->init = std::make_unique<VarExpr>("x");
        body->statements.push_back(std::move(decl));

        // if (y) { y = y + 1; }
        auto ifs = std::make_unique<IfStmt>();
        ifs->cond = std::make_unique<VarExpr>("y");

        auto thenb = std::make_unique<BlockStmt>();
        auto add = std::make_unique<BinaryExpr>();
        add->op = BinaryExpr::Op::Add;
        add->left = std::make_unique<VarExpr>("y");
        add->right = std::make_unique<IntLiteral>(1);

        auto asn = std::make_unique<AssignExpr>();
        asn->name = "y";
        asn->value = std::move(add);

        auto est = std::make_unique<ExprStmt>();
        est->expr = std::move(asn);
        thenb->statements.push_back(std::move(est));

        ifs->then_branch = std::move(thenb);
        body->statements.push_back(std::move(ifs));

        // return y;
        auto ret = std::make_unique<ReturnStmt>();
        ret->value = std::make_unique<VarExpr>("y");
        body->statements.push_back(std::move(ret));

        g.body = std::move(body);

        az.check_function(global, g);
        std::cout << "ok\n";

        // bad(): return z; (z unknown)
        FunctionDef bad;
        bad.name = "bad";
        bad.return_type = Type::Int();
        bad.body = std::make_unique<BlockStmt>();
        auto r2 = std::make_unique<ReturnStmt>();
        r2->value = std::make_unique<VarExpr>("z");
        static_cast<BlockStmt*>(bad.body.get())->statements.push_back(std::move(r2));

        az.check_function(global, bad);
        std::cout << "NO_ERROR\n";
    } catch (const std::exception& ex) {
        std::cout << "error=" << ex.what() << "\n";
    }

    return 0;
}
