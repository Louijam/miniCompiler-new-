#pragma once
#include <stdexcept>
#include <vector>
#include <iostream>
#include <string>
#include <utility>

#include "env.hpp"
#include "functions.hpp"
#include "lvalue.hpp"
#include "object.hpp"
#include "class_runtime.hpp"
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
        case ast::Type::Base::Class:
            throw std::runtime_error("default_value_for(Class) not implemented yet");
    }
    return Value{0};
}

inline Value eval_expr(Env& env, const ast::Expr& e, FunctionTable& functions, const ClassRuntime* classes_rt);
inline void exec_stmt(Env& env, const ast::Stmt& s, FunctionTable& functions, const ClassRuntime* classes_rt); // forward decl

inline LValue eval_lvalue(Env& env, const ast::Expr& e, FunctionTable& functions, const ClassRuntime* classes_rt) {
    using namespace ast;

    if (auto* v = dynamic_cast<const VarExpr*>(&e))
        return env.resolve_lvalue(v->name);

    if (auto* m = dynamic_cast<const MemberAccessExpr*>(&e)) {
        Value objv = eval_expr(env, *m->object, functions, classes_rt);
        auto* pobj = std::get_if<ObjectPtr>(&objv);
        if (!pobj || !*pobj)
            throw std::runtime_error("member access on non-object");
        return LValue::field_of(*pobj, m->field);
    }

    throw std::runtime_error("expected lvalue");
}

inline ast::Type arg_type_for_call(Env& env, const ast::Expr& arg, FunctionTable& functions, const ClassRuntime* classes_rt) {
    if (auto* v = dynamic_cast<const ast::VarExpr*>(&arg)) {
        ast::Type t = ast::strip_ref(env.static_type_of(v->name));
        t.is_ref = true;
        return t;
    }

    if (dynamic_cast<const ast::MemberAccessExpr*>(&arg)) {
        LValue lv = eval_lvalue(env, arg, functions, classes_rt);
        Value vv = env.read_lvalue(lv);
        ast::Type t = type_of_value(vv);
        t.is_ref = true;
        return t;
    }

    Value vv = eval_expr(env, arg, functions, classes_rt);
    return type_of_value(vv);
}

inline Value call_function(Env& caller_env, ast::FunctionDef& f,
                           const ast::CallExpr& call, FunctionTable& functions, const ClassRuntime* classes_rt) {
    if (f.params.size() != call.args.size())
        throw std::runtime_error("arity mismatch calling " + f.name);

    Env callee_env(&caller_env);

    for (size_t i = 0; i < call.args.size(); ++i) {
        const auto& p = f.params[i];
        const auto& a = *call.args[i];

        if (p.type.is_ref) {
            LValue target = eval_lvalue(caller_env, a, functions, classes_rt);
            callee_env.define_ref(p.name, target, p.type);
        } else {
            Value v = eval_expr(caller_env, a, functions, classes_rt);
            callee_env.define_value(p.name, v, p.type);
        }
    }

    try {
        exec_stmt(callee_env, *f.body, functions, classes_rt);
    } catch (const ReturnSignal& rs) {
        return rs.value;
    }

    return default_value_for(f.return_type);
}

inline Value call_method(Env& caller_env,
                         const ObjectPtr& obj,
                         const ast::Type& static_obj_type,
                         const ast::MethodDef& m,
                         const std::vector<ast::ExprPtr>& args,
                         FunctionTable& functions,
                         const ClassRuntime* classes_rt) {
    (void)obj;
    (void)static_obj_type;

    if (m.params.size() != args.size())
        throw std::runtime_error("arity mismatch calling method " + m.name);

    Env callee_env(&caller_env);

    for (size_t i = 0; i < args.size(); ++i) {
        const auto& p = m.params[i];
        const auto& a = *args[i];

        if (p.type.is_ref) {
            LValue target = eval_lvalue(caller_env, a, functions, classes_rt);
            callee_env.define_ref(p.name, target, p.type);
        } else {
            Value v = eval_expr(caller_env, a, functions, classes_rt);
            callee_env.define_value(p.name, v, p.type);
        }
    }

    try {
        exec_stmt(callee_env, *m.body, functions, classes_rt);
    } catch (const ReturnSignal& rs) {
        return rs.value;
    }

    return default_value_for(m.return_type);
}

