#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>

#include "repl/repl.hpp"

#include "sem/class_table.hpp"
#include "ast/class.hpp"
#include "ast/type.hpp"
#include "ast/stmt.hpp"

static std::unique_ptr<ast::BlockStmt> empty_block() {
    auto b = std::make_unique<ast::BlockStmt>();
    return b;
}

static int run_virtual_selftest() {
    using namespace ast;
    using namespace sem;

    ClassDef B;
    B.name = "B";
    B.base_name = "";

    MethodDef bm;
    bm.is_virtual = true;
    bm.name = "m";
    bm.return_type = Type::Int(false);
    bm.body = empty_block();
    B.methods.push_back(std::move(bm));

    ClassDef D;
    D.name = "D";
    D.base_name = "B";

    MethodDef dm;
    dm.is_virtual = false; // IMPORTANT: not declared virtual
    dm.name = "m";
    dm.return_type = Type::Int(false);
    dm.body = empty_block();
    D.methods.push_back(std::move(dm));

    ClassTable ct;
    ct.add_class_name("B");
    ct.add_class_name("D");

    ct.fill_class_members(B);
    ct.fill_class_members(D);

    ct.check_inheritance();
    ct.check_overrides_and_virtuals();

    const auto& csD = ct.get_class("D");
    auto it = csD.methods.find("m");
    if (it == csD.methods.end() || it->second.empty()) throw std::runtime_error("selftest: missing D::m");

    if (!it->second[0].is_virtual) {
        throw std::runtime_error("selftest: expected D::m to be virtual (inherited from B::m)");
    }

    std::cout << "VIRTUAL SELFTEST OK\n";
    return 0;
}

int main(int argc, char** argv) {
    try {
        if (argc > 1) {
            std::string arg = argv[1];
            if (arg == "--selftest-virtual") return run_virtual_selftest();
        }
        return repl::run_repl();
    } catch (const std::exception& ex) {
        std::cerr << "FEHLER: " << ex.what() << "\n";
        return 1;
    }
}
