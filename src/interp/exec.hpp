#pragma once
#include <stdexcept>
#include "env.hpp"
#include "../ast/stmt.hpp"
#include "../ast/expr.hpp"

namespace interp {

struct ReturnSignal {
    Value value;
};

inline Value eval_expr(Env& env, const ast::Expr& e) {
    using namespace ast;

    if (auto* i = dynamic_cast<const IntLiteral*>(&e)) {
        return i->value;
    }
    if (auto* v = dynamic_cast<const VarExpr*>(&e)) {
        return env.read_value(v->name);
    }
    if (auto* a = dynamic_cast<const AssignExpr*>(&e)) {
        Value rhs = eval_expr(env, *a->value);
        env.assign_value(a->name, rhs);
        return rhs;
    }
    throw std::runtime_error("unknown expression");
}

inline void exec_stmt(Env& env, const ast::Stmt& s) {
    using namespace ast;

    if (auto* b = dynamic_cast<const BlockStmt*>(&s)) {
        Env local(&env);
        for (auto& st : b->statements) {
            exec_stmt(local, *st);
        }
        return;
    }

    if (auto* v = dynamic_cast<const VarDeclStmt*>(&s)) {
        Value init = 0;
        if (v->init) init = eval_expr(env, *v->init);
        env.define_value(v->name, init);
        return;
    }

    if (auto* e = dynamic_cast<const ExprStmt*>(&s)) {
        eval_expr(env, *e->expr);
        return;
    }

    if (auto* i = dynamic_cast<const IfStmt*>(&s)) {
        Value c = eval_expr(env, *i->cond);
        bool cond = std::get<int>(c) != 0;
        if (cond) exec_stmt(env, *i->then_branch);
        else if (i->else_branch) exec_stmt(env, *i->else_branch);
        return;
    }

    if (auto* w = dynamic_cast<const WhileStmt*>(&s)) {
        while (true) {
            Value c = eval_expr(env, *w->cond);
            if (std::get<int>(c) == 0) break;
            exec_stmt(env, *w->body);
        }
        return;
    }

    if (auto* r = dynamic_cast<const ReturnStmt*>(&s)) {
        Value v = r->value ? eval_expr(env, *r->value) : Value{0};
        throw ReturnSignal{v};
    }

    throw std::runtime_error("unknown statement");
}

} // namespace interp
