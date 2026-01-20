#pragma once
#include <string>
#include <unordered_map>
#include <variant>
#include <stdexcept>

#include "value.hpp"
#include "lvalue.hpp"
#include "../ast/type.hpp"

namespace interp {

struct VarSlot {
    Value value;
    ast::Type static_type;
};

struct RefSlot {
    LValue target;
    ast::Type static_type; // ref type (T&)
};

using Slot = std::variant<VarSlot, RefSlot>;

struct Env {
    Env* parent = nullptr;
    std::unordered_map<std::string, Slot> slots;

    explicit Env(Env* p = nullptr) : parent(p) {}

    bool contains_local(const std::string& name) const {
        return slots.find(name) != slots.end();
    }

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

    bool is_ref_var(const std::string& name) {
        Slot* s = find_slot(name);
        if (!s) throw std::runtime_error("undefined variable: " + name);
        return std::holds_alternative<RefSlot>(*s);
    }

    ast::Type static_type_of(const std::string& name) {
        Slot* s = find_slot(name);
        if (!s) throw std::runtime_error("undefined variable: " + name);
        if (auto* pv = std::get_if<VarSlot>(s)) return pv->static_type;
        return std::get<RefSlot>(*s).static_type;
    }

    LValue resolve_lvalue(const std::string& name) {
        Env* def = find_def_env(name);
        if (!def) throw std::runtime_error("undefined variable: " + name);

        Slot& s = def->slots.at(name);

        if (std::holds_alternative<VarSlot>(s)) return LValue::var(*def, name);
        return std::get<RefSlot>(s).target;
    }

    void define_value(const std::string& name, Value v, ast::Type static_type) {
        if (contains_local(name)) throw std::runtime_error("duplicate definition: " + name);
        slots.emplace(name, Slot{VarSlot{std::move(v), static_type}});
    }

    void define_ref(const std::string& name, LValue target, ast::Type static_type) {
        if (contains_local(name)) throw std::runtime_error("duplicate definition: " + name);
        slots.emplace(name, Slot{RefSlot{std::move(target), static_type}});
    }

    Value read_value(const std::string& name) {
        Slot* s = find_slot(name);
        if (!s) throw std::runtime_error("undefined variable: " + name);

        if (auto* pv = std::get_if<VarSlot>(s)) return pv->value;
        return read_lvalue(std::get<RefSlot>(*s).target);
    }

    void assign_value(const std::string& name, Value v) {
        Slot* s = find_slot(name);
        if (!s) throw std::runtime_error("undefined variable: " + name);

        if (auto* pv = std::get_if<VarSlot>(s)) {
            pv->value = std::move(v);
            return;
        }
        write_lvalue(std::get<RefSlot>(*s).target, std::move(v));
    }

    void write_lvalue(const LValue& lv, Value v) {
        if (lv.kind == LValue::Kind::Var) {
            if (!lv.env) throw std::runtime_error("null lvalue env");

            Slot* s = lv.env->find_slot(lv.name);
            if (!s) throw std::runtime_error("dangling lvalue: " + lv.name);

            auto* pv = std::get_if<VarSlot>(s);
            if (!pv) throw std::runtime_error("cannot write to non-value slot: " + lv.name);
            pv->value = std::move(v);
            return;
        }

        if (!lv.obj) throw std::runtime_error("null object for field lvalue");
        lv.obj->fields[lv.field] = std::move(v);
    }

    Value read_lvalue(const LValue& lv) {
        if (lv.kind == LValue::Kind::Var) {
            if (!lv.env) throw std::runtime_error("null lvalue env");

            Slot* s = lv.env->find_slot(lv.name);
            if (!s) throw std::runtime_error("dangling lvalue: " + lv.name);

            auto* pv = std::get_if<VarSlot>(s);
            if (!pv) throw std::runtime_error("cannot read from non-value slot: " + lv.name);
            return pv->value;
        }

        if (!lv.obj) throw std::runtime_error("null object for field lvalue");
        auto it = lv.obj->fields.find(lv.field);
        if (it == lv.obj->fields.end()) throw std::runtime_error("unknown field at runtime: " + lv.field);
        return it->second;
    }
};

} // namespace interp
