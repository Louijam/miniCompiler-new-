#include <iostream>

#include "sem/scope.hpp"
#include "ast/type.hpp"

int main() {
    using namespace sem;
    using namespace ast;

    try {
        Scope global;

        // vars
        global.define_var("x", Type::Int());
        std::cout << "global x: " << to_string(global.lookup_var("x").type) << "\n";

        // nested scope shadows x
        Scope inner(&global);
        inner.define_var("x", Type::Bool());
        std::cout << "inner x: " << to_string(inner.lookup_var("x").type) << "\n";
        std::cout << "inner lookup global via parent y? -> expect error next\n";

        // funcs (overloads)
        FuncSymbol f1{"f", Type::Int(), {Type::Int(false)}};
        FuncSymbol f2{"f", Type::Int(), {Type::Int(true)}};
        global.define_func(f1);
        global.define_func(f2);

        // resolve exact
        auto& r1 = global.resolve_func("f", {Type::Int(false)});
        auto& r2 = global.resolve_func("f", {Type::Int(true)});
        std::cout << "resolve f(int): returns " << to_string(r1.return_type) << "\n";
        std::cout << "resolve f(int&): returns " << to_string(r2.return_type) << "\n";

        // trigger unknown variable error
        (void)inner.lookup_var("y");
    } catch (const std::exception& ex) {
        std::cout << "error=" << ex.what() << "\n";
    }

    return 0;
}
