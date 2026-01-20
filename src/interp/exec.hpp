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

inline Value default_value_for_type(const ast::Type& t, FunctionTable& functions);

inline ObjectPtr default_construct_object(const std::string& class_name, FunctionTable& functions) {
    auto obj = std::make_shared<Object>();
    obj->dynamic_class = class_name;

    const auto& ci = functions.class_rt.get(class_name);

    for (const auto& kv : ci.merged_fields) {
        obj->fields[kv.first] = default_value_for_type(kv.second, functions);
    }
    return obj;
}

inline Value default_value_for_type(const ast::Type& t, FunctionTable& functions) {
    using namespace ast;

    if (t.base == Type::Base::Bool) return Value{false};
    if (t.base == Type::Base::Int) return Value{0};
    if (t.base == Type::Base::Char) return Value{char('\0')};
    if (t.base == Type::Base::String) return Value{std::string("")};
    if (t.base == Type::Base::Class) {
        return Value{default_construct_object(t.class_name, functions)};
    }
    return Value{0};
}

inline void bind_fields_as_refs(Env& method_env, const ObjectPtr& self, const std::string& static_class, FunctionTable& functions) {
    // Expose fields as unqualified names (no this).
    // We bind by ref so assignments change the actual object storage.
    const auto& ci = functions.class_rt.get(static_class);
    for (const auto& kv : ci.merged_fields) {
        const std::string& fname = kv.first;
        const ast::Type& ftype = kv.second;
        method_env.define_ref(fname, LValue::field_of(self, fname), ast::Type::Ref(ftype));
    }
}

inline Value call_builtin(const std::string& name, const std::vector<Value>& args) {
    if (name == "print_int") {
        std::cout << expect_int(args.at(0), "print_int") << "\n";
        return Value{0};
    }
    if (name == "print_bool") {
        bool b = expect_bool(args.at(0), "print_bool");
        std::cout << (b ? "true" : "false") << "\n";
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

inline Value call_function(Env& env, ast::FunctionDef& f, const std::vector<Value>& arg_vals, FunctionTable& functions) {
    Env callee(&env);

    // bind parameters
    for (size_t i = 0; i < f.params.size(); ++i) {
        const auto& p = f.params[i];
        const auto& t = p.type;

        if (t.is_ref) {
            // reference arg must come from an lvalue in the caller; we pass it as ObjectPtr/Value is not enough here.
            // In this interpreter, ref passing is handled by the AST: the caller will bind ref-params via eval_lvalue.
            // So for free functions we expect the caller to have created a RefSlot already if needed.
            // Here: treat it as value-parameter (fallback) -> should be avoided by semantics/tests.
            callee.define_value(p.name, arg_vals[i], t);
        } else {
            callee.define_value(p.name, arg_vals[i], t);
        }
    }

    try {
        exec_stmt(callee, *f.body, functions);
    } catch (const ReturnSignal& rs) {
        return rs.value;
    }
    return Value{0};
}

inline Value call_method(Env& env,
                         const ObjectPtr& self,
                         const std::string& static_class,
                         const std::string& dynamic_class,
                         const ast::MethodDef& m,
                         const std::vector<Value>& arg_vals,
                         const std::vector<LValue>& arg_lvals,
                         FunctionTable& functions) {
    // Methods have access to fields + their params/locals.
    // No "this": fields are injected as refs.
    Env method_env(&env);

    bind_fields_as_refs(method_env, self, static_class, functions);

    for (size_t i = 0; i < m.params.size(); ++i) {
        const auto& p = m.params[i];
        const auto& t = p.type;

        if (t.is_ref) {
            method_env.define_ref(p.name, arg_lvals[i], t);
        } else {
            method_env.define_value(p.name, arg_vals[i], t);
        }
    }

    try {
        exec_stmt(method_env, *m.body, functions);
    } catch (const ReturnSignal& rs) {
        return rs.value;
    }
    return Value{0};
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
            Value init = v->init ? eval_expr(env, *v->init, functions) : default_value_for_type(t, functions);
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

    if (auto* ce = dynamic_cast<const ConstructExpr*>(&e)) {
        // Evaluate ctor args (we do not execute ctor body yet; we just construct default + set dynamic_class)
        (void)ce; // keep unused-warning away if ctors not executed yet
        return Value{default_construct_object(ce->class_name, functions)};
    }

    if (auto* call = dynamic_cast<const CallExpr*>(&e)) {
        // builtins
        if (call->callee.rfind("print_", 0) == 0) {
            std::vector<Value> args;
            args.reserve(call->args.size());
            for (auto& a2 : call->args) args.push_back(eval_expr(env, *a2, functions));
            return call_builtin(call->callee, args);
        }

        std::vector<Value> arg_vals;
        std::vector<ast::Type> arg_types;
        arg_vals.reserve(call->args.size());
        arg_types.reserve(call->args.size());

        for (auto& a2 : call->args) {
            arg_vals.push_back(eval_expr(env, *a2, functions));
            // runtime: type tracking not fully implemented -> assume sema ensured correct overload.
            // We still pass "base types" derived from the AST by reading param types later; for now: use Int as placeholder if unknown.
            // In practice: parser+sema will let us compute these properly.
            arg_types.push_back(ast::Type::Int(false));
        }

        ast::FunctionDef& f = functions.resolve(call->callee, arg_types);
        return call_function(env, f, arg_vals, functions);
    }

    if (auto* mc = dynamic_cast<const MethodCallExpr*>(&e)) {
        Value objv = eval_expr(env, *mc->object, functions);
        auto* pobj = std::get_if<ObjectPtr>(&objv);
        if (!pobj || !*pobj) throw std::runtime_error("method call on non-object");

        ObjectPtr self = *pobj;
        std::string dynamic_class = self->dynamic_class;

        // static class: only reliable for VarExpr right now (good enough for B& b = d; b.m()).
        std::string static_class = dynamic_class;
        bool call_via_ref = false;

        if (auto* ov = dynamic_cast<const VarExpr*>(mc->object.get())) {
            ast::Type st = env.static_type_of(ov->name);
            call_via_ref = st.is_ref;
            if (ast::Type::Base::Class == st.base) static_class = st.class_name;
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
            // for ref params we need both: value + lvalue
            bool is_lv = dynamic_cast<const VarExpr*>(a2.get()) != nullptr
                      || dynamic_cast<const MemberAccessExpr*>(a2.get()) != nullptr;

            arg_is_lvalue.push_back(is_lv);

            if (is_lv) arg_lvals.push_back(eval_lvalue(env, *a2, functions));
            else arg_lvals.push_back(LValue{}); // unused if not ref-param

            arg_vals.push_back(eval_expr(env, *a2, functions));

            // placeholder (see note in CallExpr)
            arg_types.push_back(ast::Type::Int(false));
        }

        const ast::MethodDef& target =
            functions.class_rt.resolve_method(static_class, dynamic_class,
                                              mc->method, arg_types, arg_is_lvalue, call_via_ref);

        return call_method(env, self, static_class, dynamic_class, target, arg_vals, arg_lvals, functions);
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
