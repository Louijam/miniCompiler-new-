#pragma once
#include <stdexcept>
#include <string>

#include "analyzer.hpp"
#include "scope.hpp"
#include "class_table.hpp"
#include "../ast/program.hpp"
#include "../ast/type.hpp"

namespace sem {

struct ProgramAnalyzer {
    Analyzer analyzer;

    static void check_main_signature(const Scope& global) {
        if (!global.has_func("main")) return;

        const auto& ovs = global.lookup_funcs("main");
        bool ok = false;
        for (const auto& f : ovs) {
            if (!f.param_types.empty()) continue;
            if (f.return_type == ast::Type::Int() || f.return_type == ast::Type::Void()) {
                ok = true;
                break;
            }
        }
        if (!ok) throw std::runtime_error("semantic error: invalid main signature");
    }

    void analyze(const ast::Program& p) {
        ClassTable ct;
        ct.build(p);

        Scope global(nullptr);

        for (const auto& c : p.classes) {
            global.define_class(c.name);
        }

        for (const auto& f : p.functions) {
            FuncSymbol sym;
            sym.name = f.name;
            sym.return_type = f.return_type;
            sym.param_types.reserve(f.params.size());
            for (const auto& par : f.params) sym.param_types.push_back(par.type);
            global.define_func(sym);
        }

        check_main_signature(global);

        analyzer.set_class_table(&ct);

        for (const auto& f : p.functions) analyzer.check_function(global, f);

        for (const auto& c : p.classes) {
            for (const auto& ctor : c.ctors) analyzer.check_constructor(global, ct, c.name, ctor);
            for (const auto& m : c.methods) analyzer.check_method(global, ct, c.name, m);
        }
    }
};

} // namespace sem
