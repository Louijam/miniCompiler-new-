#include <iostream>

#include "interp/exec.hpp"

int main() {
    using namespace ast;
    using namespace interp;

    Env global;
    FunctionTable functions;

    auto f = std::make_unique<FunctionDef>();
    f->name = "f";
    f->return_type = Type::Int();

    auto ret = std::make_unique<ReturnStmt>();
    ret->value = std::make_unique<IntLiteral>(42);
    f->body = std::move(ret);

    functions.add(*f);

    CallExpr call;
    call.callee = "f";

    Value v = eval_expr(global, call, functions);
    std::cout << "f() = " << to_string(v) << "\n";
    return 0;
}