inline void exec_stmt(Env& env, const ast::Stmt& s, FunctionTable& functions, const ClassRuntime* classes_rt) {
    using namespace ast;

    if (auto* b = dynamic_cast<const BlockStmt*>(&s)) {
        Env local(&env);
        for (auto& st : b->statements)
            exec_stmt(local, *st, functions, classes_rt);
        return;
    }

    if (auto* v = dynamic_cast<const VarDeclStmt*>(&s)) {
        const ast::Type& t = v->decl_type;

        if (t.is_ref) {
            if (!v->init)
                throw std::runtime_error("Referenzvariable muss initialisiert werden");
            LValue target = eval_lvalue(env, *v->init, functions, classes_rt);
            env.define_ref(v->name, target, t);
        } else {
            Value init = v->init ? eval_expr(env, *v->init, functions, classes_rt) : default_value_for(t);
            env.define_value(v->name, init, t);
        }
        return;
    }

    if (auto* e = dynamic_cast<const ExprStmt*>(&s)) {
        eval_expr(env, *e->expr, functions, classes_rt);
        return;
    }

    if (auto* i = dynamic_cast<const IfStmt*>(&s)) {
        bool cond = to_bool_like_cpp(eval_expr(env, *i->cond, functions, classes_rt));
        if (cond)
            exec_stmt(env, *i->then_branch, functions, classes_rt);
        else if (i->else_branch)
            exec_stmt(env, *i->else_branch, functions, classes_rt);
        return;
    }

    if (auto* w = dynamic_cast<const WhileStmt*>(&s)) {
        while (to_bool_like_cpp(eval_expr(env, *w->cond, functions, classes_rt)))
            exec_stmt(env, *w->body, functions, classes_rt);
        return;
    }

    if (auto* r = dynamic_cast<const ReturnStmt*>(&s)) {
        Value v = r->value ? eval_expr(env, *r->value, functions, classes_rt) : Value{0};
        throw ReturnSignal{v};
    }

    throw std::runtime_error("unknown statement");
}

