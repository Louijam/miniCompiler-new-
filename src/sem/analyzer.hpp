#pragma once
// Verhindert mehrfaches Einbinden dieser Header-Datei

#include <stdexcept>      // std::runtime_error
#include <string>         // std::string
#include <vector>         // std::vector
#include <unordered_map>  // (derzeit nicht direkt genutzt, evtl. fuer spaetere Erweiterungen)

#include "scope.hpp"        // Scope: Variablen/Funktionssymbole im aktuellen Kontext
#include "class_table.hpp"  // ClassTable: Klassenhierarchie, Felder, Methoden, Konstruktoren

#include "../ast/expr.hpp"      // AST: Expression-Knoten
#include "../ast/stmt.hpp"      // AST: Statement-Knoten
#include "../ast/function.hpp"  // AST: FunctionDef
#include "../ast/class.hpp"     // AST: ClassDef/MethodDef/ConstructorDef
#include "../ast/type.hpp"      // AST: Type

namespace sem {

// Analyzer: semantische Analyse / Typechecking
// - prüft Typen von Ausdrücken
// - prüft Statements (Return-Typ, Bedingungen, Deklarationen)
// - prüft Funktionen / Methoden / Konstruktoren
struct Analyzer {
    const ClassTable* ct = nullptr; // Zugriff auf Klasseninfos (Base-Relation, Felder, Methoden, Ctors)

    // Setzt die ClassTable, die für Klassen-bezogene Prüfungen benötigt wird
    void set_class_table(const ClassTable* t) { ct = t; }

    // Entfernt Referenz-Flag (T& -> T)
    static ast::Type base_type(ast::Type t) {
        t.is_ref = false;
        return t;
    }

    // Macht aus Type eine lesbare Darstellung für Fehlermeldungen
    static std::string type_name(const ast::Type& t) {
        using B = ast::Type::Base;
        std::string s;
        switch (t.base) {
            case B::Bool:   s = "bool"; break;
            case B::Int:    s = "int"; break;
            case B::Char:   s = "char"; break;
            case B::String: s = "string"; break;
            case B::Void:   s = "void"; break;
            case B::Class:  s = t.class_name; break;
        }
        if (t.is_ref) s += "&";
        return s;
    }

    // lvalue: Variablen und Feldzugriffe können links von '=' stehen
    bool is_lvalue(const ast::Expr& e) const {
        if (dynamic_cast<const ast::VarExpr*>(&e)) return true;
        if (dynamic_cast<const ast::MemberAccessExpr*>(&e)) return true;
        return false;
    }

    // Bedingungen in if/while: "bool-like" (wie ihr es wollt)
    // Wichtig: Referenzen sind hier verboten (kein implizites "bool" auf T&)
    bool is_bool_context_allowed(const ast::Type& t) const {
        using B = ast::Type::Base;
        if (t.is_ref) return false;
        return t.base == B::Bool || t.base == B::Int || t.base == B::Char || t.base == B::String;
    }

    // Prüft Referenzbindung: dst_ref muss T&, init_expr muss lvalue vom gleichen Base-Type sein
    bool can_bind_ref_to_expr(const ast::Type& dst_ref, const ast::Expr& init_expr, const Scope& scope) const {
        if (!dst_ref.is_ref) return false;
        ast::Type src = type_of_expr(scope, init_expr);
        if (base_type(src) != base_type(dst_ref)) return false;
        return is_lvalue(init_expr);
    }

