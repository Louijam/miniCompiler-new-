#pragma once

#include <string>
#include <memory>

#include "value.hpp"

namespace interp {

struct Env;

struct LValue {
    enum class Kind { Var, Field };

    Kind kind = Kind::Var;

    // Var
    Env* env = nullptr;
    std::string name;

    // Field
    ObjectPtr obj;
    std::string field;

    static LValue var(Env& e, std::string n) {
        LValue lv;
        lv.kind = Kind::Var;
        lv.env = &e;
        lv.name = std::move(n);
        return lv;
    }

    static LValue field_of(ObjectPtr o, std::string f) {
        LValue lv;
        lv.kind = Kind::Field;
        lv.obj = std::move(o);
        lv.field = std::move(f);
        return lv;
    }
};

} // namespace interp
