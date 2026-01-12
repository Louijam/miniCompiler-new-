#pragma once
#include <stdexcept>
#include <vector>

#include "env.hpp"
#include "functions.hpp"
#include "../ast/stmt.hpp"
#include "../ast/expr.hpp"
#include "../ast/type.hpp"

namespace interp {

struct ReturnSignal {
    Value value;
};

inline ast::Type value_to_type(const Value& v) {
    if (std::holds_alternative<int>(v)) return ast::Type::Int();
    if (std::holds_alternative<bool>(v)) return ast::Type::Bool();
    if (std::holds_alternative<char>(v)) return ast::Type::Char();
    if (std::holds_alternative<std::string>(v)) return ast::Type::String();
    throw std::runtime_error("unsupported runtime value type");
}

inline bool to_bool_like_cpp(const Value& v) {
    if (auto* pi = std::get_if<int>(&v)) return *pi != 0;
    if (auto* pb = std::get_if<bool>(&v)) return *pb;
    if (auto* pc = std::get_if<char>(&v)) return *pc != '\0';
    if (auto* ps = std::get_if<std::string>(&v)) return !ps->empty();
    throw std::runtime_error("cannot convert to bool");
}

inline int expect_int(const Value& v, const char* ctx) {
    if (auto* pi = std::get_if<int>(&v)) return *pi;
    throw std::runtime_error(std::string("type error: expected int in ") + ctx);
}

inline bool expect_bool(const Value& v, const char* ctx) {
    if (auto* pb = std::get_if<bool>(&v)) return *pb;
    throw std::runtime_error(std::string("type error: expected bool in ") + ctx);
}

inline Value eval_expr(Env& env, const ast::Expr& e, FunctionTable& functions);

inline LValue eval_lvalue(Env& env, const ast::Expr& e) {
    using namespace ast;
    if (auto* v = dynamic_cast<const VarExpr*>(&e)) return env.resolve_lvalue(v->name);
    throw std::runtime_error("expected lvalue (variable or field)");
}

inline ast::Type eval_arg_type(Env& env, const ast::Expr& e, FunctionTable& functions) {
    try {
        LValue lv = eval_lvalue(env, e);
        Value vv = env.read_lvalue(lv);
        ast::Type t = value_to_type(vv);
        t.is_ref = true;
        return t;
    } catch (...) {
        Value vv = eval_expr(env, e, functions);
        return value_to_type(vv);
    }
}

inline void exec_stmt(Env& env, const ast::Stmt& s, FunctionTable& functions) {
    using namespace ast;

    if (auto* b = dynamic_cast<const BlockStmt*>(&s)) {
        Env local(&env);
        for (auto& st : b->statements) exec_stmt(local, *st, functions);
        return;
    }

    if (auto* v = dynamic_cast<const VarDeclStmt*>(&s)) {
        Value init = 0;
        if (v->init) init = eval_expr(env, *v->init, functions);
        env.define_value(v->name, init);
        return;
    }

    if (auto* e = dynamic_cast<const ExprStmt*>(&s)) {
        eval_expr(env, *e->expr, functions);
        return;
    }

    if (auto* i = dynamic_cast<const IfStmt*>(&s)) {
        bool cond = to_bool_like_cpp(eval_expr(env, *i->cond, functions));
        if (cond) exec_stmt(env, *i->then_branch, functions);
        else if (i->else_branch) exec_stmt(env, *i->else_branch, functions);
        return;
    }

    if (auto* w = dynamic_cast<const WhileStmt*>(&s)) {
        while (to_bool_like_cpp(eval_expr(env, *w->cond, functions))) exec_stmt(env, *w->body, functions);
        return;
    }

    if (auto* r = dynamic_cast<const ReturnStmt*>(&s)) {
        Value v = r->value ? eval_expr(env, *r->value, functions) : Value{0};
        throw ReturnSignal{v};
    }

    throw std::runtime_error("unknown statement");
}

inline Value eval_expr(Env& env, const ast::Expr& e, FunctionTable& functions) {
    using namespace ast;

    if (auto* i = dynamic_cast<const IntLiteral*>(&e)) return i->value;
    if (auto* b = dynamic_cast<const BoolLiteral*>(&e)) return b->value;
    if (auto* c = dynamic_cast<const CharLiteral*>(&e)) return c->value;
    if (auto* s = dynamic_cast<const StringLiteral*>(&e)) return s->value;

    if (auto* v = dynamic_cast<const VarExpr*>(&e)) return env.read_value(v->name);

    if (auto* a = dynamic_cast<const AssignExpr*>(&e)) {
        Value rhs = eval_expr(env, *a->value, functions);
        env.assign_value(a->name, rhs);
        return rhs;
    }

    if (auto* u = dynamic_cast<const UnaryExpr*>(&e)) {
        Value x = eval_expr(env, *u->expr, functions);
        if (u->op == UnaryExpr::Op::Neg) return -expect_int(x, "unary -");
        if (u->op == UnaryExpr::Op::Not) return !expect_bool(x, "unary !");
        throw std::runtime_error("unknown unary op");
    }

    if (auto* bin = dynamic_cast<const BinaryExpr*>(&e)) {
        // && and || : only bool, short-circuit
        if (bin->op == BinaryExpr::Op::AndAnd) {
            bool lv = expect_bool(eval_expr(env, *bin->left, functions), "operator && (lhs)");
            if (!lv) return false;
            bool rv = expect_bool(eval_expr(env, *bin->right, functions), "operator && (rhs)");
            return rv;
        }
        if (bin->op == BinaryExpr::Op::OrOr) {
            bool lv = expect_bool(eval_expr(env, *bin->left, functions), "operator || (lhs)");
            if (lv) return true;
            bool rv = expect_bool(eval_expr(env, *bin->right, functions), "operator || (rhs)");
            return rv;
        }

        Value lv = eval_expr(env, *bin->left, functions);
        Value rv = eval_expr(env, *bin->right, functions);

        // arithmetic: only int
        if (bin->op == BinaryExpr::Op::Add || bin->op == BinaryExpr::Op::Sub ||
            bin->op == BinaryExpr::Op::Mul || bin->op == BinaryExpr::Op::Div ||
            bin->op == BinaryExpr::Op::Mod) {

            int li = expect_int(lv, "arithmetic");
            int ri = expect_int(rv, "arithmetic");

            switch (bin->op) {
                case BinaryExpr::Op::Add: return li + ri;
                case BinaryExpr::Op::Sub: return li - ri;
                case BinaryExpr::Op::Mul: return li * ri;
                case BinaryExpr::Op::Div:
                    if (ri == 0) throw std::runtime_error("runtime error: division by zero");
                    return li / ri;
                case BinaryExpr::Op::Mod:
                    if (ri == 0) throw std::runtime_error("runtime error: modulo by zero");
                    return li % ri;
                default: break;
            }
        }

        // equality: same type; bool/string only == !=; int/char also ok
        if (bin->op == BinaryExpr::Op::Eq || bin->op == BinaryExpr::Op::Ne) {
            bool eq = (bin->op == BinaryExpr::Op::Eq);

            if (std::holds_alternative<int>(lv) && std::holds_alternative<int>(rv)) {
                return eq ? (std::get<int>(lv) == std::get<int>(rv)) : (std::get<int>(lv) != std::get<int>(rv));
            }
            if (std::holds_alternative<char>(lv) && std::holds_alternative<char>(rv)) {
                return eq ? (std::get<char>(lv) == std::get<char>(rv)) : (std::get<char>(lv) != std::get<char>(rv));
            }
            if (std::holds_alternative<bool>(lv) && std::holds_alternative<bool>(rv)) {
                return eq ? (std::get<bool>(lv) == std::get<bool>(rv)) : (std::get<bool>(lv) != std::get<bool>(rv));
            }
            if (std::holds_alternative<std::string>(lv) && std::holds_alternative<std::string>(rv)) {
                return eq ? (std::get<std::string>(lv) == std::get<std::string>(rv))
                          : (std::get<std::string>(lv) != std::get<std::string>(rv));
            }

            throw std::runtime_error("type error: ==/!= require same type (int/char/bool/string)");
        }

        // relational: only int or char (same type)
        if (bin->op == BinaryExpr::Op::Lt || bin->op == BinaryExpr::Op::Le ||
            bin->op == BinaryExpr::Op::Gt || bin->op == BinaryExpr::Op::Ge) {

            auto cmp_int = [&](int a, int b) -> bool {
                switch (bin->op) {
                    case BinaryExpr::Op::Lt: return a < b;
                    case BinaryExpr::Op::Le: return a <= b;
                    case BinaryExpr::Op::Gt: return a > b;
                    case BinaryExpr::Op::Ge: return a >= b;
                    default: return false;
                }
            };

            if (std::holds_alternative<int>(lv) && std::holds_alternative<int>(rv)) {
                return cmp_int(std::get<int>(lv), std::get<int>(rv));
            }
            if (std::holds_alternative<char>(lv) && std::holds_alternative<char>(rv)) {
                return cmp_int(static_cast<int>(std::get<char>(lv)), static_cast<int>(std::get<char>(rv)));
            }

            throw std::runtime_error("type error: < <= > >= require int or char (same type)");
        }

        throw std::runtime_error("unknown binary op");
    }

    if (auto* c = dynamic_cast<const CallExpr*>(&e)) {
        std::vector<Type> arg_types;
        arg_types.reserve(c->args.size());
        for (auto& arg : c->args) arg_types.push_back(eval_arg_type(env, *arg, functions));

        auto& f = functions.resolve(c->callee, arg_types);

        Env call_env(&env);

        for (size_t i = 0; i < f.params.size(); ++i) {
            const auto& p = f.params[i];
            if (p.type.is_ref) {
                LValue target = eval_lvalue(env, *c->args[i]);
                call_env.define_ref(p.name, target);
            } else {
                Value v = eval_expr(env, *c->args[i], functions);
                call_env.define_value(p.name, v);
            }
        }

        try {
            exec_stmt(call_env, *f.body, functions);
        } catch (ReturnSignal& r) {
            return r.value;
        }

        return Value{0};
    }

    throw std::runtime_error("unknown expression");
}

} // namespace interp
