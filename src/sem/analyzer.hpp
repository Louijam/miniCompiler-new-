#pragma once
#include <stdexcept>
#include <string>
#include <vector>
#include <unordered_map>

#include "scope.hpp"
#include "class_table.hpp"
#include "../ast/expr.hpp"
#include "../ast/stmt.hpp"
#include "../ast/function.hpp"
#include "../ast/class.hpp"
#include "../ast/type.hpp"

namespace sem {

struct Analyzer {
    const ClassTable* ct = nullptr;

    void set_class_table(const ClassTable* t) { ct = t; }

    static ast::Type base_type(ast::Type t) {
        t.is_ref = false;
        return t;
    }

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

    bool is_lvalue(const ast::Expr& e) const {
        if (dynamic_cast<const ast::VarExpr*>(&e)) return true;
        if (dynamic_cast<const ast::MemberAccessExpr*>(&e)) return true;
        return false;
    }

    bool is_bool_context_allowed(const ast::Type& t) const {
        using B = ast::Type::Base;
        if (t.is_ref) return false;
        return t.base == B::Bool || t.base == B::Int || t.base == B::Char || t.base == B::String;
    }

    bool can_bind_ref_to_expr(const ast::Type& dst_ref, const ast::Expr& init_expr, const Scope& scope) const {
        if (!dst_ref.is_ref) return false;
        ast::Type src = type_of_expr(scope, init_expr);
        if (base_type(src) != base_type(dst_ref)) return false;
        return is_lvalue(init_expr);
    }

    ast::Type type_of_expr(const Scope& scope, const ast::Expr& e) const {
        using namespace ast;

        if (auto* b = dynamic_cast<const BoolLitExpr*>(&e)) return Type::Bool();
        if (auto* i = dynamic_cast<const IntLitExpr*>(&e)) return Type::Int();
        if (auto* c = dynamic_cast<const CharLitExpr*>(&e)) return Type::Char();
        if (auto* s = dynamic_cast<const StringLitExpr*>(&e)) return Type::String();

        if (auto* v = dynamic_cast<const VarExpr*>(&e)) {
            if (!scope.has_var(v->name))
                throw std::runtime_error("semantic error: unknown variable: " + v->name);
            return scope.lookup_var(v->name);
        }

        if (auto* u = dynamic_cast<const UnaryExpr*>(&e)) {
            ast::Type t = type_of_expr(scope, *u->rhs);
            if (u->op == "!") {
                if (base_type(t) != Type::Bool()) throw std::runtime_error("semantic error: ! expects bool");
                return Type::Bool();
            }
            if (u->op == "+" || u->op == "-") {
                if (base_type(t) != Type::Int()) throw std::runtime_error("semantic error: unary +/- expects int");
                return Type::Int();
            }
            throw std::runtime_error("semantic error: unknown unary operator: " + u->op);
        }

        if (auto* b = dynamic_cast<const BinaryExpr*>(&e)) {
            ast::Type lt = type_of_expr(scope, *b->lhs);
            ast::Type rt = type_of_expr(scope, *b->rhs);

            if (b->op == "&&" || b->op == "||") {
                if (base_type(lt) != Type::Bool() || base_type(rt) != Type::Bool())
                    throw std::runtime_error("semantic error: &&/|| expects bool operands");
                return Type::Bool();
            }

            if (b->op == "==" || b->op == "!=") {
                if (base_type(lt) != base_type(rt))
                    throw std::runtime_error("semantic error: ==/!= expects same operand types");
                if (base_type(lt) == Type::Bool() || base_type(lt) == Type::String() ||
                    base_type(lt) == Type::Int() || base_type(lt) == Type::Char()) {
                    return Type::Bool();
                }
                throw std::runtime_error("semantic error: ==/!= not supported for this type");
            }

            if (b->op == "<" || b->op == "<=" || b->op == ">" || b->op == ">=") {
                if (base_type(lt) != base_type(rt))
                    throw std::runtime_error("semantic error: relational op expects same operand types");
                if (base_type(lt) == Type::Int() || base_type(lt) == Type::Char())
                    return Type::Bool();
                throw std::runtime_error("semantic error: relational op not supported for this type");
            }

            if (b->op == "+" || b->op == "-" || b->op == "*" || b->op == "/" || b->op == "%") {
                if (base_type(lt) != Type::Int() || base_type(rt) != Type::Int())
                    throw std::runtime_error("semantic error: arithmetic expects int operands");
                return Type::Int();
            }

            throw std::runtime_error("semantic error: unknown binary operator: " + b->op);
        }

        if (auto* a = dynamic_cast<const AssignExpr*>(&e)) {
            if (!is_lvalue(*a->lhs))
                throw std::runtime_error("semantic error: left side of assignment is not an lvalue");

            ast::Type lt = type_of_expr(scope, *a->lhs);
            ast::Type rt = type_of_expr(scope, *a->rhs);

            if (lt.is_ref) {
                if (base_type(lt) != base_type(rt))
                    throw std::runtime_error("semantic error: assignment type mismatch");
                return base_type(rt);
            }

            if (base_type(lt) != base_type(rt)) {
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

        if (auto* fa = dynamic_cast<const FieldAssignExpr*>(&e)) {
            ast::Type objt = type_of_expr(scope, *fa->obj);
            if (base_type(objt).base != ast::Type::Base::Class)
                throw std::runtime_error("semantic error: field assignment on non-class object");

            if (!ct) throw std::runtime_error("semantic error: class table not set");
            ast::Type ft = ct->field_type_in_chain(base_type(objt).class_name, fa->field);

            ast::Type rt = type_of_expr(scope, *fa->rhs);

            if (ft.is_ref) {
                if (base_type(ft) != base_type(rt))
                    throw std::runtime_error("semantic error: assignment type mismatch");
                return base_type(rt);
            }

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

        if (auto* call = dynamic_cast<const CallExpr*>(&e)) {
            if (!scope.has_func(call->callee))
                throw std::runtime_error("semantic error: unknown function: " + call->callee);

            const auto& overloads = scope.lookup_funcs(call->callee);

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
                        if (!can_bind_ref_to_expr(pt, *call->args[i], scope)) { ok = false; break; }
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
                    throw std::runtime_error("semantic error: ambiguous overload for function: " + call->callee);
                }
            }

            if (!best) throw std::runtime_error("semantic error: no matching overload for function: " + call->callee);
            return best->return_type;
        }

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

            if (!best) throw std::runtime_error("semantic error: no matching overload for constructor: " + ce->class_name);
            return ast::Type::Class(ce->class_name);
        }

        if (auto* ma = dynamic_cast<const MemberAccessExpr*>(&e)) {
            ast::Type objt = type_of_expr(scope, *ma->obj);
            if (base_type(objt).base != ast::Type::Base::Class)
                throw std::runtime_error("semantic error: member access on non-class object");

            if (!ct) throw std::runtime_error("semantic error: class table not set");
            return ct->field_type_in_chain(base_type(objt).class_name, ma->member);
        }

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

            if (!best) throw std::runtime_error("semantic error: no matching overload for method: " + mc->method);
            return best->return_type;
        }

