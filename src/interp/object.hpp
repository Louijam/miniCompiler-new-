#pragma once
#include <string>
#include <unordered_map>
#include <variant>
#include <memory>

namespace interp {

struct Object;
using ObjectPtr = std::shared_ptr<Object>;

struct Object {
    std::string class_name; // dynamischer Typ
    std::unordered_map<std::string, std::variant<bool,int,char,std::string,ObjectPtr>> fields;
};

} // namespace interp
