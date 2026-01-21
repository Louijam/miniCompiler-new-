#pragma once
// Verhindert mehrfaches Einbinden dieser Header-Datei

#include <string>   // std::string
#include <vector>   // std::vector
#include <memory>   // std::unique_ptr

#include "stmt.hpp" // Definition von Statement-AST-Knoten (Stmt)
#include "type.hpp" // Definition des Typsystems (Type)

namespace ast {

// Beschreibt einen Funktionsparameter
struct Param {
    std::string name;    // Name des Parameters
    Type type;           // Typ des Parameters
};

// Beschreibt eine Funktionsdefinition im AST
struct FunctionDef {
    std::string name;                // Name der Funktion
    Type return_type;                // Rueckgabetyp der Funktion
    std::vector<Param> params;       // Parameterliste der Funktion
    std::unique_ptr<Stmt> body;      // Funktionsrumpf als Statement-AST
};

} // namespace ast
