#pragma once
#include <string>
#include <variant>
#include <unordered_map>
#include <memory>

namespace interp {

struct Object;
using ObjectPtr = std::shared_ptr<Object>;

// Object comes later for classes; we use ObjectPtr to avoid recursive variant issues.
using Value = std::variant<bool, int, char, std::string, ObjectPtr>;

struct Object {
    // runtime (dynamic) class name of this object value
    std::string dynamic_class;

    // field storage: field name -> value
    std::unordered_map<std::string, Value> fields;
};

inline std::string to_string(const Value& v) {
    struct V {
        std::string operator()(bool b) const { return b ? "true" : "false"; }
        std::string operator()(int i) const { return std::to_string(i); }
        std::string operator()(char c) const { return std::string("'") + c + "'"; }
        std::string operator()(const std::string& s) const { return "\"" + s + "\""; }
        std::string operator()(const ObjectPtr& o) const {
            if (!o) return "<null-object>";
            return "<obj:" + o->dynamic_class + ">";
        }
    };
    return std::visit(V{}, v);
}

} // namespace interp
