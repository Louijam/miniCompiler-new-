#pragma once
#include <stdexcept>
#include <vector>
#include <iostream>

#include "env.hpp"
#include "functions.hpp"
#include "class_runtime.hpp"
#include "../ast/stmt.hpp"
#include "../ast/expr.hpp"
#include "../ast/type.hpp"

namespace interp {

struct ReturnSignal { Value value; };

inline ast::Type value_to_type(const Value& v) {
    if (std::holds_alternative<int>(v)) return ast::Type::Int();
    if (std::holds_alternative<bool>(v)) return ast::Type::Bool();
    if (std::holds_alternative<char>(v)) return ast::Type::Char();
    if (std::holds_alternative<std::string>(v)) return ast::Type::String();
    if (std::holds_alternative<ObjectPtr>(v)) return ast::Type::Class(std::get<ObjectPtr>(v)->class_name);
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

inline char expect_char(const Value& v, const char* ctx) {
    if (auto* pc = std::get_if<char>(&v)) return *pc;
    throw std::runtime_error(std::string("type error: expected char in ") + ctx);
}

inline const std::string& expect_string(const Value& v, const char* ctx) {
    if (auto* ps = std::get_if<std::string>(&v)) return *ps;
    throw std::runtime_error(std::string("type error: expected string in ") + ctx);
}

inline Value eval_expr(Env& env, const ast::Expr& e, FunctionTable& functions, const ClassRuntime& rt);

inline LValue eval_lvalue(Env& env, const ast::Expr& e, FunctionTable& functions, const ClassRuntime& rt) {
    using namespace ast;
    (void)functions; (void)rt;

    if (auto* v = dynamic_cast<const VarExpr*>(&e)) return env.resolve_lvalue(v->name);
    throw std::runtime_error("expected lvalue (variable)");
}

inline ast::Type eval_arg_type(Env& env, const ast::Expr& e, FunctionTable& functions, const ClassRuntime& rt) {
    try {
        LValue lv = eval_lvalue(env, e, functions, rt);
        Value vv = env.read_lvalue(lv);
        ast::Type t = value_to_type(vv);
        t.is_ref = true;
        return t;
    } catch (...) {
        Value vv = eval_expr(env, e, functions, rt);
        return value_to_type(vv);
    }
}

inline void exec_stmt(Env& env, const ast::Stmt& s, FunctionTable& functions, const ClassRuntime& rt) {
    using namespace ast;

    if (auto* b = dynamic_cast<const BlockStmt*>(&s)) {
        Env local(&env);
        for (auto& st : b->statements) exec_stmt(local, *st, functions, rt);
        return;
    }

    if (auto* v = dynamic_cast<const VarDeclStmt*>(&s)) {
        if (v->decl_type.is_ref) {
            if (!v->init) throw std::runtime_error("runtime error: reference variable requires initializer: " + v->name);
            LValue target = eval_lvalue(env, *v->init, functions, rt);
            env.define_ref_typed(v->name, v->decl_type, target);
            return;
        }

        Value init = 0;

        if (v->decl_type.base == ast::Type::Base::Class) {
            auto o = std::make_shared<Object>();
            o->class_name = v->decl_type.class_name;
            init = o;
        }

        if (v->init) init = eval_expr(env, *v->init, functions, rt);

        env.define_value_typed(v->name, v->decl_type, init);
        return;
    }

    if (auto* e = dynamic_cast<const ExprStmt*>(&s)) {
        eval_expr(env, *e->expr, functions, rt);
        return;
    }

    if (auto* i = dynamic_cast<const IfStmt*>(&s)) {
        bool cond = to_bool_like_cpp(eval_expr(env, *i->cond, functions, rt));
        if (cond) exec_stmt(env, *i->then_branch, functions, rt);
        else if (i->else_branch) exec_stmt(env, *i->else_branch, functions, rt);
        return;
    }

    if (auto* w = dynamic_cast<const WhileStmt*>(&s)) {
        while (to_bool_like_cpp(eval_expr(env, *w->cond, functions, rt))) exec_stmt(env, *w->body, functions, rt);
        return;
    }

    if (auto* r = dynamic_cast<const ReturnStmt*>(&s)) {
        Value v = r->value ? eval_expr(env, *r->value, functions, rt) : Value{0};
        throw ReturnSignal{v};
    }

    throw std::runtime_error("unknown statement");
}

inline Value eval_expr(Env& env, const ast::Expr& e, FunctionTable& functions, const ClassRuntime& rt) {
    using namespace ast;

    if (auto* i = dynamic_cast<const IntLiteral*>(&e)) return i->value;
    if (auto* b = dynamic_cast<const BoolLiteral*>(&e)) return b->value;
    if (auto* c = dynamic_cast<const CharLiteral*>(&e)) return c->value;
    if (auto* s = dynamic_cast<const StringLiteral*>(&e)) return s->value;

    if (auto* v = dynamic_cast<const VarExpr*>(&e)) return env.read_value(v->name);

    if (auto* a = dynamic_cast<const AssignExpr*>(&e)) {
        Value rhs = eval_expr(env, *a->value, functions, rt);
        env.assign_value(a->name, rhs);
        return rhs;
    }

    if (auto* u = dynamic_cast<const UnaryExpr*>(&e)) {
        Value x = eval_expr(env, *u->expr, functions, rt);
        if (u->op == UnaryExpr::Op::Neg) return -expect_int(x, "unary -");
        if (u->op == UnaryExpr::Op::Not) return !expect_bool(x, "unary !");
        throw std::runtime_error("unknown unary op");
    }

    if (auto* bin = dynamic_cast<const BinaryExpr*>(&e)) {
        if (bin->op == BinaryExpr::Op::AndAnd) {
            bool lv = expect_bool(eval_expr(env, *bin->left, functions, rt), "operator && (lhs)");
            if (!lv) return false;
            bool rv = expect_bool(eval_expr(env, *bin->right, functions, rt), "operator && (rhs)");
            return rv;
        }
        if (bin->op == BinaryExpr::Op::OrOr) {
            bool lv = expect_bool(eval_expr(env, *bin->left, functions, rt), "operator || (lhs)");
            if (lv) return true;
            bool rv = expect_bool(eval_expr(env, *bin->right, functions, rt), "operator || (rhs)");
            return rv;
        }

        Value lv = eval_expr(env, *bin->left, functions, rt);
        Value rv = eval_expr(env, *bin->right, functions, rt);

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

        if (bin->op == BinaryExpr::Op::Eq || bin->op == BinaryExpr::Op::Ne) {
            bool eq = (bin->op == BinaryExpr::Op::Eq);

            if (std::holds_alternative<int>(lv) && std::holds_alternative<int>(rv))
                return eq ? (std::get<int>(lv) == std::get<int>(rv)) : (std::get<int>(lv) != std::get<int>(rv));
            if (std::holds_alternative<char>(lv) && std::holds_alternative<char>(rv))
                return eq ? (std::get<char>(lv) == std::get<char>(rv)) : (std::get<char>(lv) != std::get<char>(rv));
            if (std::holds_alternative<bool>(lv) && std::holds_alternative<bool>(rv))
                return eq ? (std::get<bool>(lv) == std::get<bool>(rv)) : (std::get<bool>(lv) != std::get<bool>(rv));
            if (std::holds_alternative<std::string>(lv) && std::holds_alternative<std::string>(rv))
                return eq ? (std::get<std::string>(lv) == std::get<std::string>(rv))
                          : (std::get<std::string>(lv) != std::get<std::string>(rv));

            throw std::runtime_error("type error: ==/!= require same type (int/char/bool/string)");
        }

        if (bin->op == BinaryExpr::Op::Lt || bin->op == BinaryExpr::Op::Le ||
            bin->op == BinaryExpr::Op::Gt || bin->op == BinaryExpr::Op::Ge) {

            auto cmp_int = [&](int a2, int b2) -> bool {
                switch (bin->op) {
                    case BinaryExpr::Op::Lt: return a2 < b2;
                    case BinaryExpr::Op::Le: return a2 <= b2;
                    case BinaryExpr::Op::Gt: return a2 > b2;
                    case BinaryExpr::Op::Ge: return a2 >= b2;
                    default: return false;
                }
            };

            if (std::holds_alternative<int>(lv) && std::holds_alternative<int>(rv))
                return cmp_int(std::get<int>(lv), std::get<int>(rv));

            if (std::holds_alternative<char>(lv) && std::holds_alternative<char>(rv))
                return cmp_int(static_cast<int>(std::get<char>(lv)), static_cast<int>(std::get<char>(rv)));

            throw std::runtime_error("type error: < <= > >= require int or char (same type)");
        }

        throw std::runtime_error("unknown binary op");
    }

    if (auto* mc = dynamic_cast<const MethodCallExpr*>(&e)) {
        Value ov = eval_expr(env, *mc->object, functions, rt);
        if (!std::holds_alternative<ObjectPtr>(ov)) throw std::runtime_error("runtime error: method call on non-object");
        ObjectPtr obj = std::get<ObjectPtr>(ov);

        // static receiver type + via-ref?
        std::string static_class;
        bool call_via_ref = false;

        if (auto* recv = dynamic_cast<const VarExpr*>(mc->object.get())) {
            ast::Type t = env.lookup_type(recv->name);
            call_via_ref = t.is_ref;
            if (t.base != ast::Type::Base::Class) throw std::runtime_error("runtime error: receiver not class type");
            static_class = t.class_name;
        } else {
            // fallback: treat as value (no dyn dispatch)
            ast::Type t = value_to_type(ov);
            if (t.base != ast::Type::Base::Class) throw std::runtime_error("runtime error: receiver not class type");
            static_class = t.class_name;
            call_via_ref = false;
        }

        std::vector<ast::Type> arg_types;
        arg_types.reserve(mc->args.size());
        for (auto& a2 : mc->args) arg_types.push_back(eval_arg_type(env, *a2, functions, rt));

        std::string impl_class = rt.resolve_impl_class(static_class, obj->class_name, mc->method, arg_types, call_via_ref);
        std::string callee = impl_class + "::" + mc->method;

        auto& f = functions.resolve(callee, arg_types);

        Env call_env(&env);
        call_env.define_value_typed("this", ast::Type::Class(obj->class_name, true), obj);

        for (size_t i = 0; i < f.params.size(); ++i) {
            const auto& p = f.params[i];
            if (p.type.is_ref) {
                LValue target = eval_lvalue(env, *mc->args[i], functions, rt);
                call_env.define_ref_typed(p.name, p.type, target);
            } else {
                Value v2 = eval_expr(env, *mc->args[i], functions, rt);
                call_env.define_value_typed(p.name, p.type, v2);
            }
        }

        try {
            exec_stmt(call_env, *f.body, functions, rt);
        } catch (ReturnSignal& r) {
            return r.value;
        }
        return Value{0};
    }

    if (auto* c = dynamic_cast<const CallExpr*>(&e)) {
        if (c->callee == "print_int") {
            if (c->args.size() != 1) throw std::runtime_error("print_int expects 1 argument");
            Value v2 = eval_expr(env, *c->args[0], functions, rt);
            std::cout << expect_int(v2, "print_int") << "\n";
            return Value{0};
        }
        if (c->callee == "print_bool") {
            if (c->args.size() != 1) throw std::runtime_error("print_bool expects 1 argument");
            Value v2 = eval_expr(env, *c->args[0], functions, rt);
            std::cout << (expect_bool(v2, "print_bool") ? "true" : "false") << "\n";
            return Value{0};
        }
        if (c->callee == "print_char") {
            if (c->args.size() != 1) throw std::runtime_error("print_char expects 1 argument");
            Value v2 = eval_expr(env, *c->args[0], functions, rt);
            std::cout << expect_char(v2, "print_char") << "\n";
            return Value{0};
        }
        if (c->callee == "print_string") {
            if (c->args.size() != 1) throw std::runtime_error("print_string expects 1 argument");
            Value v2 = eval_expr(env, *c->args[0], functions, rt);
            std::cout << expect_string(v2, "print_string") << "\n";
            return Value{0};
        }

        std::vector<ast::Type> arg_types;
        arg_types.reserve(c->args.size());
        for (auto& arg : c->args) arg_types.push_back(eval_arg_type(env, *arg, functions, rt));

        auto& f = functions.resolve(c->callee, arg_types);

        Env call_env(&env);

        for (size_t i = 0; i < f.params.size(); ++i) {
            const auto& p = f.params[i];
            if (p.type.is_ref) {
                LValue target = eval_lvalue(env, *c->args[i], functions, rt);
                call_env.define_ref_typed(p.name, p.type, target);
            } else {
                Value v2 = eval_expr(env, *c->args[i], functions, rt);
                call_env.define_value_typed(p.name, p.type, v2);
            }
        }

        try {
            exec_stmt(call_env, *f.body, functions, rt);
        } catch (ReturnSignal& r) {
            return r.value;
        }

        return Value{0};
    }

    throw std::runtime_error("unknown expression");
}

} // namespace interp