    // Typbestimmung eines Ausdrucks inkl. Overload-Resolution
    ast::Type type_of_expr(const Scope& scope, const ast::Expr& e) const {
        using namespace ast;

        // Literale
        if (auto* b = dynamic_cast<const BoolLitExpr*>(&e))   return Type::Bool();
        if (auto* i = dynamic_cast<const IntLitExpr*>(&e))    return Type::Int();
        if (auto* c = dynamic_cast<const CharLitExpr*>(&e))   return Type::Char();
        if (auto* s = dynamic_cast<const StringLitExpr*>(&e)) return Type::String();

        // Variable
        if (auto* v = dynamic_cast<const VarExpr*>(&e)) {
            if (!scope.has_var(v->name))
                throw std::runtime_error("semantic error: unknown variable: " + v->name);
            return scope.lookup_var(v->name);
        }

        // Unary
        if (auto* u = dynamic_cast<const UnaryExpr*>(&e)) {
            ast::Type t = type_of_expr(scope, *u->rhs);

            if (u->op == "!") {
                if (base_type(t) != Type::Bool())
                    throw std::runtime_error("semantic error: ! expects bool");
                return Type::Bool();
            }

            if (u->op == "+" || u->op == "-") {
                if (base_type(t) != Type::Int())
                    throw std::runtime_error("semantic error: unary +/- expects int");
                return Type::Int();
            }

            throw std::runtime_error("semantic error: unknown unary operator: " + u->op);
        }

        // Binary
        if (auto* b = dynamic_cast<const BinaryExpr*>(&e)) {
            ast::Type lt = type_of_expr(scope, *b->lhs);
            ast::Type rt = type_of_expr(scope, *b->rhs);

            // logische Operatoren
            if (b->op == "&&" || b->op == "||") {
                if (base_type(lt) != Type::Bool() || base_type(rt) != Type::Bool())
                    throw std::runtime_error("semantic error: &&/|| expects bool operands");
                return Type::Bool();
            }

            // equality
            if (b->op == "==" || b->op == "!=") {
                if (base_type(lt) != base_type(rt))
                    throw std::runtime_error("semantic error: ==/!= expects same operand types");

                if (base_type(lt) == Type::Bool() || base_type(lt) == Type::String() ||
                    base_type(lt) == Type::Int()  || base_type(lt) == Type::Char()) {
                    return Type::Bool();
                }
                throw std::runtime_error("semantic error: ==/!= not supported for this type");
            }

            // relational
            if (b->op == "<" || b->op == "<=" || b->op == ">" || b->op == ">=") {
                if (base_type(lt) != base_type(rt))
                    throw std::runtime_error("semantic error: relational op expects same operand types");

                if (base_type(lt) == Type::Int() || base_type(lt) == Type::Char())
                    return Type::Bool();

                throw std::runtime_error("semantic error: relational op not supported for this type");
            }

            // arithmetic
            if (b->op == "+" || b->op == "-" || b->op == "*" || b->op == "/" || b->op == "%") {
                if (base_type(lt) != Type::Int() || base_type(rt) != Type::Int())
                    throw std::runtime_error("semantic error: arithmetic expects int operands");
                return Type::Int();
            }

            throw std::runtime_error("semantic error: unknown binary operator: " + b->op);
        }

        // Assignment (lhs = rhs)
        if (auto* a = dynamic_cast<const AssignExpr*>(&e)) {
            if (!is_lvalue(*a->lhs))
                throw std::runtime_error("semantic error: left side of assignment is not an lvalue");

            ast::Type lt = type_of_expr(scope, *a->lhs);
            ast::Type rt = type_of_expr(scope, *a->rhs);

            // Wenn LHS eine Referenz ist, schreiben wir in das Ziel, aber Typ muss passen
            if (lt.is_ref) {
                if (base_type(lt) != base_type(rt))
                    throw std::runtime_error("semantic error: assignment type mismatch");
                return base_type(rt);
            }

            // Normale Zuweisung
            if (base_type(lt) != base_type(rt)) {
                // Klassen: erlaubt, wenn RHS von LHS ableitet (Slicing beim Wert-Typ möglich)
                if (lt.base == ast::Type::Base::Class && rt.base == ast::Type::Base::Class) {
                    if (!ct) throw std::runtime_error("semantic error: class table not set");
                    if (!ct->is_base_of(lt.class_name, rt.class_name))
                        throw std::runtime_error("semantic error: assignment type mismatch");
                    return base_type(lt);
                }
                throw std::runtime_error("semantic error: assignment type mismatch");
            }

            return base_type(rt);
        }

        // obj.f = rhs
        if (auto* fa = dynamic_cast<const FieldAssignExpr*>(&e)) {
            ast::Type objt = type_of_expr(scope, *fa->obj);
            if (base_type(objt).base != ast::Type::Base::Class)
                throw std::runtime_error("semantic error: field assignment on non-class object");

            if (!ct) throw std::runtime_error("semantic error: class table not set");
            ast::Type ft = ct->field_type_in_chain(base_type(objt).class_name, fa->field);

            ast::Type rt = type_of_expr(scope, *fa->rhs);

            // Feld ist Referenz: Typ muss passen, Zuweisung schreibt in Ziel
            if (ft.is_ref) {
                if (base_type(ft) != base_type(rt))
                    throw std::runtime_error("semantic error: assignment type mismatch");
                return base_type(rt);
            }

            // Wert-Feld: gleiche Base-Type oder (bei Klasse) RHS darf abgeleitet sein
            if (base_type(ft) != base_type(rt)) {
                if (ft.base == ast::Type::Base::Class && rt.base == ast::Type::Base::Class) {
                    if (!ct->is_base_of(ft.class_name, rt.class_name))
                        throw std::runtime_error("semantic error: assignment type mismatch");
                    return base_type(ft);
                }
                throw std::runtime_error("semantic error: assignment type mismatch");
            }

            return base_type(rt);
        }

        // Funktion call: f(args...)
        if (auto* call = dynamic_cast<const CallExpr*>(&e)) {
            if (!scope.has_func(call->callee))
                throw std::runtime_error("semantic error: unknown function: " + call->callee);

            const auto& overloads = scope.lookup_funcs(call->callee);

            // Overload-Resolution per "score"
            int best_score = -1;
            const FuncSymbol* best = nullptr;

            for (const auto& cand : overloads) {
                if (cand.param_types.size() != call->args.size()) continue;

                bool ok = true;
                int score = 0;

                for (size_t i = 0; i < call->args.size(); ++i) {
                    ast::Type at = type_of_expr(scope, *call->args[i]);
                    ast::Type pt = cand.param_types[i];

                    if (pt.is_ref) {
                        // Ref param: braucht lvalue und gleichen Base-Type
                        if (!can_bind_ref_to_expr(pt, *call->args[i], scope)) { ok = false; break; }
                        score += 2;
                    } else {
                        // Value param: exact base type match
                        if (base_type(at) != base_type(pt)) { ok = false; break; }
                        score += 1;
                    }
                }

                if (!ok) continue;

                if (score > best_score) {
                    best_score = score;
                    best = &cand;
                } else if (score == best_score) {
                    throw std::runtime_error("semantic error: ambiguous overload for function: " + call->callee);
                }
            }

            if (!best)
                throw std::runtime_error("semantic error: no matching overload for function: " + call->callee);

            return best->return_type;
        }

        // Konstruktion: T(args...)
        if (auto* ce = dynamic_cast<const ConstructExpr*>(&e)) {
            if (!ct) throw std::runtime_error("semantic error: class table not set");
            if (!ct->has_class(ce->class_name))
                throw std::runtime_error("semantic error: unknown class: " + ce->class_name);

            const auto& overloads = ct->constructors_of(ce->class_name);

            int best_score = -1;
            const ConstructorSymbol* best = nullptr;

            for (const auto& cand : overloads) {
                if (cand.param_types.size() != ce->args.size()) continue;

                bool ok = true;
                int score = 0;

                for (size_t i = 0; i < ce->args.size(); ++i) {
                    ast::Type at = type_of_expr(scope, *ce->args[i]);
                    ast::Type pt = cand.param_types[i];

                    if (pt.is_ref) {
                        if (!can_bind_ref_to_expr(pt, *ce->args[i], scope)) { ok = false; break; }
                        score += 2;
                    } else {
                        if (base_type(at) != base_type(pt)) { ok = false; break; }
                        score += 1;
                    }
                }

                if (!ok) continue;

                if (score > best_score) {
                    best_score = score;
                    best = &cand;
                } else if (score == best_score) {
                    throw std::runtime_error("semantic error: ambiguous overload for constructor: " + ce->class_name);
                }
            }

            if (!best)
                throw std::runtime_error("semantic error: no matching overload for constructor: " + ce->class_name);

            return ast::Type::Class(ce->class_name);
        }

        // Feldzugriff: obj.f
        if (auto* ma = dynamic_cast<const MemberAccessExpr*>(&e)) {
            ast::Type objt = type_of_expr(scope, *ma->obj);
            if (base_type(objt).base != ast::Type::Base::Class)
                throw std::runtime_error("semantic error: member access on non-class object");

            if (!ct) throw std::runtime_error("semantic error: class table not set");
            return ct->field_type_in_chain(base_type(objt).class_name, ma->member);
        }

        // Method call: obj.m(args...)
        if (auto* mc = dynamic_cast<const MethodCallExpr*>(&e)) {
            ast::Type objt = type_of_expr(scope, *mc->obj);
            if (base_type(objt).base != ast::Type::Base::Class)
                throw std::runtime_error("semantic error: method call on non-class object");

            if (!ct) throw std::runtime_error("semantic error: class table not set");

            const auto& overloads = ct->methods_of_in_chain(base_type(objt).class_name, mc->method);

            int best_score = -1;
            const MethodSymbol* best = nullptr;

            for (const auto& cand : overloads) {
                if (cand.param_types.size() != mc->args.size()) continue;

                bool ok = true;
                int score = 0;

                for (size_t i = 0; i < mc->args.size(); ++i) {
                    ast::Type at = type_of_expr(scope, *mc->args[i]);
                    ast::Type pt = cand.param_types[i];

                    if (pt.is_ref) {
                        if (!can_bind_ref_to_expr(pt, *mc->args[i], scope)) { ok = false; break; }
                        score += 2;
                    } else {
                        if (base_type(at) != base_type(pt)) { ok = false; break; }
                        score += 1;
                    }
                }

                if (!ok) continue;

                if (score > best_score) {
                    best_score = score;
                    best = &cand;
                } else if (score == best_score) {
                    throw std::runtime_error("semantic error: ambiguous overload for method: " + mc->method);
                }
            }

            if (!best)
                throw std::runtime_error("semantic error: no matching overload for method: " + mc->method);

            return best->return_type;
        }

        throw std::runtime_error("semantic error: unknown expression node");
    }

