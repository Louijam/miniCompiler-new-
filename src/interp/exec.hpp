#pragma once
// Verhindert mehrfaches Einbinden dieser Header-Datei

#include <stdexcept>   // std::runtime_error
#include <vector>      // std::vector
#include <iostream>    // std::cout

#include "env.hpp"         // Laufzeit-Umgebung / Scopes
#include "functions.hpp"   // Funktions- und Klassen-Runtime
#include "assign.hpp"      // slicing-aware assignment
#include "lvalue.hpp"      // LValue (Variable oder Feld)
#include "object.hpp"      // Objekt-Repräsentation
#include "value.hpp"       // Laufzeitwerte
#include "../ast/stmt.hpp" // AST Statements
#include "../ast/expr.hpp" // AST Expressions
#include "../ast/type.hpp" // AST Typen

namespace interp {

// Signal zum Abbrechen der Ausführung bei return
struct ReturnSignal {
    bool has_value = false; // true wenn "return expr;" genutzt wurde
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

// Vorwärtsdeklarationen (werden weiter unten definiert)
inline void exec_stmt(Env& env, const ast::Stmt& s, FunctionTable& functions);
inline void bind_fields_as_refs_dynamic(Env& method_env,
                                        const ObjectPtr& self,
                                        FunctionTable& functions);

// Ruft einen Konstruktor-Body auf (Felder als Referenzen gebunden)
inline void run_ctor_body(Env& caller_env,
                          const ObjectPtr& self,
                          const ast::ConstructorDef& ctor,
                          const std::vector<Value>& arg_vals,
                          const std::vector<LValue>& arg_lvals,
                          FunctionTable& functions) {
    Env ctor_env(&caller_env);
    bind_fields_as_refs_dynamic(ctor_env, self, functions);

    for (size_t i = 0; i < ctor.params.size(); ++i) {
        const auto& p = ctor.params[i];
        if (p.type.is_ref)
            ctor_env.define_ref(p.name, arg_lvals[i], p.type);
        else
            ctor_env.define_value(p.name, arg_vals[i], p.type);
    }

    // synthetischer Default-CTOR hat leeren Body
    if (ctor.body) {
        exec_stmt(ctor_env, *ctor.body, functions);
    }
}

// Konstruktoraufruf: erst Base()-Default, dann eigener Body
inline void run_ctor_chain(Env& caller_env,
                           const ObjectPtr& self,
                           const std::string& class_name,
                           const ast::ConstructorDef& ctor,
                           const std::vector<Value>& arg_vals,
                           const std::vector<LValue>& arg_lvals,
                           FunctionTable& functions) {
    const auto& ci = functions.class_rt.get(class_name);

    if (!ci.base.empty()) {
        // Base(): immer parameterloser Default-CTOR
        std::vector<ast::Type> empty_t;
        std::vector<bool> empty_lv;
        const ast::ConstructorDef& base_ctor = functions.class_rt.resolve_ctor(ci.base, empty_t, empty_lv);
        std::vector<Value> empty_vals;
        std::vector<LValue> empty_lvals;
        run_ctor_chain(caller_env, self, ci.base, base_ctor, empty_vals, empty_lvals, functions);
    }

    run_ctor_body(caller_env, self, ctor, arg_vals, arg_lvals, functions);
}

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

// Kopiert einen Klassenwert als echten Wert (deep copy + ggf. slicing)
inline Value copy_class_value_for_static_type(const Value& v,
                                              const ast::Type& static_t,
                                              FunctionTable& functions) {
    if (static_t.base != ast::Type::Base::Class || static_t.is_ref) return v;

    auto* src = std::get_if<ObjectPtr>(&v);
    if (!src || !*src) throw std::runtime_error("expected object value");

    Value copied = deep_copy_value(v);
    auto* objp = std::get_if<ObjectPtr>(&copied);
    if (!objp || !*objp) throw std::runtime_error("copy failed");

    if ((*objp)->dynamic_class != static_t.class_name) {
        const auto& ci = functions.class_rt.get(static_t.class_name);
        (*objp)->slice_to(static_t.class_name, ci.merged_fields);
        (*objp)->dynamic_class = static_t.class_name;
    }
    return copied;
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
        if (f.return_type.base == ast::Type::Base::Void) {
            if (rs.has_value)
                throw std::runtime_error("type error: void function must not return a value");
            return Value{0};
        }
        if (!rs.has_value)
            throw std::runtime_error("type error: non-void function must return a value");
        return rs.value;
    }
    // Kein expliziter return
    if (f.return_type.base == ast::Type::Base::Void) return Value{0};
    return default_value_for_type(f.return_type, functions);
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
        if (m.return_type.base == ast::Type::Base::Void) {
            if (rs.has_value)
                throw std::runtime_error("type error: void method must not return a value");
            return Value{0};
        }
        if (!rs.has_value)
            throw std::runtime_error("type error: non-void method must return a value");
        return rs.value;
    }
    if (m.return_type.base == ast::Type::Base::Void) return Value{0};
    return default_value_for_type(m.return_type, functions);
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

