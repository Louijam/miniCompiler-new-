#pragma once
#include <string>
#include <unordered_map>
#include <variant>
#include <stdexcept>

#include "../ast/type.hpp"
#include "value.hpp"
#include "lvalue.hpp"

namespace interp {

struct Binding { LValue target; };

struct ValueSlot {
    Value value;
    ast::Type type;
};

struct RefSlot {
    Binding bind;
    ast::Type type; // must be ref type
};

using Slot = std::variant<ValueSlot, RefSlot>;

struct Env {
    Env* parent = nullptr;
    std::unordered_map<std::string, Slot> slots;

    explicit Env(Env* p=nullptr): parent(p) {}

    bool contains_local(const std::string& name) const { return slots.find(name) != slots.end(); }

    Slot* find_slot(const std::string& name) {
        auto it = slots.find(name);
        if (it != slots.end()) return &it->second;
        if (parent) return parent->find_slot(name);
        return nullptr;
    }

    Env* find_def_env(const std::string& name) {
        auto it = slots.find(name);
        if (it != slots.end()) return this;
        if (parent) return parent->find_def_env(name);
        return nullptr;
    }

    ast::Type lookup_type(const std::string& name) {
        Slot* s = find_slot(name);
        if (!s) throw std::runtime_error("undefined variable: " + name);
        if (auto* vs = std::get_if<ValueSlot>(s)) return vs->type;
        return std::get<RefSlot>(*s).type;
    }

    LValue resolve_lvalue(const std::string& name) {
        Env* def = find_def_env(name);
        if (!def) throw std::runtime_error("undefined variable: " + name);

        Slot& s = def->slots.at(name);
        if (std::holds_alternative<ValueSlot>(s)) return LValue::var(*def, name);
        return std::get<RefSlot>(s).bind.target;
    }

    void define_value_typed(const std::string& name, ast::Type t, Value v) {
        if (contains_local(name)) throw std::runtime_error("duplicate definition: " + name);
        slots.emplace(name, Slot{ValueSlot{std::move(v), std::move(t)}});
    }

    void define_ref_typed(const std::string& name, ast::Type t_ref, LValue target) {
        if (contains_local(name)) throw std::runtime_error("duplicate definition: " + name);
        if (!t_ref.is_ref) throw std::runtime_error("internal error: define_ref_typed needs ref type");
        slots.emplace(name, Slot{RefSlot{Binding{std::move(target)}, std::move(t_ref)}});
    }

    Value read_value(const std::string& name) {
        Slot* s = find_slot(name);
        if (!s) throw std::runtime_error("undefined variable: " + name);

        if (auto* vs = std::get_if<ValueSlot>(s)) return vs->value;
        return read_lvalue(std::get<RefSlot>(*s).bind.target);
    }

    void assign_value(const std::string& name, Value v) {
        Slot* s = find_slot(name);
        if (!s) throw std::runtime_error("undefined variable: " + name);

        if (auto* vs = std::get_if<ValueSlot>(s)) { vs->value = std::move(v); return; }
        write_lvalue(std::get<RefSlot>(*s).bind.target, std::move(v));
    }

    void write_lvalue(const LValue& lv, Value v) {
        if (lv.kind != LValue::Kind::Var) throw std::runtime_error("unsupported lvalue kind");
        if (!lv.env) throw std::runtime_error("null lvalue env");

        Slot* s = lv.env->find_slot(lv.name);
        if (!s) throw std::runtime_error("dangling lvalue: " + lv.name);

        auto* vs = std::get_if<ValueSlot>(s);
        if (!vs) throw std::runtime_error("cannot write to non-value slot: " + lv.name);
        vs->value = std::move(v);
    }

    Value read_lvalue(const LValue& lv) {
        if (lv.kind != LValue::Kind::Var) throw std::runtime_error("unsupported lvalue kind");
        if (!lv.env) throw std::runtime_error("null lvalue env");

        Slot* s = lv.env->find_slot(lv.name);
        if (!s) throw std::runtime_error("dangling lvalue: " + lv.name);

        auto* vs = std::get_if<ValueSlot>(s);
        if (!vs) throw std::runtime_error("cannot read from non-value slot: " + lv.name);
        return vs->value;
    }
};

} // namespace interp