    // Prüft ein Statement gegen erwarteten Return-Typ (für Funktionen/Methoden)
    void check_stmt(Scope& scope, const ast::Stmt& s, const ast::Type& expected_return) const {
        using namespace ast;

        // Block: neuer Scope
        if (auto* b = dynamic_cast<const BlockStmt*>(&s)) {
            Scope inner(&scope);
            for (const auto& st : b->stmts) check_stmt(inner, *st, expected_return);
            return;
        }

        // VarDecl
        if (auto* v = dynamic_cast<const VarDeclStmt*>(&s)) {
            if (scope.has_var_local(v->name))
                throw std::runtime_error("semantic error: duplicate variable in scope: " + v->name);

            // Referenzen muessen initialisiert werden
            if (v->type.is_ref && !v->init)
                throw std::runtime_error("semantic error: reference variable must be initialized: " + v->name);

            if (v->init) {
                if (v->type.is_ref) {
                    // ref muss an lvalue binden können
                    if (!can_bind_ref_to_expr(v->type, *v->init, scope))
                        throw std::runtime_error("semantic error: cannot bind reference to rvalue: " + v->name);
                } else {
                    // value init: exact base type match (hier keine Klassen-Konversion implementiert)
                    ast::Type it = type_of_expr(scope, *v->init);
                    if (base_type(it) != base_type(v->type)) {
                        throw std::runtime_error("semantic error: initializer type mismatch for variable: " + v->name);
                    }
                }
            }

            scope.define_var(v->name, v->type);
            return;
        }

        // Ausdrucksstatement: nur Typechecken
        if (auto* e = dynamic_cast<const ExprStmt*>(&s)) {
            (void)type_of_expr(scope, *e->expr);
            return;
        }

        // if: Bedingung bool-like, branches prüfen
        if (auto* i = dynamic_cast<const IfStmt*>(&s)) {
            ast::Type ct2 = type_of_expr(scope, *i->cond);
            if (!is_bool_context_allowed(ct2))
                throw std::runtime_error("semantic error: if condition not convertible to bool: " + type_name(ct2));
            check_stmt(scope, *i->then_branch, expected_return);
            if (i->else_branch) check_stmt(scope, *i->else_branch, expected_return);
            return;
        }

        // while: Bedingung bool-like, body prüfen
        if (auto* w = dynamic_cast<const WhileStmt*>(&s)) {
            ast::Type ct2 = type_of_expr(scope, *w->cond);
            if (!is_bool_context_allowed(ct2))
                throw std::runtime_error("semantic error: while condition not convertible to bool: " + type_name(ct2));
            check_stmt(scope, *w->body, expected_return);
            return;
        }

        // return: muss expected_return erfüllen
        if (auto* r = dynamic_cast<const ReturnStmt*>(&s)) {
            if (expected_return == Type::Void()) {
                if (r->value) throw std::runtime_error("semantic error: return with value in void function");
                return;
            }
            if (!r->value) throw std::runtime_error("semantic error: missing return value");

            ast::Type rt = type_of_expr(scope, *r->value);
            if (base_type(rt) != base_type(expected_return)) {
                throw std::runtime_error("semantic error: return type mismatch: expected " +
                    type_name(expected_return) + ", got " + type_name(rt));
            }
            return;
        }

        throw std::runtime_error("semantic error: unknown statement node");
    }

