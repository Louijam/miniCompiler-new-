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

    static void add_builtin(Scope& global, const std::string& name, const ast::Type& param) {
        FuncSymbol sym;
        sym.name = name;
        sym.return_type = ast::Type::Void();
        sym.param_types = { param };
        global.define_func(sym);
    }

    static void add_builtins(Scope& global) {
        add_builtin(global, "print_bool",   ast::Type::Bool(false));
        add_builtin(global, "print_int",    ast::Type::Int(false));
        add_builtin(global, "print_char",   ast::Type::Char(false));
        add_builtin(global, "print_string", ast::Type::String(false));
    }

    static void check_main_signature(const Scope& global) {
        bool ok = false;

        auto it = global.funcs.find("main");
        if (it == global.funcs.end()) return;

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

        // PASS 0: class names
        for (const auto& c : p.classes) ct.add_class_name(c.name);

        // PASS 1: class members
        for (const auto& c : p.classes) ct.fill_class_members(c);

        // PASS 2: inheritance checks
        ct.check_inheritance();

        // PASS 3: overrides
        ct.check_overrides_and_virtuals();

        analyzer.set_class_table(&ct);

        // Builtins always available
        add_builtins(global);

        // PASS 1: function signatures
        for (const auto& f : p.functions) {
            FuncSymbol sym;
            sym.name = f.name;
            sym.return_type = f.return_type;
            sym.param_types.reserve(f.params.size());
            for (const auto& par : f.params) sym.param_types.push_back(par.type);
            global.define_func(sym);
        }

        check_main_signature(global);

        // PASS 2: function bodies
        for (const auto& f : p.functions) analyzer.check_function(global, f);

        // PASS 4: method bodies
        for (const auto& c : p.classes) {
            for (const auto& m : c.methods) analyzer.check_method(global, ct, c.name, m);
        }
    }
};

} // namespace sem
