#pragma once
#include <stdexcept>
#include <vector>
#include <iostream>

#include "env.hpp"
#include "functions.hpp"
#include "methods.hpp"
#include "../ast/stmt.hpp"
#include "../ast/expr.hpp"
#include "../ast/type.hpp"
#include "../sem/class_table.hpp"

namespace interp {

struct ReturnSignal {
    Value value;
};

inline ast::Type value_to_type(const Value& v) {
    if (std::holds_alternative<int>(v)) return ast::Type::Int();
    if (std::holds_alternative<bool>(v)) return ast::Type::Bool();
    if (std::holds_alternative<char>(v)) return ast::Type::Char();
    if (std::holds_alternative<std::string>(v)) return ast::Type::String();
    if (std::holds_alternative<ObjectPtr>(v)) {
        auto op = std::get<ObjectPtr>(v);
        if (!op) throw std::runtime_error("null object");
        return ast::Type::Class(op->dynamic_class);
    }
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

inline Value eval_expr(Env& env,
                       const ast::Expr& e,
                       FunctionTable& functions,
                       MethodTable& methods,
                       const sem::ClassTable& classes);

inline LValue eval_lvalue(Env& env,
                          const ast::Expr& e,
                          FunctionTable& functions,
                          MethodTable& methods,
                          const sem::ClassTable& classes) {
    using namespace ast;

    if (auto* v = dynamic_cast<const VarExpr*>(&e)) return env.resolve_lvalue(v->name);

    if (auto* ma = dynamic_cast<const MemberAccessExpr*>(&e)) {
        // only support obj as VarExpr for now
        auto* ov = dynamic_cast<const VarExpr*>(ma->object.get());
        if (!ov) throw std::runtime_error("expected lvalue object in member access");
        return LValue::field(env, ov->name, ma->field);
    }

    throw std::runtime_error("expected lvalue (variable or field)");
}

inline ast::Type eval_arg_type(Env& env,
                              const ast::Expr& e,
                              FunctionTable& functions,
                              MethodTable& methods,
                              const sem::ClassTable& classes) {
    try {
        LValue lv = eval_lvalue(env, e, functions, methods, classes);
        Value vv = env.read_lvalue(lv);
        ast::Type t = value_to_type(vv);
        t.is_ref = true;
        return t;
    } catch (...) {
        Value vv = eval_expr(env, e, functions, methods, classes);
        return value_to_type(vv);
    }
}

inline void exec_stmt(Env& env,
                      const ast::Stmt& s,
                      FunctionTable& functions,
                      MethodTable& methods,
                      const sem::ClassTable& classes) {
    using namespace ast;

    if (auto* b = dynamic_cast<const BlockStmt*>(&s)) {
        Env local(&env);
        for (auto& st : b->statements) exec_stmt(local, *st, functions, methods, classes);
        return;
    }

    if (auto* v = dynamic_cast<const VarDeclStmt*>(&s)) {
        Value init = 0;
        if (v->init) init = eval_expr(env, *v->init, functions, methods, classes);
        env.define_value(v->name, v->decl_type, init);
        return;
    }

    if (auto* e = dynamic_cast<const ExprStmt*>(&s)) {
        eval_expr(env, *e->expr, functions, methods, classes);
        return;
    }

    if (auto* i = dynamic_cast<const IfStmt*>(&s)) {
        bool cond = to_bool_like_cpp(eval_expr(env, *i->cond, functions, methods, classes));
        if (cond) exec_stmt(env, *i->then_branch, functions, methods, classes);
        else if (i->else_branch) exec_stmt(env, *i->else_branch, functions, methods, classes);
        return;
    }

    if (auto* w = dynamic_cast<const WhileStmt*>(&s)) {
        while (to_bool_like_cpp(eval_expr(env, *w->cond, functions, methods, classes))) {
            exec_stmt(env, *w->body, functions, methods, classes);
        }
        return;
    }

    if (auto* r = dynamic_cast<const ReturnStmt*>(&s)) {
        Value v = r->value ? eval_expr(env, *r->value, functions, methods, classes) : Value{0};
        throw ReturnSignal{v};
    }

    throw std::runtime_error("unknown statement");
}

inline Value eval_expr(Env& env,
                       const ast::Expr& e,
                       FunctionTable& functions,
                       MethodTable& methods,
                       const sem::ClassTable& classes) {
    using namespace ast;

    if (auto* i = dynamic_cast<const IntLiteral*>(&e)) return i->value;
    if (auto* b = dynamic_cast<const BoolLiteral*>(&e)) return b->value;
    if (auto* c = dynamic_cast<const CharLiteral*>(&e)) return c->value;
    if (auto* s = dynamic_cast<const StringLiteral*>(&e)) return s->value;

    if (auto* v = dynamic_cast<const VarExpr*>(&e)) return env.read_value(v->name);

    if (auto* a = dynamic_cast<const AssignExpr*>(&e)) {
        Value rhs = eval_expr(env, *a->value, functions, methods, classes);
        env.assign_value(a->name, rhs);
        return rhs;
    }

    if (auto* ma = dynamic_cast<const MemberAccessExpr*>(&e)) {
        LValue lv = eval_lvalue(env, e, functions, methods, classes);
        return env.read_lvalue(lv);
    }

    if (auto* mc = dynamic_cast<const MethodCallExpr*>(&e)) {
        // receiver must be VarExpr (for now)
        auto* ov = dynamic_cast<const VarExpr*>(mc->object.get());
        if (!ov) throw std::runtime_error("runtime error: method receiver must be a variable for now");

        ast::Type static_t = env.lookup_type(ov->name);
        ast::Type static_base = static_t; static_base.is_ref = false;
        if (static_base.base != ast::Type::Base::Class) throw std::runtime_error("runtime error: method call on non-class");

        std::string static_class = static_base.class_name;

        // arg types (base types only) and ref binding handled when building env
        std::vector<ast::Type> arg_base;
        arg_base.reserve(mc->args.size());
        for (auto& a2 : mc->args) {
            ast::Type at = eval_arg_type(env, *a2, functions, methods, classes);
            at.is_ref = false;
            arg_base.push_back(at);
        }

        // pick static target first
        ast::MethodDef& static_md = methods.resolve_static(classes, static_class, mc->method, arg_base);

        // decide dispatch class
        std::string dispatch_class = static_class;

        bool virtual_in_static = classes.is_virtual_in_chain(static_class, mc->method, [&](){
            std::vector<ast::Type> pars;
            pars.reserve(static_md.params.size());
            for (auto& p : static_md.params) pars.push_back(p.type);
            return pars;
        }());

        if (static_t.is_ref && virtual_in_static) {
            Value recv_v = env.read_value(ov->name);
            auto* op = std::get_if<ObjectPtr>(&recv_v);
            if (!op || !*op) throw std::runtime_error("runtime error: null receiver object");
            dispatch_class = (*op)->dynamic_class;

            std::vector<ast::Type> pars;
            pars.reserve(static_md.params.size());
            for (auto& p : static_md.params) pars.push_back(p.type);

            if (auto* dyn_md = methods.find_exact_in_chain(classes, dispatch_class, mc->method, pars)) {
                // call override
                // build call env:
                Env call_env(&env);
                // bind implicit fields into call_env as references via a hidden "__this"
                call_env.define_value("__this", ast::Type::Class(dispatch_class, true), recv_v);

                // params
                for (size_t i = 0; i < dyn_md->params.size(); ++i) {
                    const auto& p = dyn_md->params[i];
                    if (p.type.is_ref) {
                        LValue target = eval_lvalue(env, *mc->args[i], functions, methods, classes);
                        call_env.define_ref(p.name, p.type, target);
                    } else {
                        Value vv = eval_expr(env, *mc->args[i], functions, methods, classes);
                        call_env.define_value(p.name, p.type, vv);
                    }
                }

                try {
                    exec_stmt(call_env, *dyn_md->body, functions, methods, classes);
                } catch (ReturnSignal& r) {
                    return r.value;
                }
                return Value{0};
            }
        }

        // non-virtual or non-ref call -> static target
        Env call_env(&env);
        Value recv_v = env.read_value(ov->name);
        call_env.define_value("__this", ast::Type::Class(static_class, true), recv_v);

        for (size_t i = 0; i < static_md.params.size(); ++i) {
            const auto& p = static_md.params[i];
            if (p.type.is_ref) {
                LValue target = eval_lvalue(env, *mc->args[i], functions, methods, classes);
                call_env.define_ref(p.name, p.type, target);
            } else {
                Value vv = eval_expr(env, *mc->args[i], functions, methods, classes);
                call_env.define_value(p.name, p.type, vv);
            }
        }

        try {
            exec_stmt(call_env, *static_md.body, functions, methods, classes);
        } catch (ReturnSignal& r) {
            return r.value;
        }
        return Value{0};
    }

    if (auto* u = dynamic_cast<const UnaryExpr*>(&e)) {
        Value x = eval_expr(env, *u->expr, functions, methods, classes);
        if (u->op == UnaryExpr::Op::Neg) return -expect_int(x, "unary -");
        if (u->op == UnaryExpr::Op::Not) return !expect_bool(x, "unary !");
        throw std::runtime_error("unknown unary op");
    }

    if (auto* bin = dynamic_cast<const BinaryExpr*>(&e)) {
        if (bin->op == BinaryExpr::Op::AndAnd) {
            bool lv = expect_bool(eval_expr(env, *bin->left, functions, methods, classes), "operator && (lhs)");
            if (!lv) return false;
            bool rv = expect_bool(eval_expr(env, *bin->right, functions, methods, classes), "operator && (rhs)");
            return rv;
        }
        if (bin->op == BinaryExpr::Op::OrOr) {
            bool lv = expect_bool(eval_expr(env, *bin->left, functions, methods, classes), "operator || (lhs)");
            if (lv) return true;
            bool rv = expect_bool(eval_expr(env, *bin->right, functions, methods, classes), "operator || (rhs)");
            return rv;
        }

        Value lv = eval_expr(env, *bin->left, functions, methods, classes);
        Value rv = eval_expr(env, *bin->right, functions, methods, classes);

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

            auto cmp_int = [&](int a, int b) -> bool {
                switch (bin->op) {
                    case BinaryExpr::Op::Lt: return a < b;
                    case BinaryExpr::Op::Le: return a <= b;
                    case BinaryExpr::Op::Gt: return a > b;
                    case BinaryExpr::Op::Ge: return a >= b;
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

    if (auto* c = dynamic_cast<const CallExpr*>(&e)) {
        if (c->callee == "print_int") {
            if (c->args.size() != 1) throw std::runtime_error("print_int expects 1 argument");
            Value v = eval_expr(env, *c->args[0], functions, methods, classes);
            std::cout << expect_int(v, "print_int") << "\n";
            return Value{0};
        }
        if (c->callee == "print_bool") {
            if (c->args.size() != 1) throw std::runtime_error("print_bool expects 1 argument");
            Value v = eval_expr(env, *c->args[0], functions, methods, classes);
            std::cout << (expect_bool(v, "print_bool") ? "true" : "false") << "\n";
            return Value{0};
        }
        if (c->callee == "print_char") {
            if (c->args.size() != 1) throw std::runtime_error("print_char expects 1 argument");
            Value v = eval_expr(env, *c->args[0], functions, methods, classes);
            std::cout << expect_char(v, "print_char") << "\n";
            return Value{0};
        }
        if (c->callee == "print_string") {
            if (c->args.size() != 1) throw std::runtime_error("print_string expects 1 argument");
            Value v = eval_expr(env, *c->args[0], functions, methods, classes);
            std::cout << expect_string(v, "print_string") << "\n";
            return Value{0};
        }

        std::vector<ast::Type> arg_types;
        arg_types.reserve(c->args.size());
        for (auto& arg : c->args) arg_types.push_back(eval_arg_type(env, *arg, functions, methods, classes));

        auto& f = functions.resolve(c->callee, arg_types);

        Env call_env(&env);

        for (size_t i = 0; i < f.params.size(); ++i) {
            const auto& p = f.params[i];
            if (p.type.is_ref) {
                LValue target = eval_lvalue(env, *c->args[i], functions, methods, classes);
                call_env.define_ref(p.name, p.type, target);
            } else {
                Value v = eval_expr(env, *c->args[i], functions, methods, classes);
                call_env.define_value(p.name, p.type, v);
            }
        }

        try {
            exec_stmt(call_env, *f.body, functions, methods, classes);
        } catch (ReturnSignal& r) {
            return r.value;
        }

        return Value{0};
    }

    throw std::runtime_error("unknown expression");
}

} // namespace interp