    // Prüft freie Funktion (globaler Scope + Parameter + Body)
    void check_function(const Scope& global, const ast::FunctionDef& f) const {
        // Funktionsscope hängt an global, aber wir wollen lokale Variablen hinzufügen
        Scope fun_scope(const_cast<Scope*>(&global));

        // Parameter definieren
        for (const auto& p : f.params) {
            if (fun_scope.has_var_local(p.name))
                throw std::runtime_error("semantic error: duplicate parameter name: " + p.name);
            fun_scope.define_var(p.name, p.type);
        }

        check_stmt(fun_scope, *f.body, f.return_type);
    }

    // Prüft Methode: Felder im Member-Scope sichtbar + Parameter + Body gegen ReturnType
    void check_method(const Scope& global,
                      const ClassTable& ctab,
                      const std::string& class_name,
                      const ast::MethodDef& m) const {
        Scope member_scope(const_cast<Scope*>(&global));

        // Felder der Klasse (inkl. geerbte) als "implizite Variablen" im Methodenkörper
        const auto& fields = ctab.merged_fields_for(class_name);
        for (const auto& f : fields) member_scope.define_var(f.name, f.type);

        Scope method_scope(&member_scope);

        // Parameter: dürfen keine Felder shadowen und keine Duplicates sein
        for (const auto& p : m.params) {
            if (ctab.has_field_in_chain(class_name, p.name)) {
                throw std::runtime_error("semantic error: parameter shadows field in method: " + p.name);
            }
            if (method_scope.has_var_local(p.name))
                throw std::runtime_error("semantic error: duplicate parameter name: " + p.name);
            method_scope.define_var(p.name, p.type);
        }

        check_stmt(method_scope, *m.body, m.return_type);
    }

    // Prüft Konstruktor: Felder sichtbar + Parameter + Body (expected return ist void)
    void check_constructor(const Scope& global,
                           const ClassTable& ctab,
                           const std::string& class_name,
                           const ast::ConstructorDef& ctor) const {
        Scope member_scope(const_cast<Scope*>(&global));

        const auto& fields = ctab.merged_fields_for(class_name);
        for (const auto& f : fields) member_scope.define_var(f.name, f.type);

        Scope ctor_scope(&member_scope);

        // Parameter: keine Shadowing der Felder, keine Duplicates
        for (const auto& p : ctor.params) {
            if (ctab.has_field_in_chain(class_name, p.name)) {
                throw std::runtime_error("semantic error: parameter shadows field in constructor: " + p.name);
            }
            if (ctor_scope.has_var_local(p.name))
                throw std::runtime_error("semantic error: duplicate parameter name: " + p.name);
            ctor_scope.define_var(p.name, p.type);
        }

        // Konstruktor hat keinen Return-Wert
        check_stmt(ctor_scope, *ctor.body, ast::Type::Void());
    }
};

} // namespace sem
