#pragma once
// Verhindert mehrfaches Einbinden dieser Header-Datei

#include <string>           // std::string
#include <variant>          // std::variant
#include <unordered_map>    // std::unordered_map
#include <memory>           // std::shared_ptr

#include "../ast/type.hpp"  // ast::Type (fuer Slicing)

namespace interp {

struct Object;                              // Forward-Deklaration fuer Objekt-Typ
using ObjectPtr = std::shared_ptr<Object>;  // Gemeinsamer Pointer-Typ fuer Objekte

// Laufzeitwert der Sprache:
// Kann primitive Typen oder ein Objekt sein
// ObjectPtr wird verwendet, um rekursive Typdefinitionen zu vermeiden
using Value = std::variant<bool, int, char, std::string, ObjectPtr>;

// Laufzeit-Repräsentation eines Objekts
struct Object {
    std::string dynamic_class;                    // Dynamischer (runtime) Klassenname
    std::unordered_map<std::string, Value> fields; // Feldspeicher: Feldname -> Wert

    // Entfernt alle Felder, die nicht in allowed vorkommen (Object-Slicing)
    void slice_to(const std::string& static_class,
                  const std::unordered_map<std::string, ast::Type>& allowed) {
        (void)static_class;
        for (auto it = fields.begin(); it != fields.end(); ) {
            if (allowed.find(it->first) == allowed.end()) it = fields.erase(it);
            else ++it;
        }
    }
};

// Debug-/Ausgabe-Hilfsfunktion fuer Laufzeitwerte
inline std::string to_string(const Value& v) {
    struct V {
        std::string operator()(bool b) const {
            return b ? "true" : "false";
        }
        std::string operator()(int i) const {
            return std::to_string(i);
        }
        std::string operator()(char c) const {
            return std::string("'") + c + "'";
        }
        std::string operator()(const std::string& s) const {
            return "\"" + s + "\"";
        }
        std::string operator()(const ObjectPtr& o) const {
            if (!o) return "<null-object>";
            return "<obj:" + o->dynamic_class + ">";
        }
    };
    return std::visit(V{}, v);
}

} // namespace interp
