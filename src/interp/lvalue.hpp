#pragma once
#include <string>

namespace interp {

struct Env; // forward

struct LValue {
    enum class Kind { Var /*, Field */ };

    Kind kind = Kind::Var;
    Env* env = nullptr;
    std::string name;

    static LValue var(Env& e, std::string n) {
        LValue lv;
        lv.kind = Kind::Var;
        lv.env = &e;
        lv.name = std::move(n);
        return lv;
    }
};

} // namespace interp
