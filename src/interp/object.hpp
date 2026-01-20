#pragma once
#include <string>
#include <unordered_map>
#include "../ast/type.hpp"
#include "value.hpp"

namespace interp {

struct Object {
    std::string class_name; // dynamischer Typ
    std::unordered_map<std::string, Value> fields;
};

} // namespace interp
