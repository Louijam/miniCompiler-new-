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

struct CharLiteral : Expr {
    char value;
    explicit CharLiteral(char v) : value(v) {}
};

struct StringLiteral : Expr {
    std::string value;
    explicit StringLiteral(std::string v) : value(std::move(v)) {}
};

struct VarExpr : Expr {
    std::string name;
    explicit VarExpr(std::string n) : name(std::move(n)) {}
};

// Assignment to variable only. Field assignment is represented by FieldAssignExpr.
struct AssignExpr : Expr {
    std::string name;
    ExprPtr value;
};

// NEW: obj.f = expr
struct FieldAssignExpr : Expr {
    ExprPtr object;
    std::string field;
    ExprPtr value;
};

struct UnaryExpr : Expr {
    enum class Op { Neg, Not } op;
    ExprPtr expr;
};

struct BinaryExpr : Expr {
    enum class Op {
        Add, Sub, Mul, Div, Mod,
        Lt, Le, Gt, Ge,
        Eq, Ne,
        AndAnd, OrOr
    } op;
    ExprPtr left;
    ExprPtr right;
};

struct CallExpr : Expr {
    std::string callee;
    std::vector<ExprPtr> args;
};

// NEW: T(args) - object construction expression (used for: T x = T(args);)
struct ConstructExpr : Expr {
    std::string class_name;
    std::vector<ExprPtr> args;
};

// obj.f
struct MemberAccessExpr : Expr {
    ExprPtr object;
    std::string field;
};

// obj.m(args)
struct MethodCallExpr : Expr {
    ExprPtr object;
    std::string method;
    std::vector<ExprPtr> args;
};

} // namespace ast
