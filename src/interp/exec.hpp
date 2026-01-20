#pragma once

#include "../ast/expr.hpp"
#include "../sem/class_table.hpp"

#include "env.hpp"
#include "value.hpp"
#include "functions.hpp"
#include "methods.hpp"

namespace interp {

inline Value eval_expr(
    Env& env,
    const ast::Expr& e,
    FunctionTable& functions,
    MethodTable& methods,
    const sem::ClassTable& classes
) {
    (void)env;
    (void)e;
    (void)functions;
    (void)methods;
    (void)classes;
    return Value{};
}

} // namespace interp
