#pragma once
#include <stdexcept>
#include <vector>
#include <iostream>

#include "env.hpp"
#include "functions.hpp"
#include "lvalue.hpp"
#include "object.hpp"
#include "value.hpp"
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

inline ast::Type type_of_value(const Value& v) {
    if (std::holds_alternative<bool>(v)) return ast::Type::Bool(false);
    if (std::holds_alternative<int>(v)) return ast::Type::Int(false);
    if (std::holds_alternative<char>(v)) return ast::Type::Char(false);
    if (std::holds_alternative<std::string>(v)) return ast::Type::String(false);
    if (auto* o = std::get_if<ObjectPtr>(&v)) {
        if (!*o) throw std::runtime_error("null object value");
        return ast::Type::Class((*o)->dynamic_class, false);
    }
    throw std::runtime_error("unknown runtime value kind");
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

inline bool is_lvalue_expr(const ast::Expr& e) {
    return dynamic_cast<const ast::VarExpr*>(&e) != nullptr
        || dynamic_cast<const ast::MemberAccessExpr*>(&e) != nullptr;
}

inline Value default_value_for_type(const ast::Type& t, FunctionTable& functions);

inline void exec_stmt(Env& env, const ast::Stmt& s, FunctionTable& functions);

inline void bind_fields_as_refs(Env& method_env,
                                const ObjectPtr& self,
                                const std::string& static_class,
                                FunctionTable& functions) {
    const auto& ci = functions.class_rt.get(static_class);
    for (const auto& kv : ci.merged_fields) {
        ast::Type rt = kv.second;
        rt.is_ref = true;
        method_env.define_ref(kv.first, LValue::field_of(self, kv.first), rt);
    }
}

inline ObjectPtr allocate_object_with_default_fields(const std::string& class_name, FunctionTable& functions) {
    auto obj = std::make_shared<Object>();
    obj->dynamic_class = class_name;

    const auto& ci = functions.class_rt.get(class_name);
    for (const auto& kv : ci.merged_fields) {
        obj->fields[kv.first] = default_value_for_type(kv.second, functions);
    }
    return obj;
}

inline Value default_value_for_type(const ast::Type& t, FunctionTable& functions) {
    using Base = ast::Type::Base;

    if (t.base == Base::Bool) return Value{false};
    if (t.base == Base::Int) return Value{0};
    if (t.base == Base::Char) return Value{char('\0')};
    if (t.base == Base::String) return Value{std::string("")};
    if (t.base == Base::Class) {
        auto obj = allocate_object_with_default_fields(t.class_name, functions);
        (void)obj;
        return Value{obj};
    }

    return Value{0};
}

inline Value call_builtin(const std::string& name, const std::vector<Value>& args) {
    if (name == "print_int") {
        std::cout << expect_int(args.at(0), "print_int") << "\n";
        return Value{0};
    }
    if (name == "print_bool") {
        bool b = expect_bool(args.at(0), "print_bool");
        // GOLD tests wollen 0/1
        std::cout << (b ? 1 : 0) << "\n";
        return Value{0};
    }
    if (name == "print_char") {
        auto* pc = std::get_if<char>(&args.at(0));
        if (!pc) throw std::runtime_error("type error: expected char in print_char");
        std::cout << *pc << "\n";
        return Value{0};
    }
    if (name == "print_string") {
        auto* ps = std::get_if<std::string>(&args.at(0));
        if (!ps) throw std::runtime_error("type error: expected string in print_string");
        std::cout << *ps << "\n";
        return Value{0};
    }
    throw std::runtime_error("unknown builtin: " + name);
}

inline Value call_function(Env& caller_env,
                           ast::FunctionDef& f,
                           const std::vector<Value>& arg_vals,
                           const std::vector<LValue>& arg_lvals,
                           FunctionTable& functions) {
    Env callee(&caller_env);

    for (size_t i = 0; i < f.params.size(); ++i) {
        const auto& p = f.params[i];
        if (p.type.is_ref) {
            callee.define_ref(p.name, arg_lvals[i], p.type);
        } else {
            callee.define_value(p.name, arg_vals[i], p.type);
        }
    }

    try {
        exec_stmt(callee, *f.body, functions);
    } catch (const ReturnSignal& rs) {
        return rs.value;
    }
    return Value{0};
}

inline Value call_method(Env& caller_env,
                         const ObjectPtr& self,
                         const std::string& static_class,
                         const ast::MethodDef& m,
                         const std::vector<Value>& arg_vals,
                         const std::vector<LValue>& arg_lvals,
                         FunctionTable& functions) {
    Env method_env(&caller_env);

    bind_fields_as_refs(method_env, self, static_class, functions);

    for (size_t i = 0; i < m.params.size(); ++i) {
        const auto& p = m.params[i];
        if (p.type.is_ref) {
            method_env.define_ref(p.name, arg_lvals[i], p.type);
        } else {
            method_env.define_value(p.name, arg_vals[i], p.type);
        }
    }

    try {
        exec_stmt(method_env, *m.body, functions);
    } catch (const ReturnSignal& rs) {
        return rs.value;
    }
    return Value{0};
}

inline void call_ctor(Env& caller_env,
                      const ObjectPtr& self,
                      const std::string& ctor_class,
                      const ast::ConstructorDef& ctor,
                      const std::vector<Value>& arg_vals,
                      const std::vector<LValue>& arg_lvals,
                      FunctionTable& functions) {
    Env ctor_env(&caller_env);

    bind_fields_as_refs(ctor_env, self, ctor_class, functions);

    for (size_t i = 0; i < ctor.params.size(); ++i) {
        const auto& p = ctor.params[i];
        if (p.type.is_ref) {
            ctor_env.define_ref(p.name, arg_lvals[i], p.type);
        } else {
            ctor_env.define_value(p.name, arg_vals[i], p.type);
        }
    }

    if (!ctor.body) return;

    try {
        exec_stmt(ctor_env, *ctor.body, functions);
    } catch (const ReturnSignal&) {
        // ignore
    }
}

inline void run_default_ctor_chain(Env& caller_env,
                                  const ObjectPtr& self,
                                  const std::string& class_name,
                                  FunctionTable& functions) {
    const auto& ci = functions.class_rt.get(class_name);

    if (!ci.base.empty()) {
        run_default_ctor_chain(caller_env, self, ci.base, functions);
    }

    std::vector<Value> args;
    std::vector<LValue> lvs;
    std::vector<ast::Type> tys;
    std::vector<bool> islv;

    const auto& ctor = functions.class_rt.resolve_ctor(class_name, tys, islv);
    call_ctor(caller_env, self, class_name, ctor, args, lvs, functions);
}

inline void run_base_default_only(Env& caller_env,
                                 const ObjectPtr& self,
                                 const std::string& class_name,
                                 FunctionTable& functions) {
    const auto& ci = functions.class_rt.get(class_name);
    if (ci.base.empty()) return;
    run_default_ctor_chain(caller_env, self, ci.base, functions);
}

inline Value construct_object(Env& caller_env,
                              const std::string& class_name,
                              const std::vector<Value>& arg_vals,
                              const std::vector<LValue>& arg_lvals,
                              const std::vector<ast::Type>& arg_types,
                              const std::vector<bool>& arg_is_lvalue,
                              FunctionTable& functions) {
    ObjectPtr self = allocate_object_with_default_fields(class_name, functions);

    run_base_default_only(caller_env, self, class_name, functions);

    const auto& ctor = functions.class_rt.resolve_ctor(class_name, arg_types, arg_is_lvalue);
    call_ctor(caller_env, self, class_name, ctor, arg_vals, arg_lvals, functions);

    return Value{self};
}

inline void exec_stmt(Env& env, const ast::Stmt& s, FunctionTable& functions) {
    using namespace ast;

    if (auto* b = dynamic_cast<const BlockStmt*>(&s)) {
        Env local(&env);
        for (auto& st : b->statements) exec_stmt(local, *st, functions);
        return;
    }

    if (auto* v = dynamic_cast<const VarDeclStmt*>(&s)) {
        const ast::Type& t = v->decl_type;

        if (t.is_ref) {
            if (!v->init) throw std::runtime_error("Referenzvariable muss initialisiert werden");
            LValue target = eval_lvalue(env, *v->init, functions);
            env.define_ref(v->name, target, t);
        } else {
            Value init;
            if (v->init) init = eval_expr(env, *v->init, functions);
            else {
                if (t.base == ast::Type::Base::Class) {
                    auto obj = allocate_object_with_default_fields(t.class_name, functions);
                    run_default_ctor_chain(env, obj, t.class_name, functions);
                    init = Value{obj};
                } else {
                    init = default_value_for_type(t, functions);
                }
            }
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
        if (cond) exec_stmt(env, *i->then_branch, functions);
        else if (i->else_branch) exec_stmt(env, *i->else_branch, functions);
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

    if (auto* v = dynamic_cast<const VarExpr*>(&e)) return env.read_value(v->name);

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

    if (auto* ce = dynamic_cast<const ConstructExpr*>(&e)) {
        std::vector<Value> arg_vals;
        std::vector<LValue> arg_lvals;
        std::vector<ast::Type> arg_types;
        std::vector<bool> arg_is_lvalue;

        arg_vals.reserve(ce->args.size());
        arg_lvals.reserve(ce->args.size());
        arg_types.reserve(ce->args.size());
        arg_is_lvalue.reserve(ce->args.size());

        for (auto& a2 : ce->args) {
            bool lv = is_lvalue_expr(*a2);
            arg_is_lvalue.push_back(lv);
            if (lv) arg_lvals.push_back(eval_lvalue(env, *a2, functions));
            else arg_lvals.push_back(LValue{});

            Value vv = eval_expr(env, *a2, functions);
            arg_vals.push_back(vv);
            arg_types.push_back(type_of_value(vv));
        }

        return construct_object(env, ce->class_name, arg_vals, arg_lvals, arg_types, arg_is_lvalue, functions);
    }

    if (auto* call = dynamic_cast<const CallExpr*>(&e)) {
        if (call->callee.rfind("print_", 0) == 0) {
            std::vector<Value> args;
            args.reserve(call->args.size());
            for (auto& a2 : call->args) args.push_back(eval_expr(env, *a2, functions));
            return call_builtin(call->callee, args);
        }

        std::vector<Value> arg_vals;
        std::vector<LValue> arg_lvals;
        std::vector<ast::Type> arg_types;
        std::vector<bool> arg_is_lvalue;

        arg_vals.reserve(call->args.size());
        arg_lvals.reserve(call->args.size());
        arg_types.reserve(call->args.size());
        arg_is_lvalue.reserve(call->args.size());

        for (auto& a2 : call->args) {
            bool lv = is_lvalue_expr(*a2);
            arg_is_lvalue.push_back(lv);
            if (lv) arg_lvals.push_back(eval_lvalue(env, *a2, functions));
            else arg_lvals.push_back(LValue{});

            Value vv = eval_expr(env, *a2, functions);
            arg_vals.push_back(vv);
            arg_types.push_back(type_of_value(vv));
        }

        ast::FunctionDef& f = functions.resolve(call->callee, arg_types, arg_is_lvalue);
        return call_function(env, f, arg_vals, arg_lvals, functions);
    }

    if (auto* mc = dynamic_cast<const MethodCallExpr*>(&e)) {
        Value objv = eval_expr(env, *mc->object, functions);
        auto* pobj = std::get_if<ObjectPtr>(&objv);
        if (!pobj || !*pobj) throw std::runtime_error("method call on non-object");

        ObjectPtr self = *pobj;
        std::string dynamic_class = self->dynamic_class;

        std::string static_class = dynamic_class;
        bool call_via_ref = false;

        if (auto* ov = dynamic_cast<const VarExpr*>(mc->object.get())) {
            ast::Type st = env.static_type_of(ov->name);
            call_via_ref = st.is_ref;
            if (st.base == ast::Type::Base::Class) static_class = st.class_name;
        }

        std::vector<Value> arg_vals;
        std::vector<LValue> arg_lvals;
        std::vector<ast::Type> arg_types;
        std::vector<bool> arg_is_lvalue;

        arg_vals.reserve(mc->args.size());
        arg_lvals.reserve(mc->args.size());
        arg_types.reserve(mc->args.size());
        arg_is_lvalue.reserve(mc->args.size());

        for (auto& a2 : mc->args) {
            bool lv = is_lvalue_expr(*a2);
            arg_is_lvalue.push_back(lv);
            if (lv) arg_lvals.push_back(eval_lvalue(env, *a2, functions));
            else arg_lvals.push_back(LValue{});

            Value vv = eval_expr(env, *a2, functions);
            arg_vals.push_back(vv);
            arg_types.push_back(type_of_value(vv));
        }

        const ast::MethodDef& target =
            functions.class_rt.resolve_method(static_class, dynamic_class,
                                              mc->method, arg_types, arg_is_lvalue, call_via_ref);

        return call_method(env, self, static_class, target, arg_vals, arg_lvals, functions);
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

        // arithmetic int
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

        // relational (< <= > >=) for int/char
        if (bin->op == BinaryExpr::Op::Lt || bin->op == BinaryExpr::Op::Le ||
            bin->op == BinaryExpr::Op::Gt || bin->op == BinaryExpr::Op::Ge) {

            if (std::holds_alternative<int>(lv) && std::holds_alternative<int>(rv)) {
                int a = std::get<int>(lv), b = std::get<int>(rv);
                if (bin->op == BinaryExpr::Op::Lt) return a < b;
                if (bin->op == BinaryExpr::Op::Le) return a <= b;
                if (bin->op == BinaryExpr::Op::Gt) return a > b;
                return a >= b;
            }
            if (std::holds_alternative<char>(lv) && std::holds_alternative<char>(rv)) {
                char a = std::get<char>(lv), b = std::get<char>(rv);
                if (bin->op == BinaryExpr::Op::Lt) return a < b;
                if (bin->op == BinaryExpr::Op::Le) return a <= b;
                if (bin->op == BinaryExpr::Op::Gt) return a > b;
                return a >= b;
            }
            throw std::runtime_error("type error: relational expects int or char");
        }

        // equality (== !=) for bool/int/char/string
        if (bin->op == BinaryExpr::Op::Eq || bin->op == BinaryExpr::Op::Ne) {
            bool eq = false;

            if (std::holds_alternative<int>(lv) && std::holds_alternative<int>(rv)) {
                eq = std::get<int>(lv) == std::get<int>(rv);
            } else if (std::holds_alternative<bool>(lv) && std::holds_alternative<bool>(rv)) {
                eq = std::get<bool>(lv) == std::get<bool>(rv);
            } else if (std::holds_alternative<char>(lv) && std::holds_alternative<char>(rv)) {
                eq = std::get<char>(lv) == std::get<char>(rv);
            } else if (std::holds_alternative<std::string>(lv) && std::holds_alternative<std::string>(rv)) {
                eq = std::get<std::string>(lv) == std::get<std::string>(rv);
            } else {
                throw std::runtime_error("type error: ==/!= expects same primitive type");
            }

            if (bin->op == BinaryExpr::Op::Eq) return eq;
            return !eq;
        }
    }

    throw std::runtime_error("unknown expression");
}

} // namespace interp