            // Klassenwerte sind Werte: deep copy + ggf. slicing zum statischen Typ
            if (t.base == ast::Type::Base::Class)
                init = copy_class_value_for_static_type(init, t, functions);

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
        ReturnSignal rs;
        if (r->value) {
            rs.has_value = true;
            rs.value = eval_expr(env, *r->value, functions);
        } else {
            rs.has_value = false;
            rs.value = Value{0};
        }
        throw rs;
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

    // Unär
    if (auto* u = dynamic_cast<const UnaryExpr*>(&e)) {
        Value v = eval_expr(env, *u->expr, functions);
        if (u->op == UnaryExpr::Op::Neg) {
            return Value{ -expect_int(v, "unary -") };
        }
        if (u->op == UnaryExpr::Op::Not) {
            return Value{ !expect_bool(v, "unary !") };
        }
        throw std::runtime_error("unknown unary operator");
    }

    // Binär
    if (auto* b = dynamic_cast<const BinaryExpr*>(&e)) {
        // Short-circuit fuer && / ||
        if (b->op == BinaryExpr::Op::AndAnd) {
            bool left = to_bool_like_cpp(eval_expr(env, *b->left, functions));
            if (!left) return Value{false};
            bool right = to_bool_like_cpp(eval_expr(env, *b->right, functions));
            return Value{right};
        }
        if (b->op == BinaryExpr::Op::OrOr) {
            bool left = to_bool_like_cpp(eval_expr(env, *b->left, functions));
            if (left) return Value{true};
            bool right = to_bool_like_cpp(eval_expr(env, *b->right, functions));
            return Value{right};
        }

        Value lv = eval_expr(env, *b->left, functions);
        Value rv = eval_expr(env, *b->right, functions);

        switch (b->op) {
            case BinaryExpr::Op::Add: return Value{ expect_int(lv, "+") + expect_int(rv, "+") };
            case BinaryExpr::Op::Sub: return Value{ expect_int(lv, "-") - expect_int(rv, "-") };
            case BinaryExpr::Op::Mul: return Value{ expect_int(lv, "*") * expect_int(rv, "*") };
            case BinaryExpr::Op::Div: {
                int r = expect_int(rv, "/");
                if (r == 0) throw std::runtime_error("runtime error: division by zero");
                return Value{ expect_int(lv, "/") / r };
            }
            case BinaryExpr::Op::Mod: {
                int r = expect_int(rv, "%");
                if (r == 0) throw std::runtime_error("runtime error: modulo by zero");
                return Value{ expect_int(lv, "%") % r };
            }
            case BinaryExpr::Op::Lt: {
                if (auto* li = std::get_if<int>(&lv)) {
                    return Value{ *li < expect_int(rv, "<") };
                }
                if (auto* lc = std::get_if<char>(&lv)) {
                    auto* rc = std::get_if<char>(&rv);
                    if (!rc) throw std::runtime_error("type error: expected char in <");
                    return Value{ *lc < *rc };
                }
                throw std::runtime_error("type error: invalid operands for <");
            }
            case BinaryExpr::Op::Le: {
                if (auto* li = std::get_if<int>(&lv)) {
                    return Value{ *li <= expect_int(rv, "<=") };
                }
                if (auto* lc = std::get_if<char>(&lv)) {
                    auto* rc = std::get_if<char>(&rv);
                    if (!rc) throw std::runtime_error("type error: expected char in <=");
                    return Value{ *lc <= *rc };
                }
                throw std::runtime_error("type error: invalid operands for <=");
            }
            case BinaryExpr::Op::Gt: {
                if (auto* li = std::get_if<int>(&lv)) {
                    return Value{ *li > expect_int(rv, ">") };
                }
                if (auto* lc = std::get_if<char>(&lv)) {
                    auto* rc = std::get_if<char>(&rv);
                    if (!rc) throw std::runtime_error("type error: expected char in >");
                    return Value{ *lc > *rc };
                }
                throw std::runtime_error("type error: invalid operands for >");
            }
            case BinaryExpr::Op::Ge: {
                if (auto* li = std::get_if<int>(&lv)) {
                    return Value{ *li >= expect_int(rv, ">=") };
                }
                if (auto* lc = std::get_if<char>(&lv)) {
                    auto* rc = std::get_if<char>(&rv);
                    if (!rc) throw std::runtime_error("type error: expected char in >=");
                    return Value{ *lc >= *rc };
                }
                throw std::runtime_error("type error: invalid operands for >=");
            }
            case BinaryExpr::Op::Eq: {
                if (lv.index() != rv.index()) throw std::runtime_error("type error: == requires same types");
                if (auto* li = std::get_if<int>(&lv)) return Value{ *li == std::get<int>(rv) };
                if (auto* lb = std::get_if<bool>(&lv)) return Value{ *lb == std::get<bool>(rv) };
                if (auto* lc = std::get_if<char>(&lv)) return Value{ *lc == std::get<char>(rv) };
                if (auto* ls = std::get_if<std::string>(&lv)) return Value{ *ls == std::get<std::string>(rv) };
                throw std::runtime_error("type error: unsupported ==");
            }
            case BinaryExpr::Op::Ne: {
                if (lv.index() != rv.index()) throw std::runtime_error("type error: != requires same types");
                if (auto* li = std::get_if<int>(&lv)) return Value{ *li != std::get<int>(rv) };
                if (auto* lb = std::get_if<bool>(&lv)) return Value{ *lb != std::get<bool>(rv) };
                if (auto* lc = std::get_if<char>(&lv)) return Value{ *lc != std::get<char>(rv) };
                if (auto* ls = std::get_if<std::string>(&lv)) return Value{ *ls != std::get<std::string>(rv) };
                throw std::runtime_error("type error: unsupported !=");
            }
            default:
                break;
        }
    }

