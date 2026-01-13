#pragma once
#include <stdexcept>
#include <string>
#include <vector>

#include "scope.hpp"
#include "../ast/expr.hpp"
#include "../ast/type.hpp"

namespace sem {

struct Analyzer {
    static bool is_lvalue(const ast::Expr& e) {
        return dynamic_cast<const ast::VarExpr*>(&e) != nullptr;
    }

    static ast::Type base_type(ast::Type t) {
        t.is_ref = false;
        return t;
    }

    static std::string type_name(const ast::Type& t) {
        return ast::to_string(t);
    }

    ast::Type type_of_expr(const Scope& scope, const ast::Expr& e) const {
        using namespace ast;

        if (dynamic_cast<const IntLiteral*>(&e)) return Type::Int();
        if (dynamic_cast<const BoolLiteral*>(&e)) return Type::Bool();
        if (dynamic_cast<const CharLiteral*>(&e)) return Type::Char();
        if (dynamic_cast<const StringLiteral*>(&e)) return Type::String();

        if (auto* v = dynamic_cast<const VarExpr*>(&e)) {
            return scope.lookup_var(v->name).type;
        }

        if (auto* a = dynamic_cast<const AssignExpr*>(&e)) {
            const auto& lhs = scope.lookup_var(a->name);
            ast::Type rhs_t = type_of_expr(scope, *a->value);

            if (base_type(lhs.type) != base_type(rhs_t)) {
                throw std::runtime_error("semantic error: assignment type mismatch: " +
                    lhs.name + " is " + type_name(lhs.type) + ", rhs is " + type_name(rhs_t));
            }
            return rhs_t;
        }

        if (auto* u = dynamic_cast<const UnaryExpr*>(&e)) {
            ast::Type t = type_of_expr(scope, *u->expr);

            if (u->op == UnaryExpr::Op::Neg) {
                if (base_type(t) != Type::Int()) throw std::runtime_error("semantic error: unary - expects int");
                return Type::Int();
            }
            if (u->op == UnaryExpr::Op::Not) {
                if (base_type(t) != Type::Bool()) throw std::runtime_error("semantic error: ! expects bool");
                return Type::Bool();
            }
            throw std::runtime_error("semantic error: unknown unary op");
        }

        if (auto* bin = dynamic_cast<const BinaryExpr*>(&e)) {
            ast::Type lt = type_of_expr(scope, *bin->left);
            ast::Type rt = type_of_expr(scope, *bin->right);

            auto L = base_type(lt);
            auto R = base_type(rt);

            if (bin->op == BinaryExpr::Op::Add || bin->op == BinaryExpr::Op::Sub ||
                bin->op == BinaryExpr::Op::Mul || bin->op == BinaryExpr::Op::Div ||
                bin->op == BinaryExpr::Op::Mod) {
                if (L != Type::Int() || R != Type::Int())
                    throw std::runtime_error("semantic error: arithmetic expects int,int");
                return Type::Int();
            }

            if (bin->op == BinaryExpr::Op::AndAnd || bin->op == BinaryExpr::Op::OrOr) {
                if (L != Type::Bool() || R != Type::Bool())
                    throw std::runtime_error("semantic error: &&/|| expects bool,bool");
                return Type::Bool();
            }

            if (bin->op == BinaryExpr::Op::Eq || bin->op == BinaryExpr::Op::Ne) {
                if (L != R) throw std::runtime_error("semantic error: ==/!= require same type");
                if (L != Type::Int() && L != Type::Char() && L != Type::Bool() && L != Type::String())
                    throw std::runtime_error("semantic error: ==/!= unsupported type");
                return Type::Bool();
            }

            if (bin->op == BinaryExpr::Op::Lt || bin->op == BinaryExpr::Op::Le ||
                bin->op == BinaryExpr::Op::Gt || bin->op == BinaryExpr::Op::Ge) {
                if (L != R) throw std::runtime_error("semantic error: relational ops require same type");
                if (L != Type::Int() && L != Type::Char())
                    throw std::runtime_error("semantic error: relational ops require int or char");
                return Type::Bool();
            }

            throw std::runtime_error("semantic error: unknown binary op");
        }

        if (auto* call = dynamic_cast<const CallExpr*>(&e)) {
            if (call->callee == "print_int")    { check_builtin(scope, *call, ast::Type::Int());    return ast::Type::Int(); }
            if (call->callee == "print_bool")   { check_builtin(scope, *call, ast::Type::Bool());   return ast::Type::Int(); }
            if (call->callee == "print_char")   { check_builtin(scope, *call, ast::Type::Char());   return ast::Type::Int(); }
            if (call->callee == "print_string") { check_builtin(scope, *call, ast::Type::String()); return ast::Type::Int(); }

            const FuncSymbol& f = resolve_call(scope, *call);
            return f.return_type;
        }

        throw std::runtime_error("semantic error: unknown expression node");
    }

    // Key change: argument types for overload resolution:
    // lvalue -> T& ; rvalue -> T
    std::vector<ast::Type> arg_types_for_call(const Scope& scope, const ast::CallExpr& call) const {
        std::vector<ast::Type> arg_types;
        arg_types.reserve(call.args.size());

        for (const auto& arg : call.args) {
            ast::Type t = type_of_expr(scope, *arg);
            t.is_ref = is_lvalue(*arg); // mark lvalues as ref for exact signature matching
            arg_types.push_back(t);
        }
        return arg_types;
    }

    const FuncSymbol& resolve_call(const Scope& scope, const ast::CallExpr& call) const {
        // exact match including & marker (using arg_types_for_call)
        auto arg_types = arg_types_for_call(scope, call);
        return scope.resolve_func(call.callee, arg_types);
    }

    void check_builtin(const Scope& scope, const ast::CallExpr& call, const ast::Type& expected) const {
        if (call.args.size() != 1) throw std::runtime_error("semantic error: " + call.callee + " expects 1 arg");
        ast::Type t = type_of_expr(scope, *call.args[0]);
        if (base_type(t) != base_type(expected))
            throw std::runtime_error("semantic error: " + call.callee + " expects " + type_name(expected));
    }
};

} // namespace sem
