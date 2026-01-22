#pragma once
// Verhindert mehrfaches Einbinden dieser Header-Datei

#include <stdexcept>   // std::runtime_error
#include <string>      // std::string

#include "env.hpp"         // Laufzeit-Umgebung (Variablen, Typinfos)
#include "functions.hpp"   // Funktionstabelle + Klassen-Runtime-Infos
#include "object.hpp"      // Objekt-Repräsentation (Object, Felder, dynamic_class)
#include "value.hpp"       // Laufzeitwerte (Value, z.B. int, bool, ObjectPtr)
#include "../ast/type.hpp" // AST-Typdefinitionen

namespace interp {

// Tiefe Kopie fuer Values (Klassenwerte haben Wertsemantik, keine Pointer-Aliasing)
inline Value deep_copy_value(const Value& v) {
    if (auto* o = std::get_if<ObjectPtr>(&v)) {
        if (!*o) throw std::runtime_error("null object value");

        ObjectPtr dst = std::make_shared<Object>();
        dst->dynamic_class = (*o)->dynamic_class;
        for (const auto& [k, vv] : (*o)->fields) {
            dst->fields[k] = deep_copy_value(vv);
        }
        return Value{dst};
    }
    return v; // primitives: copy by value
}

// Weist einem benannten Wert (keinem Feld!) einen neuen Wert zu
// Berücksichtigt korrektes Object-Slicing bei Klassenwerten:
//
// - Wenn LHS ein Klassenwert (kein Ref) ist
// - und RHS ein Objekt ist
// - und sich statischer Typ (LHS) und dynamischer Typ (RHS) unterscheiden,
//   dann:
//     * werden nur die Felder der statischen LHS-Klasse übernommen (Slicing)
//     * dynamic_class wird auf die statische LHS-Klasse gesetzt
//
// In allen anderen Fällen erfolgt eine normale Zuweisung.
inline void assign_value_slicing_aware(Env& env,
                                      const std::string& name,
                                      const Value& rhs,
                                      FunctionTable& functions) {
    // Statischer Typ der linken Seite (Variable)
    ast::Type lhs_t = env.static_type_of(name);

    // Nur relevant bei Klassenwerten, die KEINE Referenzen sind
    if (lhs_t.base == ast::Type::Base::Class && !lhs_t.is_ref) {

        // RHS muss ein Objekt sein
        if (auto* rhs_obj = std::get_if<ObjectPtr>(&rhs)) {
            if (!*rhs_obj)
                throw std::runtime_error("assignment from null object");

            // Aktuellen Wert der LHS-Variable lesen
            Value cur = env.read_value(name);
            auto* lhs_obj = std::get_if<ObjectPtr>(&cur);
            if (!lhs_obj || !*lhs_obj)
                throw std::runtime_error("assignment to non-object");

            // Statischer Typ der LHS-Variable
            const std::string lhs_static = lhs_t.class_name;
            // Dynamischer Typ des RHS-Objekts
            const std::string rhs_dynamic = (*rhs_obj)->dynamic_class;

            // Gleicher Typ: komplette Kopie (keine Slicing-Regel notwendig)
            if (lhs_static == rhs_dynamic) {
                Value copied = deep_copy_value(rhs);
                auto* rhs_copy = std::get_if<ObjectPtr>(&copied);
                (*lhs_obj)->dynamic_class = rhs_dynamic;
                (*lhs_obj)->fields = (*rhs_copy)->fields;
                return;
            }

            // Unterschiedliche Typen: Object-Slicing
            // Nur Felder der statischen LHS-Klasse behalten
            const auto& lhs_ci = functions.class_rt.get(lhs_static);

            // Erst alles kopieren (deep copy) ...
            Value copied = deep_copy_value(rhs);
            auto* rhs_copy = std::get_if<ObjectPtr>(&copied);
            (*lhs_obj)->fields = (*rhs_copy)->fields;
            // ... dann auf statische LHS-Klasse zuschneiden
            (*lhs_obj)->slice_to(lhs_static, lhs_ci.merged_fields);
            // Dynamischen Typ auf statischen Typ setzen
            (*lhs_obj)->dynamic_class = lhs_static;
            return;
        }
    }

    // Fallback: normale Zuweisung (keine Klassenwerte / Referenzen / Primitivtypen)
    env.assign_value(name, rhs);
}

} // namespace interp