    // Zuweisung (slicing-aware)
    if (auto* a = dynamic_cast<const AssignExpr*>(&e)) {
        Value rhs = eval_expr(env, *a->value, functions);
        assign_value_slicing_aware(env, a->name, rhs, functions);
        return rhs;
    }

    // Feldzugriff / Feldzuweisung
    if (auto* fa = dynamic_cast<const FieldAssignExpr*>(&e)) {
        // object.f = rhs
        Value objv = eval_expr(env, *fa->object, functions);
        auto* pobj = std::get_if<ObjectPtr>(&objv);
        if (!pobj || !*pobj)
            throw std::runtime_error("field assignment on non-object");
        LValue lv = LValue::field_of(*pobj, fa->field);
        Value rhs = eval_expr(env, *fa->value, functions);
        env.write_lvalue(lv, rhs);
        return rhs;
    }

    if (dynamic_cast<const MemberAccessExpr*>(&e)) {
        LValue lv = eval_lvalue(env, e, functions);
        return env.read_lvalue(lv);
    }

    // Funktionsaufruf
    if (auto* c = dynamic_cast<const CallExpr*>(&e)) {
        std::vector<Value> arg_vals;
        std::vector<LValue> arg_lvals;
        std::vector<ast::Type> arg_types;
        std::vector<bool> arg_is_lv;

        arg_vals.reserve(c->args.size());
        arg_lvals.reserve(c->args.size());
        arg_types.reserve(c->args.size());
        arg_is_lv.reserve(c->args.size());

        for (const auto& ap : c->args) {
            bool islv = is_lvalue_expr(*ap);
            arg_is_lv.push_back(islv);

            Value v = eval_expr(env, *ap, functions);
            arg_vals.push_back(v);
            arg_types.push_back(type_of_value(v));

            if (islv) arg_lvals.push_back(eval_lvalue(env, *ap, functions));
            else arg_lvals.push_back(LValue{});
        }

        // builtins
        if (c->callee == "print_int" || c->callee == "print_bool" ||
            c->callee == "print_char" || c->callee == "print_string") {
            return call_builtin(c->callee, arg_vals);
        }

        ast::FunctionDef& f = functions.resolve(c->callee, arg_types, arg_is_lv);
        return call_function(env, f, arg_vals, arg_lvals, functions);
    }

