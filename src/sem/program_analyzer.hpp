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
    Analyzer fun_analyzer;

    static void check_main_signature(const Scope& global) {
        bool ok = false;

        auto it = global.funcs.find("main");
        if (it == global.funcs.end()) return; // main optional

        for (const auto& f : it->second) {
            if (!f.param_types.empty()) continue;
            auto rt = Analyzer::base_type(f.return_type);
            if (rt == ast::Type::Int() || rt == ast::Type::Void()) ok = true;
        }

        if (!ok) {
            throw std::runtime_error("semantic error: invalid main signature (allowed: int main() or void main())");
        }
    }

    void analyze(const ast::Program& p) {
        Scope global;
        ClassTable ct;

        // ---------- PASS 0 (classes): collect names ----------
        for (const auto& c : p.classes) ct.add_class_name(c.name);

        // ---------- PASS 1 (classes): fill members + base links ----------
        for (const auto& c : p.classes) ct.fill_class_members(c);

        // ---------- PASS 2 (classes): inheritance validity ----------
        ct.check_inheritance();

        // ---------- PASS 3 (classes): override checks ----------
        ct.check_overrides_and_virtuals();

        // ---------- PASS 1 (functions): collect function signatures ----------
        for (const auto& f : p.functions) {
            FuncSymbol sym;
            sym.name = f.name;
            sym.return_type = f.return_type;

            sym.param_types.reserve(f.params.size());
            for (const auto& par : f.params) sym.param_types.push_back(par.type);

            global.define_func(sym);
        }

        check_main_signature(global);

        // ---------- PASS 2 (functions): check bodies ----------
        for (const auto& f : p.functions) {
            fun_analyzer.check_function(global, f);
        }
    }
};

} // namespace sem
