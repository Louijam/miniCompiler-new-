#pragma once
// Verhindert mehrfaches Einbinden dieser Header-Datei

#include <stdexcept>   // std::runtime_error
#include <vector>      // std::vector
#include <iostream>    // std::cout

#include "env.hpp"         // Laufzeit-Umgebung / Scopes
#include "functions.hpp"   // Funktions- und Klassen-Runtime
#include "lvalue.hpp"      // LValue (Variable oder Feld)
#include "object.hpp"      // Objekt-Repräsentation
#include "value.hpp"       // Laufzeitwerte
#include "../ast/stmt.hpp" // AST Statements
#include "../ast/expr.hpp" // AST Expressions
#include "../ast/type.hpp" // AST Typen

namespace interp {

// Signal zum Abbrechen der Ausführung bei return
struct ReturnSignal {
    Value value;           // Rückgabewert
};

// C++-ähnliche Wahrheitswert-Konvertierung
inline bool to_bool_like_cpp(const Value& v) {
    if (auto* pi = std::get_if<int>(&v)) return *pi != 0;
    if (auto* pb = std::get_if<bool>(&v)) return *pb;
    if (auto* pc = std::get_if<char>(&v)) return *pc != '\0';
    if (auto* ps = std::get_if<std::string>(&v)) return !ps->empty();
    throw std::runtime_error("cannot convert to bool");
}

// Erzwingt int-Wert
inline int expect_int(const Value& v, const char* ctx) {
    if (auto* pi = std::get_if<int>(&v)) return *pi;
    throw std::runtime_error(std::string("type error: expected int in ") + ctx);
}

// Erzwingt bool-Wert
inline bool expect_bool(const Value& v, const char* ctx) {
    if (auto* pb = std::get_if<bool>(&v)) return *pb;
    throw std::runtime_error(std::string("type error: expected bool in ") + ctx);
}

// Leitet statischen Typ aus einem Laufzeitwert ab
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

// Vorwärtsdeklaration: Ausdrucksauswertung
inline Value eval_expr(Env& env, const ast::Expr& e, FunctionTable& functions);

// Wertet einen Ausdruck als LValue aus
inline LValue eval_lvalue(Env& env, const ast::Expr& e, FunctionTable& functions) {
    using namespace ast;

    // Variable
    if (auto* v = dynamic_cast<const VarExpr*>(&e))
        return env.resolve_lvalue(v->name);

    // Objektfeld
    if (auto* m = dynamic_cast<const MemberAccessExpr*>(&e)) {
        Value objv = eval_expr(env, *m->object, functions);
        auto* pobj = std::get_if<ObjectPtr>(&objv);
        if (!pobj || !*pobj)
            throw std::runtime_error("member access on non-object");
        return LValue::field_of(*pobj, m->field);
    }

    throw std::runtime_error("expected lvalue");
}

// Prüft, ob ein Ausdruck ein LValue ist
inline bool is_lvalue_expr(const ast::Expr& e) {
    return dynamic_cast<const ast::VarExpr*>(&e) != nullptr
        || dynamic_cast<const ast::MemberAccessExpr*>(&e) != nullptr;
}

// Vorwärtsdeklarationen
inline Value default_value_for_type(const ast::Type& t, FunctionTable& functions);
inline void exec_stmt(Env& env, const ast::Stmt& s, FunctionTable& functions);

// Bindet alle Felder des *dynamischen* Objekts als Referenzen
inline void bind_fields_as_refs_dynamic(Env& method_env,
                                        const ObjectPtr& self,
                                        FunctionTable& functions) {
    const auto& ci = functions.class_rt.get(self->dynamic_class);
    for (const auto& kv : ci.merged_fields) {
        ast::Type rt = kv.second;
        rt.is_ref = true;
        method_env.define_ref(kv.first, LValue::field_of(self, kv.first), rt);
    }
}

// Allokiert ein Objekt mit Default-Feldern
inline ObjectPtr allocate_object_with_default_fields(const std::string& class_name,
                                                     FunctionTable& functions) {
    auto obj = std::make_shared<Object>();
    obj->dynamic_class = class_name;

    const auto& ci = functions.class_rt.get(class_name);
    for (const auto& kv : ci.merged_fields) {
        obj->fields[kv.first] = default_value_for_type(kv.second, functions);
    }
    return obj;
}

// Erzeugt Default-Werte für Typen
inline Value default_value_for_type(const ast::Type& t, FunctionTable& functions) {
    using Base = ast::Type::Base;

    if (t.base == Base::Bool)   return Value{false};
    if (t.base == Base::Int)    return Value{0};
    if (t.base == Base::Char)   return Value{char('\0')};
    if (t.base == Base::String) return Value{std::string("")};
    if (t.base == Base::Class) {
        auto obj = allocate_object_with_default_fields(t.class_name, functions);
        return Value{obj};
    }

    return Value{0};
}

// Builtin-Funktionen (print_*)
inline Value call_builtin(const std::string& name, const std::vector<Value>& args) {
    if (name == "print_int") {
        std::cout << expect_int(args.at(0), "print_int") << "\n";
        return Value{0};
    }
    if (name == "print_bool") {
        bool b = expect_bool(args.at(0), "print_bool");
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

// Aufruf einer freien Funktion
inline Value call_function(Env& caller_env,
                           ast::FunctionDef& f,
                           const std::vector<Value>& arg_vals,
                           const std::vector<LValue>& arg_lvals,
                           FunctionTable& functions) {
    Env callee(&caller_env);

    // Parameter binden
    for (size_t i = 0; i < f.params.size(); ++i) {
        const auto& p = f.params[i];
        if (p.type.is_ref)
            callee.define_ref(p.name, arg_lvals[i], p.type);
        else
            callee.define_value(p.name, arg_vals[i], p.type);
    }

    try {
        exec_stmt(callee, *f.body, functions);
    } catch (const ReturnSignal& rs) {
        return rs.value;
    }
    return Value{0};
}

// Aufruf einer Methode
inline Value call_method(Env& caller_env,
                         const ObjectPtr& self,
                         const std::string& static_class,
                         const ast::MethodDef& m,
                         const std::vector<Value>& arg_vals,
                         const std::vector<LValue>& arg_lvals,
                         FunctionTable& functions) {
    (void)static_class;

    Env method_env(&caller_env);

    // Felder des dynamischen Objekts binden
    bind_fields_as_refs_dynamic(method_env, self, functions);

    // Parameter binden
    for (size_t i = 0; i < m.params.size(); ++i) {
        const auto& p = m.params[i];
        if (p.type.is_ref)
            method_env.define_ref(p.name, arg_lvals[i], p.type);
        else
            method_env.define_value(p.name, arg_vals[i], p.type);
    }

    try {
        exec_stmt(method_env, *m.body, functions);
    } catch (const ReturnSignal& rs) {
        return rs.value;
    }
    return Value{0};
}

// Ausführung eines Statements
inline void exec_stmt(Env& env, const ast::Stmt& s, FunctionTable& functions) {
    using namespace ast;

    // Block
    if (auto* b = dynamic_cast<const BlockStmt*>(&s)) {
        Env local(&env);
        for (auto& st : b->statements)
            exec_stmt(local, *st, functions);
        return;
    }

    // Variablendeklaration
    if (auto* v = dynamic_cast<const VarDeclStmt*>(&s)) {
        const ast::Type& t = v->decl_type;

        if (t.is_ref) {
            if (!v->init)
                throw std::runtime_error("Referenzvariable muss initialisiert werden");
            LValue target = eval_lvalue(env, *v->init, functions);
            env.define_ref(v->name, target, t);
        } else {
            Value init;
            if (v->init)
                init = eval_expr(env, *v->init, functions);
            else
                init = default_value_for_type(t, functions);
            env.define_value(v->name, init, t);
        }
        return;
    }

    // Ausdrucksstatement
    if (auto* e = dynamic_cast<const ExprStmt*>(&s)) {
        eval_expr(env, *e->expr, functions);
        return;
    }

    // If
    if (auto* i = dynamic_cast<const IfStmt*>(&s)) {
        bool cond = to_bool_like_cpp(eval_expr(env, *i->cond, functions));
        if (cond) exec_stmt(env, *i->then_branch, functions);
        else if (i->else_branch) exec_stmt(env, *i->else_branch, functions);
        return;
    }

    // While
    if (auto* w = dynamic_cast<const WhileStmt*>(&s)) {
        while (to_bool_like_cpp(eval_expr(env, *w->cond, functions)))
            exec_stmt(env, *w->body, functions);
        return;
    }

    // Return
    if (auto* r = dynamic_cast<const ReturnStmt*>(&s)) {
        Value v = r->value ? eval_expr(env, *r->value, functions) : Value{0};
        throw ReturnSignal{v};
    }

    throw std::runtime_error("unknown statement");
}

// Ausdrucksauswertung
inline Value eval_expr(Env& env, const ast::Expr& e, FunctionTable& functions) {
    using namespace ast;

    // Literale
    if (auto* i = dynamic_cast<const IntLiteral*>(&e)) return i->value;
    if (auto* b = dynamic_cast<const BoolLiteral*>(&e)) return b->value;
    if (auto* c = dynamic_cast<const CharLiteral*>(&e)) return c->value;
    if (auto* s = dynamic_cast<const StringLiteral*>(&e)) return s->value;

    // Variable
    if (auto* v = dynamic_cast<const VarExpr*>(&e))
        return env.read_value(v->name);

    // Zuweisung
    if (auto* a = dynamic_cast<const AssignExpr*>(&e)) {
        Value rhs = eval_expr(env, *a->value, functions);
        env.assign_value(a->name, rhs);
        return rhs;
    }

    // Feldzugriff / Feldzuweisung
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

    throw std::runtime_error("unknown expression");
}

} // namespace interp
