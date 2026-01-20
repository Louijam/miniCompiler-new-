#pragma once
#include <iostream>
#include <stdexcept>
#include "env.hpp"
#include "functions.hpp"
#include "../ast/expr.hpp"
#include "../ast/stmt.hpp"

namespace interp {

struct ReturnSignal { Value value; };

inline bool to_bool(const Value& v){
    if (auto* b=std::get_if<bool>(&v)) return *b;
    if (auto* i=std::get_if<int>(&v)) return *i!=0;
    if (auto* c=std::get_if<char>(&v)) return *c!='\0';
    if (auto* s=std::get_if<std::string>(&v)) return !s->empty();
    throw std::runtime_error("not bool-convertible");
}

Value eval_expr(Env&, const ast::Expr&, FunctionTable&);

void exec_stmt(Env& env, const ast::Stmt& s, FunctionTable& ft){
    using namespace ast;

    if (auto* b=dynamic_cast<const BlockStmt*>(&s)){
        Env inner(&env);
        for (auto& st:b->statements) exec_stmt(inner,*st,ft);
        return;
    }

    if (auto* v=dynamic_cast<const VarDeclStmt*>(&s)){
        Value init{};
        if (v->init) init = eval_expr(env,*v->init,ft);
        env.define_value(v->name, init);
        return;
    }

    if (auto* e=dynamic_cast<const ExprStmt*>(&s)){
        eval_expr(env,*e->expr,ft); return;
    }

    if (auto* i=dynamic_cast<const IfStmt*>(&s)){
        if (to_bool(eval_expr(env,*i->cond,ft)))
            exec_stmt(env,*i->then_branch,ft);
        else if (i->else_branch)
            exec_stmt(env,*i->else_branch,ft);
        return;
    }

    if (auto* w=dynamic_cast<const WhileStmt*>(&s)){
        while (to_bool(eval_expr(env,*w->cond,ft)))
            exec_stmt(env,*w->body,ft);
        return;
    }

    if (auto* r=dynamic_cast<const ReturnStmt*>(&s)){
        Value v{};
        if (r->value) v = eval_expr(env,*r->value,ft);
        throw ReturnSignal{v};
    }

    throw std::runtime_error("unknown stmt");
}

Value eval_expr(Env& env, const ast::Expr& e, FunctionTable& ft){
    using namespace ast;

    if (auto* i=dynamic_cast<const IntLiteral*>(&e)) return i->value;
    if (auto* b=dynamic_cast<const BoolLiteral*>(&e)) return b->value;
    if (auto* c=dynamic_cast<const CharLiteral*>(&e)) return c->value;
    if (auto* s=dynamic_cast<const StringLiteral*>(&e)) return s->value;

    if (auto* v=dynamic_cast<const VarExpr*>(&e)) return env.read_value(v->name);

    if (auto* a=dynamic_cast<const AssignExpr*>(&e)){
        Value rhs = eval_expr(env,*a->value,ft);
        env.assign_value(a->name,rhs);
        return rhs;
    }

    if (auto* mc=dynamic_cast<const MethodCallExpr*>(&e)){
        auto objv = eval_expr(env,*mc->object,ft);
        auto obj = std::get<ObjectPtr>(objv);

        std::vector<ast::Type> arg_types;
        for (auto& a2: mc->args) arg_types.push_back(ast::Type::Int()); // placeholder

        auto& f = ft.resolve(obj->class_name + "::" + mc->method, arg_types);

        Env call_env(&env);
        call_env.define_value("this", obj);

        try {
            exec_stmt(call_env,*f.body,ft);
        } catch (ReturnSignal& r) {
            return r.value;
        }
        return Value{0};
    }

    if (auto* c=dynamic_cast<const CallExpr*>(&e)){
        auto& f = ft.resolve(c->callee,{});
        Env call_env(&env);
        try {
            exec_stmt(call_env,*f.body,ft);
        } catch (ReturnSignal& r){
            return r.value;
        }
        return Value{0};
    }

    throw std::runtime_error("unknown expr");
}

} // namespace interp