        throw std::runtime_error("semantic error: unknown expression node");
    }

    void check_stmt(Scope& scope, const ast::Stmt& s, const ast::Type& expected_return) const {
        using namespace ast;

        if (auto* b = dynamic_cast<const BlockStmt*>(&s)) {
            Scope inner(&scope);
            for (const auto& st : b->stmts) check_stmt(inner, *st, expected_return);
            return;
        }

        if (auto* v = dynamic_cast<const VarDeclStmt*>(&s)) {
            if (scope.has_var_local(v->name))
                throw std::runtime_error("semantic error: duplicate variable in scope: " + v->name);

            if (v->type.is_ref && !v->init)
                throw std::runtime_error("semantic error: reference variable must be initialized: " + v->name);

            if (v->init) {
                if (v->type.is_ref) {
                    if (!can_bind_ref_to_expr(v->type, *v->init, scope))
                        throw std::runtime_error("semantic error: cannot bind reference to rvalue: " + v->name);
                } else {
                    ast::Type it = type_of_expr(scope, *v->init);
                    if (base_type(it) != base_type(v->type)) {
                        throw std::runtime_error("semantic error: initializer type mismatch for variable: " + v->name);
                    }
                }
            }

            scope.define_var(v->name, v->type);
            return;
        }

        if (auto* e = dynamic_cast<const ExprStmt*>(&s)) {
            (void)type_of_expr(scope, *e->expr);
            return;
        }

        if (auto* i = dynamic_cast<const IfStmt*>(&s)) {
            ast::Type ct2 = type_of_expr(scope, *i->cond);
            if (!is_bool_context_allowed(ct2))
                throw std::runtime_error("semantic error: if condition not convertible to bool: " + type_name(ct2));
            check_stmt(scope, *i->then_branch, expected_return);
            if (i->else_branch) check_stmt(scope, *i->else_branch, expected_return);
            return;
        }

        if (auto* w = dynamic_cast<const WhileStmt*>(&s)) {
            ast::Type ct2 = type_of_expr(scope, *w->cond);
            if (!is_bool_context_allowed(ct2))
                throw std::runtime_error("semantic error: while condition not convertible to bool: " + type_name(ct2));
            check_stmt(scope, *w->body, expected_return);
            return;
        }

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

    void check_function(const Scope& global, const ast::FunctionDef& f) const {
        Scope fun_scope(const_cast<Scope*>(&global));

        for (const auto& p : f.params) {
            if (fun_scope.has_var_local(p.name))
                throw std::runtime_error("semantic error: duplicate parameter name: " + p.name);
            fun_scope.define_var(p.name, p.type);
        }

        check_stmt(fun_scope, *f.body, f.return_type);
    }

    void check_method(const Scope& global,
                      const ClassTable& ctab,
                      const std::string& class_name,
                      const ast::MethodDef& m) const {
        Scope member_scope(const_cast<Scope*>(&global));

        const auto& fields = ctab.merged_fields_for(class_name);
        for (const auto& f : fields) member_scope.define_var(f.name, f.type);

        Scope method_scope(&member_scope);

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

    void check_constructor(const Scope& global,
                           const ClassTable& ctab,
                           const std::string& class_name,
                           const ast::ConstructorDef& ctor) const {
        Scope member_scope(const_cast<Scope*>(&global));

        const auto& fields = ctab.merged_fields_for(class_name);
        for (const auto& f : fields) member_scope.define_var(f.name, f.type);

        Scope ctor_scope(&member_scope);

        for (const auto& p : ctor.params) {
            if (ctab.has_field_in_chain(class_name, p.name)) {
                throw std::runtime_error("semantic error: parameter shadows field in constructor: " + p.name);
            }
            if (ctor_scope.has_var_local(p.name))
                throw std::runtime_error("semantic error: duplicate parameter name: " + p.name);
            ctor_scope.define_var(p.name, p.type);
        }

        check_stmt(ctor_scope, *ctor.body, ast::Type::Void());
    }
};

} // namespace sem
