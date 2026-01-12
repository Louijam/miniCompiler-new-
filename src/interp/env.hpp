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

    // define value variable
    void define_value(const std::string& name, Value v) {
        if (contains_local(name)) throw std::runtime_error("duplicate definition: " + name);
        slots.emplace(name, Slot{std::move(v)});
    }

    // define reference variable
    void define_ref(const std::string& name, LValue target) {
        if (contains_local(name)) throw std::runtime_error("duplicate definition: " + name);
        slots.emplace(name, Slot{Binding{std::move(target)}});
    }

    Value read_value(const std::string& name) {
        Slot* s = find_slot(name);
        if (!s) throw std::runtime_error("undefined variable: " + name);

        if (auto* pv = std::get_if<Value>(s)) return *pv; // copy
        auto* pb = std::get_if<Binding>(s);
        return read_lvalue(pb->target);
    }

    void assign_value(const std::string& name, Value v) {
        Slot* s = find_slot(name);
        if (!s) throw std::runtime_error("undefined variable: " + name);

        if (auto* pv = std::get_if<Value>(s)) {
            *pv = std::move(v);
            return;
        }
        auto* pb = std::get_if<Binding>(s);
        write_lvalue(pb->target, std::move(v));
    }

    // write directly to an lvalue
    void write_lvalue(const LValue& lv, Value v) {
        if (lv.kind != LValue::Kind::Var) throw std::runtime_error("unsupported lvalue kind");
        if (!lv.env) throw std::runtime_error("null lvalue env");

        Slot* s = lv.env->find_slot(lv.name);
        if (!s) throw std::runtime_error("dangling lvalue: " + lv.name);

        auto* pv = std::get_if<Value>(s);
        if (!pv) throw std::runtime_error("cannot write to non-value slot: " + lv.name);
        *pv = std::move(v);
    }

    Value read_lvalue(const LValue& lv) {
        if (lv.kind != LValue::Kind::Var) throw std::runtime_error("unsupported lvalue kind");
        if (!lv.env) throw std::runtime_error("null lvalue env");

        Slot* s = lv.env->find_slot(lv.name);
        if (!s) throw std::runtime_error("dangling lvalue: " + lv.name);

        auto* pv = std::get_if<Value>(s);
        if (!pv) throw std::runtime_error("cannot read from non-value slot: " + lv.name);
        return *pv;
    }
};

} // namespace interp
