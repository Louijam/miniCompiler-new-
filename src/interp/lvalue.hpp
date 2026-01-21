#pragma once
// Verhindert mehrfaches Einbinden dieser Header-Datei

#include <string>   // std::string
#include <memory>   // std::shared_ptr

#include "value.hpp" // Definition von ObjectPtr

namespace interp {

struct Env; // Forward-Deklaration, um zirkuläre Abhängigkeit zu vermeiden

// Repräsentiert ein LValue (linke Seite einer Zuweisung)
struct LValue {

    // Art des LValues: Variable oder Objektfeld
    enum class Kind { Var, Field };

    Kind kind = Kind::Var; // Standard: Variable

    // --- Variable ---
    Env* env = nullptr;    // Umgebung, in der die Variable definiert ist
    std::string name;      // Name der Variable

    // --- Objektfeld ---
    ObjectPtr obj;         // Objekt, dessen Feld adressiert wird
    std::string field;     // Name des Feldes

    // Erzeugt ein LValue fuer eine Variable
    static LValue var(Env& e, std::string n) {
        LValue lv;
        lv.kind = Kind::Var;
        lv.env = &e;
        lv.name = std::move(n);
        return lv;
    }

    // Erzeugt ein LValue fuer ein Objektfeld
    static LValue field_of(ObjectPtr o, std::string f) {
        LValue lv;
        lv.kind = Kind::Field;
        lv.obj = std::move(o);
        lv.field = std::move(f);
        return lv;
    }
};

} // namespace interp