    // Konstruktion: T(args)
    if (auto* ce = dynamic_cast<const ConstructExpr*>(&e)) {
        std::vector<Value> arg_vals;
        std::vector<LValue> arg_lvals;
        std::vector<ast::Type> arg_types;
        std::vector<bool> arg_is_lv;

        for (const auto& ap : ce->args) {
            bool islv = is_lvalue_expr(*ap);
            arg_is_lv.push_back(islv);

            Value v = eval_expr(env, *ap, functions);
            arg_vals.push_back(v);
            arg_types.push_back(type_of_value(v));

            if (islv) arg_lvals.push_back(eval_lvalue(env, *ap, functions));
            else arg_lvals.push_back(LValue{});
        }

        ObjectPtr obj = allocate_object_with_default_fields(ce->class_name, functions);

        try {
            const ast::ConstructorDef& ctor = functions.class_rt.resolve_ctor(ce->class_name, arg_types, arg_is_lv);
            run_ctor_chain(env, obj, ce->class_name, ctor, arg_vals, arg_lvals, functions);
        } catch (const std::runtime_error& ex) {
            // Impliziter Copy-Ctor: T(x) wobei x ein Objekt ist (auch D->B mit Slicing)
            if (ce->args.size() == 1) {
                if (auto* src_obj = std::get_if<ObjectPtr>(&arg_vals[0])) {
                    if (*src_obj) {
                        Value copied = copy_class_value_for_static_type(arg_vals[0], ast::Type::Class(ce->class_name, false), functions);
                        return copied;
                    }
                }
            }
            throw; // echter Konstruktor-Fehler
        }

        return Value{obj};
    }

    // Methodenaufruf: obj.m(args)
    if (auto* mc = dynamic_cast<const MethodCallExpr*>(&e)) {
        // Objekt auswerten
        Value objv = eval_expr(env, *mc->object, functions);
        auto* pobj = std::get_if<ObjectPtr>(&objv);
        if (!pobj || !*pobj)
            throw std::runtime_error("method call on non-object");
        ObjectPtr self = *pobj;

        // Argumente
        std::vector<Value> arg_vals;
        std::vector<LValue> arg_lvals;
        std::vector<ast::Type> arg_types;
        std::vector<bool> arg_is_lv;

        for (const auto& ap : mc->args) {
            bool islv = is_lvalue_expr(*ap);
            arg_is_lv.push_back(islv);

            Value v = eval_expr(env, *ap, functions);
            arg_vals.push_back(v);
            arg_types.push_back(type_of_value(v));

            if (islv) arg_lvals.push_back(eval_lvalue(env, *ap, functions));
            else arg_lvals.push_back(LValue{});
        }

        // Statischer Typ + call_via_ref bestimmen (Polymorphie nur ueber Referenzen)
        std::string static_class = self->dynamic_class;
        bool call_via_ref = false;

        if (auto* ve = dynamic_cast<const VarExpr*>(mc->object.get())) {
            ast::Type st = env.static_type_of(ve->name);
            if (st.base == ast::Type::Base::Class) static_class = st.class_name;
            call_via_ref = env.is_ref_var(ve->name);
        }

        const ast::MethodDef& target = functions.class_rt.resolve_method(
            static_class,
            self->dynamic_class,
            mc->method,
            arg_types,
            arg_is_lv,
            call_via_ref
        );

        return call_method(env, self, static_class, target, arg_vals, arg_lvals, functions);
    }

    throw std::runtime_error("unknown expression");
}

} // namespace interp
