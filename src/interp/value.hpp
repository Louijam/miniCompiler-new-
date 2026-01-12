#pragma once
#include <string>
#include <variant>

namespace interp {

// Object kommt später für Klassen
using Value = std::variant<bool, int, char, std::string>;

inline std::string to_string(const Value& v) {
    struct V {
        std::string operator()(bool b) const { return b ? "true" : "false"; }
        std::string operator()(int i) const { return std::to_string(i); }
        std::string operator()(char c) const { return std::string("'") + c + "'"; }
        std::string operator()(const std::string& s) const { return "\"" + s + "\""; }
    };
    return std::visit(V{}, v);
}

} // namespace interp
