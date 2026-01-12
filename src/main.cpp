#include "interp/exec.hpp"
#include <string>

int main() {
    using namespace ast;
    using namespace interp;

    Env env;
    FunctionTable funcs;

    env.define_value("i", 42);
    env.define_value("b", true);
    env.define_value("c", 'Z');
    env.define_value("s", std::string("hello"));

    auto call_i = std::make_unique<CallExpr>();
    call_i->callee = "print_int";
    call_i->args.push_back(std::make_unique<VarExpr>("i"));

    auto call_b = std::make_unique<CallExpr>();
    call_b->callee = "print_bool";
    call_b->args.push_back(std::make_unique<VarExpr>("b"));

    auto call_c = std::make_unique<CallExpr>();
    call_c->callee = "print_char";
    call_c->args.push_back(std::make_unique<VarExpr>("c"));

    auto call_s = std::make_unique<CallExpr>();
    call_s->callee = "print_string";
    call_s->args.push_back(std::make_unique<VarExpr>("s"));

    (void)eval_expr(env, *call_i, funcs);
    (void)eval_expr(env, *call_b, funcs);
    (void)eval_expr(env, *call_c, funcs);
    (void)eval_expr(env, *call_s, funcs);

    return 0;
}
