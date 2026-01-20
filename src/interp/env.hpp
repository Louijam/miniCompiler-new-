#pragma once
#include <string>
#include <unordered_map>
#include <variant>
#include <stdexcept>

#include "value.hpp"
#include "lvalue.hpp"

namespace interp {

struct Binding { LValue target; };
using Slot = std::variant<Value, Binding>;

struct Env {
    Env* parent = nullptr;
    std::unordered_map<std::string, Slot> slots;

    explicit Env(Env* p=nullptr):parent(p){}

    bool contains_local(const std::string& n) const { return slots.count(n); }

    Slot* find_slot(const std::string& n){
        if (slots.count(n)) return &slots[n];
        if (parent) return parent->find_slot(n);
        return nullptr;
    }

    Env* find_def_env(const std::string& n){
        if (slots.count(n)) return this;
        if (parent) return parent->find_def_env(n);
        return nullptr;
    }

    LValue resolve_lvalue(const std::string& n){
        Env* e = find_def_env(n);
        if (!e) throw std::runtime_error("undefined variable: " + n);
        Slot& s = e->slots[n];
        if (auto* v = std::get_if<Value>(&s)) return LValue::var(*e,n);
        return std::get<Binding>(s).target;
    }

    void define_value(const std::string& n, Value v){
        if (contains_local(n)) throw std::runtime_error("duplicate definition: "+n);
        slots.emplace(n, Slot{v});
    }

    void define_ref(const std::string& n, LValue lv){
        if (contains_local(n)) throw std::runtime_error("duplicate definition: "+n);
        slots.emplace(n, Slot{Binding{lv}});
    }

    Value read_value(const std::string& n){
        Slot* s = find_slot(n);
        if (!s) throw std::runtime_error("undefined variable: "+n);
        if (auto* v = std::get_if<Value>(s)) return *v;
        return read_lvalue(std::get<Binding>(*s).target);
    }

    void assign_value(const std::string& n, Value v){
        Slot* s = find_slot(n);
        if (!s) throw std::runtime_error("undefined variable: "+n);
        if (auto* pv = std::get_if<Value>(s)) { *pv = v; return; }
        write_lvalue(std::get<Binding>(*s).target, v);
    }

    void write_lvalue(const LValue& lv, Value v){
        Slot* s = lv.env->find_slot(lv.name);
        auto* pv = std::get_if<Value>(s);
        *pv = v;
    }

    Value read_lvalue(const LValue& lv){
        Slot* s = lv.env->find_slot(lv.name);
        auto* pv = std::get_if<Value>(s);
        return *pv;
    }
};

} // namespace interp
