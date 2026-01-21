#pragma once
// Verhindert mehrfaches Einbinden dieser Header-Datei

#include <vector>   // std::vector

#include "function.hpp" // Definition von FunctionDef
#include "class.hpp"    // Definition von ClassDef

namespace ast {

// Wurzelknoten des AST: gesamtes Programm
struct Program {
    std::vector<ClassDef> classes;       // Alle Klassendefinitionen im Programm
    std::vector<FunctionDef> functions;  // Alle freien Funktionsdefinitionen
};

} // namespace ast
