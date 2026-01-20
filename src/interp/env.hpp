#pragma once
#include <string>
#include <unordered_map>
#include <variant>
#include <stdexcept>

#include "value.hpp"
#include "lvalue.hpp"

namespace interp {

struct Binding {
    LValue target;
};

using Slot = std::variant<Value, Binding>;

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

    LValue resolve_lvalue(const std::string& name) {
        Env* def = find_def_env(name);
        if (!def) throw std::runtime_error("undefined variable: " + name);

        Slot& s = def->slots.at(name);

        if (std::holds_alternative<Value>(s)) {
            return LValue::var(*def, name);
        }
        return std::get<Binding>(s).target;
    }

    void define_value(const std::string& name, Value v) {
        if (contains_local(name)) throw std::runtime_error("duplicate definition: " + name);
        slots.emplace(name, Slot{std::move(v)});
    }

    void define_ref(const std::string& name, LValue target) {
        if (contains_local(name)) throw std::runtime_error("duplicate definition: " + name);
        slots.emplace(name, Slot{Binding{std::move(target)}});
    }

    Value read_value(const std::string& name) {
        Slot* s = find_slot(name);
        if (!s) throw std::runtime_error("undefined variable: " + name);

        if (auto* pv = std::get_if<Value>(s)) return *pv;
        return read_lvalue(std::get<Binding>(*s).target);
    }

    void assign_value(const std::string& name, Value v) {
        Slot* s = find_slot(name);
        if (!s) throw std::runtime_error("undefined variable: " + name);

        if (auto* pv = std::get_if<Value>(s)) {
            *pv = std::move(v);
            return;
        }
        write_lvalue(std::get<Binding>(*s).target, std::move(v));
    }

    void write_lvalue(const LValue& lv, Value v) {
        if (lv.kind == LValue::Kind::Var) {
            if (!lv.env) throw std::runtime_error("null lvalue env");
            Slot* s = lv.env->find_slot(lv.name);
            if (!s) throw std::runtime_error("dangling lvalue: " + lv.name);

            auto* pv = std::get_if<Value>(s);
            if (!pv) throw std::runtime_error("cannot write to non-value slot: " + lv.name);
            *pv = std::move(v);
            return;
        }

        if (lv.kind == LValue::Kind::Field) {
            if (!lv.obj) throw std::runtime_error("null object in field lvalue");
            auto it = lv.obj->fields.find(lv.field);
            if (it == lv.obj->fields.end()) throw std::runtime_error("unknown field: " + lv.field);

            // store as Value (same variant), so assign directly:
            it->second = std::move(v);
            return;
        }

        throw std::runtime_error("unsupported lvalue kind");
    }

    Value read_lvalue(const LValue& lv) {
        if (lv.kind == LValue::Kind::Var) {
            if (!lv.env) throw std::runtime_error("null lvalue env");
            Slot* s = lv.env->find_slot(lv.name);
            if (!s) throw std::runtime_error("dangling lvalue: " + lv.name);

            auto* pv = std::get_if<Value>(s);
            if (!pv) throw std::runtime_error("cannot read from non-value slot: " + lv.name);
            return *pv;
        }

        if (lv.kind == LValue::Kind::Field) {
            if (!lv.obj) throw std::runtime_error("null object in field lvalue");
            auto it = lv.obj->fields.find(lv.field);
            if (it == lv.obj->fields.end()) throw std::runtime_error("unknown field: " + lv.field);

            // field storage is a Value-compatible variant; return as Value
            const auto& raw = it->second;
            if (auto* pb = std::get_if<bool>(&raw)) return *pb;
            if (auto* pi = std::get_if<int>(&raw)) return *pi;
            if (auto* pc = std::get_if<char>(&raw)) return *pc;
            if (auto* ps = std::get_if<std::string>(&raw)) return *ps;
            if (auto* po = std::get_if<ObjectPtr>(&raw)) return *po;
            throw std::runtime_error("invalid field value");
        }

        throw std::runtime_error("unsupported lvalue kind");
    }
};

} // namespace interp