inline Value eval_expr(Env& env, const ast::Expr& e, FunctionTable& functions, const ClassRuntime* classes_rt) {
    using namespace ast;

    if (auto* i = dynamic_cast<const IntLiteral*>(&e)) return i->value;
    if (auto* b = dynamic_cast<const BoolLiteral*>(&e)) return b->value;
    if (auto* c = dynamic_cast<const CharLiteral*>(&e)) return c->value;
    if (auto* s = dynamic_cast<const StringLiteral*>(&e)) return s->value;

    if (auto* v = dynamic_cast<const VarExpr*>(&e))
        return env.read_value(v->name);

    if (auto* a = dynamic_cast<const AssignExpr*>(&e)) {
        Value rhs = eval_expr(env, *a->value, functions, classes_rt);
        env.assign_value(a->name, rhs);
        return rhs;
    }

    if (auto* fa = dynamic_cast<const FieldAssignExpr*>(&e)) {
        Value objv = eval_expr(env, *fa->object, functions, classes_rt);
        auto* pobj = std::get_if<ObjectPtr>(&objv);
        if (!pobj || !*pobj)
            throw std::runtime_error("field assignment on non-object");
        LValue lv = LValue::field_of(*pobj, fa->field);
        Value rhs = eval_expr(env, *fa->value, functions, classes_rt);
        env.write_lvalue(lv, rhs);
        return rhs;
    }

    if (dynamic_cast<const MemberAccessExpr*>(&e)) {
        LValue lv = eval_lvalue(env, e, functions, classes_rt);
        return env.read_lvalue(lv);
    }

    if (auto* call = dynamic_cast<const CallExpr*>(&e)) {
        std::vector<ast::Type> arg_types;
        arg_types.reserve(call->args.size());
        for (auto& a : call->args)
            arg_types.push_back(arg_type_for_call(env, *a, functions, classes_rt));

        ast::FunctionDef& f = functions.resolve(call->callee, arg_types);
        return call_function(env, f, *call, functions, classes_rt);
    }

    if (auto* mc = dynamic_cast<const MethodCallExpr*>(&e)) {
        if (!classes_rt) throw std::runtime_error("runtime error: MethodCall requires ClassRuntime");

        Value objv = eval_expr(env, *mc->object, functions, classes_rt);
        auto* pobj = std::get_if<ObjectPtr>(&objv);
        if (!pobj || !*pobj) throw std::runtime_error("method call on non-object");

        bool call_via_ref = false;
        std::string static_cls;

        if (auto* ov = dynamic_cast<const VarExpr*>(mc->object.get())) {
            ast::Type st = env.static_type_of(ov->name);
            call_via_ref = st.is_ref;
            st.is_ref = false;
            if (st.base != ast::Type::Base::Class) throw std::runtime_error("method object is not class-typed");
            static_cls = st.class_name;
        } else {
            ast::Type st = type_of_value(objv);
            if (st.base != ast::Type::Base::Class) throw std::runtime_error("method object is not class-typed");
            static_cls = st.class_name;
            call_via_ref = false;
        }

        std::vector<ast::Type> arg_types;
        std::vector<bool> arg_is_lvalue;
        arg_types.reserve(mc->args.size());
        arg_is_lvalue.reserve(mc->args.size());

        for (auto& a : mc->args) {
            bool is_lv = dynamic_cast<const VarExpr*>(a.get()) != nullptr
                      || dynamic_cast<const MemberAccessExpr*>(a.get()) != nullptr;
            arg_is_lvalue.push_back(is_lv);
            arg_types.push_back(arg_type_for_call(env, *a, functions, classes_rt));
        }

        const std::string dyn_cls = (*pobj)->dynamic_class;

        const ast::MethodDef& m = classes_rt->resolve_method(
            static_cls, dyn_cls, mc->method, arg_types, arg_is_lvalue, call_via_ref
        );

        ast::Type static_obj_type = ast::Type::Class(static_cls, call_via_ref);
        return call_method(env, *pobj, static_obj_type, m, mc->args, functions, classes_rt);
    }

    if (auto* u = dynamic_cast<const UnaryExpr*>(&e)) {
        Value x = eval_expr(env, *u->expr, functions, classes_rt);
        if (u->op == UnaryExpr::Op::Neg) return -expect_int(x, "unary -");
        if (u->op == UnaryExpr::Op::Not) return !expect_bool(x, "unary !");
    }

    if (auto* bin = dynamic_cast<const BinaryExpr*>(&e)) {
        if (bin->op == BinaryExpr::Op::AndAnd) {
            bool lv = expect_bool(eval_expr(env, *bin->left, functions, classes_rt), "&& lhs");
            if (!lv) return false;
            return expect_bool(eval_expr(env, *bin->right, functions, classes_rt), "&& rhs");
        }
        if (bin->op == BinaryExpr::Op::OrOr) {
            bool lv = expect_bool(eval_expr(env, *bin->left, functions, classes_rt), "|| lhs");
            if (lv) return true;
            return expect_bool(eval_expr(env, *bin->right, functions, classes_rt), "|| rhs");
        }

        Value lv = eval_expr(env, *bin->left, functions, classes_rt);
        Value rv = eval_expr(env, *bin->right, functions, classes_rt);

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

            case BinaryExpr::Op::Lt:
            case BinaryExpr::Op::Le:
            case BinaryExpr::Op::Gt:
            case BinaryExpr::Op::Ge: {
                if (std::holds_alternative<int>(lv)) {
                    int a = expect_int(lv, "relop");
                    int b = expect_int(rv, "relop");
                    if (bin->op == BinaryExpr::Op::Lt) return a < b;
                    if (bin->op == BinaryExpr::Op::Le) return a <= b;
                    if (bin->op == BinaryExpr::Op::Gt) return a > b;
                    return a >= b;
                }
                if (std::holds_alternative<char>(lv)) {
                    char a = expect_char(lv, "relop");
                    char b = expect_char(rv, "relop");
                    if (bin->op == BinaryExpr::Op::Lt) return a < b;
                    if (bin->op == BinaryExpr::Op::Le) return a <= b;
                    if (bin->op == BinaryExpr::Op::Gt) return a > b;
                    return a >= b;
                }
                throw std::runtime_error("type error: relational operators only for int/char");
            }

            case BinaryExpr::Op::Eq:
            case BinaryExpr::Op::Ne: {
                bool eq = false;
                if (std::holds_alternative<int>(lv)) {
                    eq = expect_int(lv, "==") == expect_int(rv, "==");
                } else if (std::holds_alternative<char>(lv)) {
                    eq = expect_char(lv, "==") == expect_char(rv, "==");
                } else if (std::holds_alternative<bool>(lv)) {
                    eq = expect_bool(lv, "==") == expect_bool(rv, "==");
                } else if (std::holds_alternative<std::string>(lv)) {
                    eq = expect_string(lv, "==") == expect_string(rv, "==");
                } else {
                    throw std::runtime_error("type error: ==/!= not supported for this type");
                }
                return (bin->op == BinaryExpr::Op::Eq) ? eq : !eq;
            }

            case BinaryExpr::Op::AndAnd:
            case BinaryExpr::Op::OrOr:
                break;
        }
    }

    throw std::runtime_error("unknown expression");
}

// Backward-compat wrappers (alte Aufrufe bleiben ok)
inline Value eval_expr(Env& env, const ast::Expr& e, FunctionTable& functions) {
    return eval_expr(env, e, functions, nullptr);
}
inline void exec_stmt(Env& env, const ast::Stmt& s, FunctionTable& functions) {
    exec_stmt(env, s, functions, nullptr);
}

} // namespace interp
