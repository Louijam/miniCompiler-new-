#pragma once
#include <memory>
#include <string>
#include <vector>

namespace ast {

struct Expr {
    virtual ~Expr() = default;
};

using ExprPtr = std::unique_ptr<Expr>;

struct IntLiteral : Expr {
    int value;
    explicit IntLiteral(int v) : value(v) {}
};

struct VarExpr : Expr {
    std::string name;
    explicit VarExpr(std::string n) : name(std::move(n)) {}
};

struct AssignExpr : Expr {
    std::string name;
    ExprPtr value;
};

struct CallExpr : Expr {
    std::string callee;
    std::vector<ExprPtr> args;
};

} // namespace ast
