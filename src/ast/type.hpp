#pragma once
// Verhindert mehrfaches Einbinden dieser Header-Datei

#include <string>   // std::string

namespace ast {

// Repräsentiert einen Typ im Sprach-Typsystem
struct Type {

    // Grundtypen der Sprache
    enum class Base {
        Bool,       // bool
        Int,        // int
        Char,       // char
        String,     // string
        Void,       // void
        Class       // Klassentyp
    };

    Base base = Base::Int;        // Basistyp (Default: int)
    bool is_ref = false;          // true => Referenztyp (T&)
    std::string class_name;       // Name der Klasse (nur bei Base::Class relevant)

    // Fabrikfunktionen fuer primitive Typen
    static Type Bool(bool ref=false)   { return Type{Base::Bool, ref, ""}; }
    static Type Int(bool ref=false)    { return Type{Base::Int, ref, ""}; }
    static Type Char(bool ref=false)   { return Type{Base::Char, ref, ""}; }
    static Type String(bool ref=false) { return Type{Base::String, ref, ""}; }
    static Type Void()                 { return Type{Base::Void, false, ""}; }

    // Fabrikfunktion fuer Klassentypen
    static Type Class(std::string name, bool ref=false) {
        Type t;
        t.base = Base::Class;          // Markiert als Klassentyp
        t.is_ref = ref;                // Optional Referenz
        t.class_name = std::move(name);// Klassenname
        return t;
    }

    // Typvergleich (inkl. Referenz und Klassenname)
    bool operator==(const Type& other) const {
        return base == other.base &&
               is_ref == other.is_ref &&
               class_name == other.class_name;
    }

    // Ungleichheitsoperator
    bool operator!=(const Type& other) const {
        return !(*this == other);
    }
};

// Wandelt einen Typ in seine String-Repräsentation um
inline std::string to_string(const Type& t) {
    std::string s;
    switch (t.base) {
        case Type::Base::Bool:   s = "bool"; break;
        case Type::Base::Int:    s = "int"; break;
        case Type::Base::Char:   s = "char"; break;
        case Type::Base::String: s = "string"; break;
        case Type::Base::Void:   s = "void"; break;
        case Type::Base::Class:  s = t.class_name; break;
    }
    if (t.is_ref)
        s += "&";                // Referenztyp kennzeichnen
    return s;
}

// Hilfsfunktion: entfernt Referenz-Flag (z.B. fuer Assignments oder Typregeln)
inline Type strip_ref(Type t) {
    t.is_ref = false;
    return t;
}

} // namespace ast
