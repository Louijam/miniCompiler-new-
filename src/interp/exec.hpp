#pragma once
#include <stdexcept>
#include <vector>
#include <iostream>
#include <string>

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

inline char expect_char(const Value& v, const char* ctx) {
    if (auto* pc = std::get_if<char>(&v)) return *pc;
    throw std::runtime_error(std::string("type error: expected char in ") + ctx);
}

inline const std::string& expect_string(const Value& v, const char* ctx) {
    if (auto* ps = std::get_if<std::string>(&v)) return *ps;
    throw std::runtime_error(std::string("type error: expected string in ") + ctx);
}

inline ast::Type type_of_value(const Value& v) {
    if (std::holds_alternative<bool>(v)) return ast::Type::Bool(false);
    if (std::holds_alternative<int>(v)) return ast::Type::Int(false);
    if (std::holds_alternative<char>(v)) return ast::Type::Char(false);
    if (std::holds_alternative<std::string>(v)) return ast::Type::String(false);
    if (auto* po = std::get_if<ObjectPtr>(&v)) {
        if (!*po) throw std::runtime_error("null object has no type");
        return ast::Type::Class((*po)->dynamic_class, false);
    }
    throw std::runtime_error("unknown runtime value type");
}

inline Value default_value_for(const ast::Type& t) {
    switch (t.base) {
        case ast::Type::Base::Bool:   return Value{false};
        case ast::Type::Base::Int:    return Value{0};
        case ast::Type::Base::Char:   return Value{'\0'};
        case ast::Type::Base::String: return Value{std::string("")};
        case ast::Type::Base::Void:   return Value{0};
        case ast::Type::Base::Class: {
            auto o = std::make_shared<Object>();
            o->dynamic_class = t.class_name;
            return Value{o};
        }
    }
    return Value{0};
}

inline Value eval_expr(Env& env, const ast::Expr& e, FunctionTable& functions);
inline void exec_stmt(Env& env, const ast::Stmt& s, FunctionTable& functions); // forward decl

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

inline ast::Type arg_type_for_call(Env& env, const ast::Expr& arg, FunctionTable& functions) {
    if (auto* v = dynamic_cast<const ast::VarExpr*>(&arg)) {
        ast::Type t = ast::strip_ref(env.static_type_of(v->name));
        t.is_ref = true;
        return t;
    }

    if (dynamic_cast<const ast::MemberAccessExpr*>(&arg)) {
        LValue lv = eval_lvalue(env, arg, functions);
        Value vv = env.read_lvalue(lv);
        ast::Type t = type_of_value(vv);
        t.is_ref = true;
        return t;
    }

    Value vv = eval_expr(env, arg, functions);
    return type_of_value(vv);
}

inline Value call_function(Env& caller_env, ast::FunctionDef& f,
                           const ast::CallExpr& call, FunctionTable& functions) {
    if (f.params.size() != call.args.size())
        throw std::runtime_error("arity mismatch calling " + f.name);

    Env callee_env(&caller_env);

    for (size_t i = 0; i < call.args.size(); ++i) {
        const auto& p = f.params[i];
        const auto& a = *call.args[i];

        if (p.type.is_ref) {
            LValue target = eval_lvalue(caller_env, a, functions);
            callee_env.define_ref(p.name, target, p.type);
        } else {
            Value v = eval_expr(caller_env, a, functions);
            callee_env.define_value(p.name, v, p.type);
        }
    }

    try {
        exec_stmt(callee_env, *f.body, functions);
    } catch (const ReturnSignal& rs) {
        return rs.value;
    }

    return default_value_for(f.return_type);
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
            Value init = v->init ? eval_expr(env, *v->init, functions) : default_value_for(t);
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

inline void builtin_print(const std::string& name, const Value& v) {
    if (name == "print_bool")   { std::cout << (std::get<bool>(v) ? "true" : "false") << "\n"; return; }
    if (name == "print_int")    { std::cout << std::get<int>(v) << "\n"; return; }
    if (name == "print_char")   { std::cout << std::get<char>(v) << "\n"; return; }
    if (name == "print_string") { std::cout << std::get<std::string>(v) << "\n"; return; }
    throw std::runtime_error("unknown builtin: " + name);
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
        Value objv = eval_expr(env, *fa->object, functions);
        auto* pobj = std::get_if<ObjectPtr>(&objv);
        if (!pobj || !*pobj) throw std::runtime_error("field assignment on non-object");
        LValue lv = LValue::field_of(*pobj, fa->field);
        Value rhs = eval_expr(env, *fa->value, functions);
        env.write_lvalue(lv, rhs);
        return rhs;
    }

    if (dynamic_cast<const MemberAccessExpr*>(&e)) {
        LValue lv = eval_lvalue(env, e, functions);
        return env.read_lvalue(lv);
    }

    if (auto* call = dynamic_cast<const CallExpr*>(&e)) {
        // builtins
        if (call->callee == "print_bool" || call->callee == "print_int" ||
            call->callee == "print_char" || call->callee == "print_string") {

            if (call->args.size() != 1) throw std::runtime_error("arity mismatch calling " + call->callee);
            Value v = eval_expr(env, *call->args[0], functions);
            builtin_print(call->callee, v);
            return Value{0};
        }

        std::vector<ast::Type> arg_types;
        arg_types.reserve(call->args.size());
        for (auto& a : call->args)
            arg_types.push_back(arg_type_for_call(env, *a, functions));

        ast::FunctionDef& f = functions.resolve(call->callee, arg_types);
        return call_function(env, f, *call, functions);
    }

    if (auto* ce = dynamic_cast<const ConstructExpr*>(&e)) {
        auto o = std::make_shared<Object>();
        o->dynamic_class = ce->class_name;
        return Value{o};
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

        switch (bin->op) {
            case BinaryExpr::Op::Add: return expect_int(lv, "+") + expect_int(rv, "+");
            case BinaryExpr::Op::Sub: return expect_int(lv, "-") - expect_int(rv, "-");
            case BinaryExpr::Op::Mul: return expect_int(lv, "*") * expect_int(rv, "*");
            case BinaryExpr::Op::Div: {
                int r = expect_int(rv, "/");
                if (r == 0) throw std::runtime_error("Division durch 0");
                return expect_int(lv, "/") / r;
            }
            case BinaryExpr::Op::Mod: {
                int r = expect_int(rv, "%");
                if (r == 0) throw std::runtime_error("Modulo durch 0");
                return expect_int(lv, "%") % r;
            }
            default:
                break;
        }
    }

    throw std::runtime_error("unknown expression");
}

} // namespace interp
