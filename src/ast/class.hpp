#pragma once
// Verhindert mehrfaches Einbinden dieser Header-Datei

#include <string>   // std::string
#include <vector>   // std::vector
#include <memory>   // Smart Pointer (z.B. fuer StmtPtr)

#include "type.hpp"      // Definition des Typsystems (ast::Type)
#include "function.hpp"  // Definition von Funktionsparametern (ast::Param)
#include "stmt.hpp"      // Definition von Statement-AST-Knoten (StmtPtr)

namespace ast {

// Beschreibt ein Feld (Membervariable) innerhalb einer Klasse
struct FieldDecl {
    Type type;           // Typ des Feldes
    std::string name;    // Name des Feldes
};

// Beschreibt eine Methode innerhalb einer Klasse
struct MethodDef {
    bool is_virtual = false;           // true, falls Methode virtual deklariert ist
    std::string name;                  // Name der Methode
    Type return_type;                  // Rueckgabetyp der Methode
    std::vector<ast::Param> params;    // Parameterliste der Methode
    StmtPtr body;                      // Methodenrumpf als AST
};

// Beschreibt einen Konstruktor einer Klasse
// Konstruktoren haben keinen Rueckgabetyp
struct ConstructorDef {
    std::vector<ast::Param> params;    // Parameterliste des Konstruktors
    StmtPtr body;                      // Konstruktor-Rumpf als AST
};

// Beschreibt eine komplette Klassendefinition im AST
struct ClassDef {
    std::string name;                  // Klassenname
    std::string base_name;             // Basisklasse (leer => keine Vererbung)

    std::vector<FieldDecl> fields;     // Alle Felder der Klasse
    std::vector<ConstructorDef> ctors; // Alle Konstruktoren der Klasse
    std::vector<MethodDef> methods;    // Alle Methoden der Klasse
};

} // namespace ast
