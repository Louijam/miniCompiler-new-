#pragma once
#include <memory>
#include <vector>
#include <optional>
#include <string>

#include "type.hpp"

namespace ast {

struct Stmt {
    virtual ~Stmt() = default;
};

using StmtPtr = std::unique_ptr<Stmt>;

struct BlockStmt : Stmt {
    std::vector<StmtPtr> statements;
};

struct Expr; // forward

struct ExprStmt : Stmt {
    std::unique_ptr<Expr> expr;
};

struct VarDeclStmt : Stmt {
    Type decl_type;           // <-- NEU: deklarierter Typ (z.B. int, bool, T&, ...)
    std::string name;
    std::unique_ptr<Expr> init; // optional initializer expression
};

struct IfStmt : Stmt {
    std::unique_ptr<Expr> cond;
    StmtPtr then_branch;
    StmtPtr else_branch; // may be null
};

struct WhileStmt : Stmt {
    std::unique_ptr<Expr> cond;
    StmtPtr body;
};

struct ReturnStmt : Stmt {
    std::unique_ptr<Expr> value; // may be null (void)
};

} // namespace ast
