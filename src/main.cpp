#include <iostream>

#include "ast/program.hpp"
#include "interp/env.hpp"
#include "interp/exec.hpp"
#include "interp/functions.hpp"
#include "interp/methods.hpp"
#include "sem/class_table.hpp"

int main() {
    ast::Program p;

    interp::Env global;
    interp::FunctionTable ft;
    interp::MethodTable methods;
    sem::ClassTable classes;

    (void)p;
    (void)global;
    (void)ft;
    (void)methods;
    (void)classes;

    return 0;
}
