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

struct BoolLiteral : Expr {
    bool value;
    explicit BoolLiteral(bool v) : value(v) {}
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

struct UnaryExpr : Expr {
    enum class Op { Neg, Not };
    Op op;
    ExprPtr expr;
};

struct BinaryExpr : Expr {
    enum class Op {
        Add, Sub, Mul, Div, Mod,
        Eq, Ne, Lt, Le, Gt, Ge,
        AndAnd, OrOr
    };
    Op op;
    ExprPtr left;
    ExprPtr right;
};

} // namespace ast
