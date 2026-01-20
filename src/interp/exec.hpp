#pragma once
#include <stdexcept>
#include <vector>
#include <iostream>

#include "env.hpp"
#include "functions.hpp"
#include "lvalue.hpp"
#include "object.hpp"
#include "../ast/stmt.hpp"
#include "../ast/expr.hpp"
#include "../ast/type.hpp"

namespace interp {

struct ReturnSignal {
    Value value;
};

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

inline LValue eval_lvalue(Env& env, const ast::Expr& e, FunctionTable& functions) {
    using namespace ast;

    if (auto* v = dynamic_cast<const VarExpr*>(&e))
        return env.resolve_lvalue(v->name);

    if (auto* m = dynamic_cast<const MemberAccessExpr*>(&e)) {
        Value objv = eval_expr(env, *m->object, functions);
        auto* pobj = std::get_if<ObjectPtr>(&objv);
        if (!pobj || !*pobj)
            throw std::runtime_error("member access on non-object");
        return LValue::field_of(*pobj, m->field);
    }

    throw std::runtime_error("expected lvalue");
}

inline void exec_stmt(Env& env, const ast::Stmt& s, FunctionTable& functions) {
    using namespace ast;

    if (auto* b = dynamic_cast<const BlockStmt*>(&s)) {
        Env local(&env);
        for (auto& st : b->statements)
            exec_stmt(local, *st, functions);
        return;
    }

    if (auto* v = dynamic_cast<const VarDeclStmt*>(&s)) {
        const ast::Type& t = v->decl_type;

        if (t.is_ref) {
            if (!v->init)
                throw std::runtime_error("Referenzvariable muss initialisiert werden");
            LValue target = eval_lvalue(env, *v->init, functions);
            env.define_ref(v->name, target, t);
        } else {
            Value init = v->init ? eval_expr(env, *v->init, functions) : Value{0};
            env.define_value(v->name, init, t);
        }
        return;
    }

    if (auto* e = dynamic_cast<const ExprStmt*>(&s)) {
        eval_expr(env, *e->expr, functions);
        return;
    }

    if (auto* i = dynamic_cast<const IfStmt*>(&s)) {
        bool cond = to_bool_like_cpp(eval_expr(env, *i->cond, functions));
        if (cond)
            exec_stmt(env, *i->then_branch, functions);
        else if (i->else_branch)
            exec_stmt(env, *i->else_branch, functions);
        return;
    }

    if (auto* w = dynamic_cast<const WhileStmt*>(&s)) {
        while (to_bool_like_cpp(eval_expr(env, *w->cond, functions)))
            exec_stmt(env, *w->body, functions);
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

    if (auto* v = dynamic_cast<const VarExpr*>(&e))
        return env.read_value(v->name);

    if (auto* a = dynamic_cast<const AssignExpr*>(&e)) {
        Value rhs = eval_expr(env, *a->value, functions);
        env.assign_value(a->name, rhs);
        return rhs;
    }

    if (auto* fa = dynamic_cast<const FieldAssignExpr*>(&e)) {
        LValue lv = eval_lvalue(env, e, functions);
        Value rhs = eval_expr(env, *fa->value, functions);
        env.write_lvalue(lv, rhs);
        return rhs;
    }

    if (dynamic_cast<const MemberAccessExpr*>(&e)) {
        LValue lv = eval_lvalue(env, e, functions);
        return env.read_lvalue(lv);
    }

    if (auto* u = dynamic_cast<const UnaryExpr*>(&e)) {
        Value x = eval_expr(env, *u->expr, functions);
        if (u->op == UnaryExpr::Op::Neg) return -expect_int(x, "unary -");
        if (u->op == UnaryExpr::Op::Not) return !expect_bool(x, "unary !");
    }

    if (auto* bin = dynamic_cast<const BinaryExpr*>(&e)) {
        if (bin->op == BinaryExpr::Op::AndAnd) {
            bool lv = expect_bool(eval_expr(env, *bin->left, functions), "&& lhs");
            if (!lv) return false;
            return expect_bool(eval_expr(env, *bin->right, functions), "&& rhs");
        }
        if (bin->op == BinaryExpr::Op::OrOr) {
            bool lv = expect_bool(eval_expr(env, *bin->left, functions), "|| lhs");
            if (lv) return true;
            return expect_bool(eval_expr(env, *bin->right, functions), "|| rhs");
        }

        Value lv = eval_expr(env, *bin->left, functions);
        Value rv = eval_expr(env, *bin->right, functions);

        if (bin->op == BinaryExpr::Op::Add) return expect_int(lv, "+") + expect_int(rv, "+");
        if (bin->op == BinaryExpr::Op::Sub) return expect_int(lv, "-") - expect_int(rv, "-");
        if (bin->op == BinaryExpr::Op::Mul) return expect_int(lv, "*") * expect_int(rv, "*");
        if (bin->op == BinaryExpr::Op::Div) {
            int r = expect_int(rv, "/");
            if (r == 0) throw std::runtime_error("Division durch 0");
            return expect_int(lv, "/") / r;
        }
        if (bin->op == BinaryExpr::Op::Mod) {
            int r = expect_int(rv, "%");
            if (r == 0) throw std::runtime_error("Modulo durch 0");
            return expect_int(lv, "%") % r;
        }
    }

    throw std::runtime_error("unknown expression");
}

} // namespace interp
