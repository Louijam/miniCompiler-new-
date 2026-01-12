#pragma once
#include <string>

namespace ast {

struct Type {
    enum class Base {
        Bool,
        Int,
        Char,
        String,
        Void,
        Class
    };

    Base base = Base::Int;
    bool is_ref = false;          // true -> T&
    std::string class_name;       // only used if base == Class

    static Type Bool(bool ref=false)   { return Type{Base::Bool, ref, ""}; }
    static Type Int(bool ref=false)    { return Type{Base::Int, ref, ""}; }
    static Type Char(bool ref=false)   { return Type{Base::Char, ref, ""}; }
    static Type String(bool ref=false) { return Type{Base::String, ref, ""}; }
    static Type Void()                { return Type{Base::Void, false, ""}; }

    static Type Class(std::string name, bool ref=false) {
        Type t;
        t.base = Base::Class;
        t.is_ref = ref;
        t.class_name = std::move(name);
        return t;
    }

    bool operator==(const Type& other) const {
        return base == other.base && is_ref == other.is_ref && class_name == other.class_name;
    }
    bool operator!=(const Type& other) const { return !(*this == other); }
};

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
    if (t.is_ref) s += "&";
    return s;
}

// helper: get non-ref version (for rules like "no ref to ref", assign, etc.)
inline Type strip_ref(Type t) {
    t.is_ref = false;
    return t;
}

} // namespace ast

