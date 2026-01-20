#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>

#include "repl/repl.hpp"

#include "sem/program_analyzer.hpp"
#include "ast/program.hpp"
#include "ast/expr.hpp"
#include "ast/stmt.hpp"
#include "ast/type.hpp"
#include "ast/function.hpp"

static std::unique_ptr<ast::CallExpr> make_call(const std::string& name, std::vector<ast::ExprPtr> args) {
    auto c = std::make_unique<ast::CallExpr>();
    c->callee = name;
    c->args = std::move(args);
    return c;
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

static int run_sema_selftest() {
    using namespace ast;

    // Build minimal program:
    // int main() { print_int(1); return 0; }
    Program p;

    FunctionDef f;
    f.name = "main";
    f.return_type = Type::Int(false);

    std::vector<std::unique_ptr<Stmt>> body;

    {
        std::vector<ExprPtr> args;
        args.push_back(std::make_unique<IntLiteral>(1));
        body.push_back(make_exprstmt(make_call("print_int", std::move(args))));
    }
    body.push_back(make_return(std::make_unique<IntLiteral>(0)));

    f.body = make_block(std::move(body));
    p.functions.push_back(std::move(f));

    sem::ProgramAnalyzer pa;
    pa.analyze(p);

    std::cout << "SEMA SELFTEST OK\n";
    return 0;
}

int main(int argc, char** argv) {
    try {
        if (argc > 1) {
            std::string arg = argv[1];
            if (arg == "--selftest-sema") {
                return run_sema_selftest();
            }
        }
        return repl::run_repl();
    } catch (const std::exception& ex) {
        std::cerr << "FEHLER: " << ex.what() << "\n";
        return 1;
    }
}
