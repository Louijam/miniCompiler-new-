#pragma once
// Verhindert mehrfaches Einbinden

#include <stdexcept> // std::runtime_error
#include <string>    // std::string

#include "analyzer.hpp"      // Analyzer: type_of_expr/check_stmt/check_function/check_method/check_constructor
#include "scope.hpp"         // Scope: globale Symboltabelle (Funktionen/Klassen)
#include "class_table.hpp"   // ClassTable: Klassenhierarchie + Member-Signaturen
#include "../ast/program.hpp"// AST: Program
#include "../ast/type.hpp"   // AST: Type

namespace sem {

// ProgramAnalyzer: Entry-Point für semantische Analyse eines gesamten Programms.
// Baut erst Symboltabellen (Klassen + Funktionen) auf und lässt dann den Analyzer drüber laufen.
struct ProgramAnalyzer {
    Analyzer analyzer; // wiederverwendbarer Analyzer (benötigt ClassTable-Pointer)

    // Prüft main-Signatur (nur wenn main existiert):
    // erlaubt: int main() oder void main()
    static void check_main_signature(const Scope& global) {
        if (!global.has_func("main")) return;

        const auto& ovs = global.lookup_funcs("main");
        bool ok = false;

        for (const auto& f : ovs) {
            // main darf keine Parameter haben (in diesem Projekt)
            if (!f.param_types.empty()) continue;

            // return type muss int oder void sein
            if (f.return_type == ast::Type::Int() || f.return_type == ast::Type::Void()) {
                ok = true;
                break;
            }
        }

        if (!ok) throw std::runtime_error("semantic error: invalid main signature");
    }

    // Führt die vollständige semantische Analyse aus:
    // 1) Klassen-Infos bauen (ClassTable)
    // 2) globalen Scope mit Klassen + Funktionssignaturen füllen
    // 3) main-Signatur prüfen
    // 4) Analyzer auf Funktionen, Konstruktoren und Methoden anwenden
    void analyze(const ast::Program& p) {
        // Klassenstruktur und Member-Signaturen sammeln/validieren
        ClassTable ct;
        ct.build(p);

        // Globaler Scope enthält Klassennamen + Funktionsüberladungen
        Scope global(nullptr);

        // Klassen registrieren (für Typchecks: "ist das ein Klassentyp?")
        for (const auto& c : p.classes) {
            global.define_class(c.name);
        }

        // Funktionssignaturen registrieren (Overload-Resolution im Analyzer)
        for (const auto& f : p.functions) {
            FuncSymbol sym;
            sym.name = f.name;
            sym.return_type = f.return_type;

            sym.param_types.reserve(f.params.size());
            for (const auto& par : f.params) sym.param_types.push_back(par.type);

            global.define_func(sym);
        }

        // main() optional, aber wenn vorhanden muss Signatur passen
        check_main_signature(global);

        // Analyzer braucht ClassTable für:
        // - base_of checks
        // - field/method lookup in inheritance chain
        // - ctor/method overload resolution bei Klassen
        analyzer.set_class_table(&ct);

        // Funktionen typechecken
        for (const auto& f : p.functions) analyzer.check_function(global, f);

        // Klassen-Member typechecken (Ctors & Methoden)
        for (const auto& c : p.classes) {
            for (const auto& ctor : c.ctors) analyzer.check_constructor(global, ct, c.name, ctor);
            for (const auto& m : c.methods) analyzer.check_method(global, ct, c.name, m);
        }
    }
};

} // namespace sem
