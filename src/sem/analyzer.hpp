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

    static bool is_lvalue(const ast::Expr& e) {
        return dynamic_cast<const ast::VarExpr*>(&e) != nullptr
            || dynamic_cast<const ast::MemberAccessExpr*>(&e) != nullptr;
    }

    static ast::Type base_type(ast::Type t) {
        t.is_ref = false;
        return t;
    }

    static std::string type_name(const ast::Type& t) {
        return ast::to_string(t);
    }

    static bool is_bool_context_allowed(const ast::Type& t) {
        auto b = base_type(t);
        return b == ast::Type::Bool() || b == ast::Type::Int() || b == ast::Type::Char() || b == ast::Type::String();
    }

    std::string class_name_of(const ast::Type& t) const {
        auto b = base_type(t);
        if (b.base != ast::Type::Base::Class) {
            throw std::runtime_error("semantic error: expected class type, got " + type_name(t));
        }
        return b.class_name;
    }

    bool class_compatible_for_copy(const ast::Type& dst, const ast::Type& src) const {
        if (base_type(dst).base != ast::Type::Base::Class) return false;
        if (base_type(src).base != ast::Type::Base::Class) return false;
        if (!ct) throw std::runtime_error("semantic error: internal: class table not set");
        std::string d = base_type(dst).class_name;
        std::string s = base_type(src).class_name;
        return ct->is_same_or_derived(s, d); // src derived -> dst base allowed (slicing)
    }

    bool ref_compatible_for_bind(const ast::Type& dst_ref, const ast::Type& src, const ast::Expr& init_expr) const {
        if (!dst_ref.is_ref) return false;
        if (!is_lvalue(init_expr)) return false;

        ast::Type D = base_type(dst_ref);
        ast::Type S = base_type(src);

        if (D == S) return true;

        if (D.base == ast::Type::Base::Class && S.base == ast::Type::Base::Class) {
            if (!ct) throw std::runtime_error("semantic error: internal: class table not set");
            return ct->is_same_or_derived(S.class_name, D.class_name);
        }

        return false;
    }

    // ---------- overload resolution (functions) ----------
    const FuncSymbol& resolve_call(const Scope& scope, const ast::CallExpr& call) const {
        const Scope* s = &scope;
        const std::vector<FuncSymbol>* overloads = nullptr;
        while (s) {
            auto it = s->funcs.find(call.callee);
            if (it != s->funcs.end()) { overloads = &it->second; break; }
            s = s->parent;
        }
        if (!overloads) throw std::runtime_error("semantic error: unknown function: " + call.callee);

        const FuncSymbol* best = nullptr;
        int best_score = -1;

        for (const auto& cand : *overloads) {
            if (cand.param_types.size() != call.args.size()) continue;

            bool ok = true;
            int score = 0;

            for (size_t i = 0; i < call.args.size(); ++i) {
                ast::Type arg_t = base_type(type_of_expr(scope, *call.args[i]));
                ast::Type par_t = cand.param_types[i];

                if (base_type(par_t) != arg_t) { ok = false; break; }

                if (par_t.is_ref) {
                    if (!is_lvalue(*call.args[i])) { ok = false; break; }
                    score += 1;
                }
            }

            if (!ok) continue;

            if (score > best_score) {
                best_score = score;
                best = &cand;
            } else if (score == best_score) {
                throw std::runtime_error("semantic error: ambiguous overload: " + call.callee);
            }
        }

        if (!best) throw std::runtime_error("semantic error: no matching overload: " + call.callee);
        return *best;
    }

    // ---------- expressions ----------
    ast::Type type_of_expr(const Scope& scope, const ast::Expr& e) const {
        using namespace ast;

        if (dynamic_cast<const IntLiteral*>(&e)) return Type::Int();
        if (dynamic_cast<const BoolLiteral*>(&e)) return Type::Bool();
        if (dynamic_cast<const CharLiteral*>(&e)) return Type::Char();
        if (dynamic_cast<const StringLiteral*>(&e)) return Type::String();

        if (auto* v = dynamic_cast<const VarExpr*>(&e)) {
            return scope.lookup_var(v->name).type;
        }

        if (auto* a = dynamic_cast<const AssignExpr*>(&e)) {
            const auto& lhs = scope.lookup_var(a->name);
            ast::Type rhs_t = type_of_expr(scope, *a->value);

            if (base_type(lhs.type) != base_type(rhs_t)) {
                if (!class_compatible_for_copy(lhs.type, rhs_t)) {
                    throw std::runtime_error("semantic error: assignment type mismatch: " +
                        lhs.name + " is " + type_name(lhs.type) + ", rhs is " + type_name(rhs_t));
                }
            }
            return rhs_t;
        }

        if (auto* fa = dynamic_cast<const FieldAssignExpr*>(&e)) {
            if (!ct) throw std::runtime_error("semantic error: internal: class table not set");

            // in unserem Subset: obj muss lvalue sein (kein Feldschreiben auf temporaeren Objekten)
            if (!is_lvalue(*fa->object)) {
                throw std::runtime_error("semantic error: field assignment requires lvalue object");
            }

            ast::Type obj_t = type_of_expr(scope, *fa->object);
            std::string cn = class_name_of(obj_t);

            if (!ct->has_field_in_chain(cn, fa->field)) {
                throw std::runtime_error("semantic error: unknown field: " + cn + "." + fa->field);
            }

            ast::Type field_t = ct->field_type_in_chain(cn, fa->field);
            ast::Type rhs_t = type_of_expr(scope, *fa->value);

            if (base_type(field_t) != base_type(rhs_t)) {
                if (!class_compatible_for_copy(field_t, rhs_t)) {
                    throw std::runtime_error("semantic error: field assignment type mismatch: " +
                        cn + "." + fa->field + " is " + type_name(field_t) + ", rhs is " + type_name(rhs_t));
                }
            }

            return rhs_t; // assignment expression result type = rhs
        }

        if (auto* ce = dynamic_cast<const ConstructExpr*>(&e)) {
            if (!ct) throw std::runtime_error("semantic error: internal: class table not set");
            if (!ct->has_class(ce->class_name)) {
                throw std::runtime_error("semantic error: unknown class: " + ce->class_name);
            }

            std::vector<ast::Type> arg_base;
            std::vector<bool> arg_lv;
            arg_base.reserve(ce->args.size());
            arg_lv.reserve(ce->args.size());

            for (const auto& a2 : ce->args) {
                arg_base.push_back(base_type(type_of_expr(scope, *a2)));
                arg_lv.push_back(is_lvalue(*a2));
            }

            (void)ct->resolve_ctor_call(ce->class_name, arg_base, arg_lv);
            return Type::Class(ce->class_name);
        }

        if (auto* m = dynamic_cast<const MemberAccessExpr*>(&e)) {
            if (!ct) throw std::runtime_error("semantic error: internal: class table not set");
            ast::Type obj_t = type_of_expr(scope, *m->object);
            std::string cn = class_name_of(obj_t);

            if (!ct->has_field_in_chain(cn, m->field)) {
                throw std::runtime_error("semantic error: unknown field: " + cn + "." + m->field);
            }
            return ct->field_type_in_chain(cn, m->field);
        }

        if (auto* mc = dynamic_cast<const MethodCallExpr*>(&e)) {
            if (!ct) throw std::runtime_error("semantic error: internal: class table not set");
            ast::Type obj_t = type_of_expr(scope, *mc->object);
            std::string cn = class_name_of(obj_t);

            std::vector<ast::Type> arg_base;
            std::vector<bool> arg_lv;
            arg_base.reserve(mc->args.size());
            arg_lv.reserve(mc->args.size());

            for (const auto& a2 : mc->args) {
                arg_base.push_back(base_type(type_of_expr(scope, *a2)));
                arg_lv.push_back(is_lvalue(*a2));
            }

            const auto& chosen = ct->resolve_method_call(cn, mc->method, arg_base, arg_lv);
            return chosen.return_type;
        }

        if (auto* u = dynamic_cast<const UnaryExpr*>(&e)) {
            ast::Type t = type_of_expr(scope, *u->expr);

            if (u->op == UnaryExpr::Op::Neg) {
                if (base_type(t) != Type::Int()) throw std::runtime_error("semantic error: unary - expects int");
                return Type::Int();
            }
            if (u->op == UnaryExpr::Op::Not) {
                if (base_type(t) != Type::Bool()) throw std::runtime_error("semantic error: ! expects bool");
                return Type::Bool();
            }
            throw std::runtime_error("semantic error: unknown unary op");
        }

        if (auto* bin = dynamic_cast<const BinaryExpr*>(&e)) {
            ast::Type lt = type_of_expr(scope, *bin->left);
            ast::Type rt = type_of_expr(scope, *bin->right);

            auto L = base_type(lt);
            auto R = base_type(rt);

            if (bin->op == BinaryExpr::Op::Add || bin->op == BinaryExpr::Op::Sub ||
                bin->op == BinaryExpr::Op::Mul || bin->op == BinaryExpr::Op::Div ||
                bin->op == BinaryExpr::Op::Mod) {
                if (L != Type::Int() || R != Type::Int())
                    throw std::runtime_error("semantic error: arithmetic expects int,int");
                return Type::Int();
            }

            if (bin->op == BinaryExpr::Op::AndAnd || bin->op == BinaryExpr::Op::OrOr) {
                if (L != Type::Bool() || R != Type::Bool())
                    throw std::runtime_error("semantic error: &&/|| expects bool,bool");
                return Type::Bool();
            }

            if (bin->op == BinaryExpr::Op::Eq || bin->op == BinaryExpr::Op::Ne) {
                if (L != R) throw std::runtime_error("semantic error: ==/!= require same type");
                if (L != Type::Int() && L != Type::Char() && L != Type::Bool() && L != Type::String())
                    throw std::runtime_error("semantic error: ==/!= unsupported type");
                return Type::Bool();
            }

            if (bin->op == BinaryExpr::Op::Lt || bin->op == BinaryExpr::Op::Le ||
                bin->op == BinaryExpr::Op::Gt || bin->op == BinaryExpr::Op::Ge) {
                if (L != R) throw std::runtime_error("semantic error: relational ops require same type");
                if (L != Type::Int() && L != Type::Char())
                    throw std::runtime_error("semantic error: relational ops require int or char");
                return Type::Bool();
            }

            throw std::runtime_error("semantic error: unknown binary op");
        }

        if (auto* call = dynamic_cast<const ast::CallExpr*>(&e)) {
            const FuncSymbol& f = resolve_call(scope, *call);
            return f.return_type;
        }

        throw std::runtime_error("semantic error: unknown expression node");
    }

    // ---------- statements ----------
    void check_stmt(Scope& scope, const ast::Stmt& s, const ast::Type& expected_return) const {
        using namespace ast;

        if (auto* b = dynamic_cast<const BlockStmt*>(&s)) {
            Scope inner(&scope);
            for (const auto& st : b->statements) check_stmt(inner, *st, expected_return);
            return;
        }

        if (auto* v = dynamic_cast<const VarDeclStmt*>(&s)) {
            if (scope.has_var_local(v->name))
                throw std::runtime_error("semantic error: variable redefinition in same scope: " + v->name);

            ast::Type declared = v->decl_type;

            if (declared.is_ref) {
                if (!v->init)
                    throw std::runtime_error("semantic error: reference variable requires initializer: " + v->name);
                ast::Type init_t = type_of_expr(scope, *v->init);

                if (!ref_compatible_for_bind(declared, init_t, *v->init)) {
                    throw std::runtime_error("semantic error: init type mismatch for " + v->name +
                        ": declared " + type_name(declared) + ", init " + type_name(init_t));
                }

                scope.define_var(v->name, declared);
                return;
            }

            if (v->init) {
                ast::Type init_t = type_of_expr(scope, *v->init);

                if (base_type(declared) != base_type(init_t)) {
                    if (!class_compatible_for_copy(declared, init_t)) {
                        throw std::runtime_error("semantic error: init type mismatch for " + v->name +
                            ": declared " + type_name(declared) + ", init " + type_name(init_t));
                    }
                }
            }

            scope.define_var(v->name, declared);
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
        auto merged = ctab.merged_fields_derived_wins(class_name);
        for (const auto& kv : merged) member_scope.define_var(kv.first, kv.second);

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
};

} // namespace sem
