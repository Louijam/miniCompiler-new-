#pragma once
#include <string>
#include <variant>
#include <memory>
#include <unordered_map>

namespace interp {

struct Object {
    std::string class_name; // dynamischer Typ
    std::unordered_map<std::string, std::variant<bool,int,char,std::string,std::shared_ptr<Object>>> fields;
};

using ObjectPtr = std::shared_ptr<Object>;

// Value kann jetzt auch Objekte halten
using Value = std::variant<bool, int, char, std::string, ObjectPtr>;

inline std::string to_string(const Value& v) {
    struct V {
        std::string operator()(bool b) const { return b ? "true" : "false"; }
        std::string operator()(int i) const { return std::to_string(i); }
        std::string operator()(char c) const { return std::string("'") + c + "'"; }
        std::string operator()(const std::string& s) const { return "\"" + s + "\""; }
        std::string operator()(const ObjectPtr& o) const { return std::string("<obj:") + o->class_name + ">"; }
    };
    return std::visit(V{}, v);
}

} // namespace interp
