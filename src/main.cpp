#include <iostream>
#include "interp/exec.hpp"

int main() {
    using namespace ast;
    using namespace interp;

    Env env;
    FunctionTable funcs;

    env.define_value("c1", 'a');
    env.define_value("c2", 'b');
    env.define_value("s1", std::string("foo"));
    env.define_value("s2", std::string("foo"));
    env.define_value("s3", std::string("bar"));

    auto e1 = std::make_unique<BinaryExpr>();
    e1->op = BinaryExpr::Op::Lt;
    e1->left = std::make_unique<VarExpr>("c1");
    e1->right = std::make_unique<VarExpr>("c2");

    auto e2 = std::make_unique<BinaryExpr>();
    e2->op = BinaryExpr::Op::Eq;
    e2->left = std::make_unique<VarExpr>("s1");
    e2->right = std::make_unique<VarExpr>("s2");

    auto e3 = std::make_unique<BinaryExpr>();
    e3->op = BinaryExpr::Op::Ne;
    e3->left = std::make_unique<VarExpr>("s1");
    e3->right = std::make_unique<VarExpr>("s3");

    std::cout << "c1<c2=" << to_string(eval_expr(env, *e1, funcs)) << "\n";
    std::cout << "s1==s2=" << to_string(eval_expr(env, *e2, funcs)) << "\n";
    std::cout << "s1!=s3=" << to_string(eval_expr(env, *e3, funcs)) << "\n";
    return 0;
}
