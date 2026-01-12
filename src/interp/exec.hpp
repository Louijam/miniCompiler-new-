#pragma once
#include <stdexcept>

#include "env.hpp"
#include "functions.hpp"
#include "../ast/stmt.hpp"
#include "../ast/expr.hpp"

namespace interp {

struct ReturnSignal {
    Value value;
};

inline Value eval_expr(Env& env, const ast::Expr& e, FunctionTable& functions);

inline void exec_stmt(Env& env, const ast::Stmt& s, FunctionTable& functions) {
    using namespace ast;

    if (auto* b = dynamic_cast<const BlockStmt*>(&s)) {
        Env local(&env);
        for (auto& st : b->statements)
            exec_stmt(local, *st, functions);
        return;
    }

    if (auto* v = dynamic_cast<const VarDeclStmt*>(&s)) {
        Value init = 0;
        if (v->init)
            init = eval_expr(env, *v->init, functions);
        env.define_value(v->name, init);
        return;
    }

    if (auto* e = dynamic_cast<const ExprStmt*>(&s)) {
        eval_expr(env, *e->expr, functions);
        return;
    }

    if (auto* i = dynamic_cast<const IfStmt*>(&s)) {
        bool cond = std::get<int>(eval_expr(env, *i->cond, functions)) != 0;
        if (cond) {
            exec_stmt(env, *i->then_branch, functions);
        } else if (i->else_branch) {
            exec_stmt(env, *i->else_branch, functions);
        }
        return;
    }

    if (auto* w = dynamic_cast<const WhileStmt*>(&s)) {
        while (std::get<int>(eval_expr(env, *w->cond, functions)) != 0)
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

    if (auto* i = dynamic_cast<const IntLiteral*>(&e))
        return i->value;

    if (auto* v = dynamic_cast<const VarExpr*>(&e))
        return env.read_value(v->name);

    if (auto* a = dynamic_cast<const AssignExpr*>(&e)) {
        Value rhs = eval_expr(env, *a->value, functions);
        env.assign_value(a->name, rhs);
        return rhs;
    }

    if (auto* c = dynamic_cast<const CallExpr*>(&e)) {
        auto& f = functions.get(c->callee);

        if (f.params.size() != c->args.size())
            throw std::runtime_error("argument count mismatch");

        Env call_env(&env);

        for (size_t i = 0; i < f.params.size(); ++i) {
            Value v = eval_expr(env, *c->args[i], functions);
            call_env.define_value(f.params[i].name, v);
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
