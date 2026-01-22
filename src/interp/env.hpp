#pragma once
// Verhindert mehrfaches Einbinden dieser Header-Datei

#include <string>           // std::string
#include <unordered_map>   // std::unordered_map
#include <variant>         // std::variant
#include <stdexcept>       // std::runtime_error

#include "value.hpp"       // Laufzeitwerte (Value)
#include "lvalue.hpp"      // LValue (Variable oder Feldzugriff)
#include "../ast/type.hpp" // Statischer Typ (ast::Type)

namespace interp {

// Slot fuer normale Variablen (Wert + statischer Typ)
struct VarSlot {
    Value value;           // Aktueller Laufzeitwert
    ast::Type static_type; // Statischer Typ der Variable
};

// Slot fuer Referenzvariablen (T&)
struct RefSlot {
    LValue target;         // Ziel-LValue, auf das die Referenz zeigt
    ast::Type static_type; // Statischer Referenztyp (T&)
};

// Ein Slot ist entweder ein normaler Wert oder eine Referenz
using Slot = std::variant<VarSlot, RefSlot>;

// Laufzeit-Umgebung (Scope / Stack-Frame)
struct Env {
    Env* parent = nullptr;                           // Übergeordnete Umgebung (Scope-Kette)
    std::unordered_map<std::string, Slot> slots;    // Lokale Variablen

    explicit Env(Env* p = nullptr) : parent(p) {}

    // Prüft, ob eine Variable lokal definiert ist
    bool contains_local(const std::string& name) const {
        return slots.find(name) != slots.end();
    }

    // Sucht einen Slot in der Scope-Kette
    Slot* find_slot(const std::string& name) {
        auto it = slots.find(name);
        if (it != slots.end()) return &it->second;
        if (parent) return parent->find_slot(name);
        return nullptr;
    }

    // Findet die Umgebung, in der eine Variable definiert wurde
    Env* find_def_env(const std::string& name) {
        auto it = slots.find(name);
        if (it != slots.end()) return this;
        if (parent) return parent->find_def_env(name);
        return nullptr;
    }

    // Prüft, ob eine Variable eine Referenz ist
    bool is_ref_var(const std::string& name) {
        Slot* s = find_slot(name);
        if (!s) throw std::runtime_error("undefined variable: " + name);
        return std::holds_alternative<RefSlot>(*s);
    }

    // Liefert den statischen Typ einer Variable
    ast::Type static_type_of(const std::string& name) {
        Slot* s = find_slot(name);
        if (!s) throw std::runtime_error("undefined variable: " + name);
        if (auto* pv = std::get_if<VarSlot>(s)) return pv->static_type;
        return std::get<RefSlot>(*s).static_type;
    }

    // Erzeugt ein LValue aus einem Variablennamen
    LValue resolve_lvalue(const std::string& name) {
        Env* def = find_def_env(name);
        if (!def) throw std::runtime_error("undefined variable: " + name);

        Slot& s = def->slots.at(name);

        if (std::holds_alternative<VarSlot>(s))
            return LValue::var(*def, name);

        return std::get<RefSlot>(s).target;
    }

    // Definiert eine neue normale Variable
    void define_value(const std::string& name, Value v, ast::Type static_type) {
        if (contains_local(name))
            throw std::runtime_error("duplicate definition: " + name);
        slots.emplace(name, Slot{VarSlot{std::move(v), static_type}});
    }

    // Definiert eine neue Referenzvariable
    void define_ref(const std::string& name, LValue target, ast::Type static_type) {
        if (contains_local(name))
            throw std::runtime_error("duplicate definition: " + name);
        slots.emplace(name, Slot{RefSlot{std::move(target), static_type}});
    }

    // Liest den Wert einer Variable (inkl. Dereferenzierung)
    Value read_value(const std::string& name) {
        Slot* s = find_slot(name);
        if (!s) throw std::runtime_error("undefined variable: " + name);

        if (auto* pv = std::get_if<VarSlot>(s))
            return pv->value;

        return read_lvalue(std::get<RefSlot>(*s).target);
    }

    // Weist einer Variable einen neuen Wert zu
    void assign_value(const std::string& name, Value v) {
        Slot* s = find_slot(name);
        if (!s) throw std::runtime_error("undefined variable: " + name);

        if (auto* pv = std::get_if<VarSlot>(s)) {
            pv->value = std::move(v);
            return;
        }

        write_lvalue(std::get<RefSlot>(*s).target, std::move(v));
    }

    // Schreibt in ein LValue (Variable oder Objektfeld)
    void write_lvalue(const LValue& lv, Value v) {
        if (lv.kind == LValue::Kind::Var) {
            if (!lv.env) throw std::runtime_error("null lvalue env");

            Slot* s = lv.env->find_slot(lv.name);
            if (!s) throw std::runtime_error("dangling lvalue: " + lv.name);

            auto* pv = std::get_if<VarSlot>(s);
            if (!pv)
                throw std::runtime_error("cannot write to non-value slot: " + lv.name);

            pv->value = std::move(v);
            return;
        }

        // Feld-LValue
        if (!lv.obj)
            throw std::runtime_error("null object for field lvalue");

        auto it = lv.obj->fields.find(lv.field);
        if (it == lv.obj->fields.end())
            throw std::runtime_error("unknown field at runtime: " + lv.field);

        it->second = std::move(v);
    }

    // Liest aus einem LValue (Variable oder Objektfeld)
    Value read_lvalue(const LValue& lv) {
        if (lv.kind == LValue::Kind::Var) {
            if (!lv.env) throw std::runtime_error("null lvalue env");

            Slot* s = lv.env->find_slot(lv.name);
            if (!s) throw std::runtime_error("dangling lvalue: " + lv.name);

            auto* pv = std::get_if<VarSlot>(s);
            if (!pv)
                throw std::runtime_error("cannot read from non-value slot: " + lv.name);

            return pv->value;
        }

        // Feld-LValue
        if (!lv.obj)
            throw std::runtime_error("null object for field lvalue");

        auto it = lv.obj->fields.find(lv.field);
        if (it == lv.obj->fields.end())
            throw std::runtime_error("unknown field at runtime: " + lv.field);

        return it->second;
    }
};

} // namespace interp
