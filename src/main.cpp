#include <iostream>
#include "interp/env.hpp"
#include "interp/value.hpp"

int main() {
    using namespace interp;

    Env global;

    global.define_value("x", 1);
    global.define_ref("r", LValue::var(global, "x"));

    std::cout << "x=" << to_string(global.read_value("x")) << "\n";
    std::cout << "r=" << to_string(global.read_value("r")) << "\n";

    global.assign_value("r", 42);

    std::cout << "after r=42\n";
    std::cout << "x=" << to_string(global.read_value("x")) << "\n";
    std::cout << "r=" << to_string(global.read_value("r")) << "\n";

    return 0;
}
