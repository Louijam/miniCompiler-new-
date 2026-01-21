#pragma once
// Verhindert mehrfaches Einbinden dieser Header-Datei

#include <memory>    // std::unique_ptr
#include <vector>    // std::vector
#include <optional>  // std::optional (hier vorbereitet, evtl. spaeter genutzt)
#include <string>    // std::string

#include "type.hpp"  // Definition des Typsystems (Type)

namespace ast {

// Basisklasse aller Statement-Knoten im AST
struct Stmt {
    virtual ~Stmt() = default; // Virtueller Destruktor fuer polymorphe Nutzung
};

// Eigentumszeiger fuer Statement-Knoten
using StmtPtr = std::unique_ptr<Stmt>;

// Block von Statements: { stmt1; stmt2; ... }
struct BlockStmt : Stmt {
    std::vector<StmtPtr> statements; // Sequenz von Statements im Block
};

struct Expr; // Forward-Deklaration, um zyklische Includes zu vermeiden

// Statement, das nur aus einem Ausdruck besteht (z.B. Funktionsaufruf)
struct ExprStmt : Stmt {
    std::unique_ptr<Expr> expr; // Auszufuehrender Ausdruck
};

// Variablendeklaration: T x = expr;
struct VarDeclStmt : Stmt {
    Type decl_type;                 // Deklarierter Typ der Variable (z.B. int, bool, T&, ...)
    std::string name;               // Name der Variable
    std::unique_ptr<Expr> init;     // Optionaler Initialisierer (kann null sein)
};

// If-Statement: if (cond) then_branch else else_branch
struct IfStmt : Stmt {
    std::unique_ptr<Expr> cond; // Bedingung
    StmtPtr then_branch;        // Dann-Zweig
    StmtPtr else_branch;        // Else-Zweig (kann null sein)
};

// While-Schleife: while (cond) body
struct WhileStmt : Stmt {
    std::unique_ptr<Expr> cond; // Schleifenbedingung
    StmtPtr body;               // Schleifenrumpf
};

// Return-Statement: return expr;
struct ReturnStmt : Stmt {
    std::unique_ptr<Expr> value; // Rueckgabewert (null bei void-return)
};

} // namespace ast
