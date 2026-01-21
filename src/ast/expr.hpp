#pragma once
// Verhindert mehrfaches Einbinden dieser Header-Datei

#include <memory>   // std::unique_ptr
#include <string>   // std::string
#include <vector>   // std::vector

namespace ast {

// Basisklasse aller Ausdrucks-Knoten im AST
struct Expr {
    virtual ~Expr() = default; // Virtueller Destruktor fuer polymorphe Nutzung
};

// Eigentumszeiger fuer Ausdrucks-Knoten
using ExprPtr = std::unique_ptr<Expr>;

// Integer-Literal, z.B. 42
struct IntLiteral : Expr {
    int value;                       // Wert des Literals
    explicit IntLiteral(int v)       // Expliziter Konstruktor
        : value(v) {}
};

// Boolean-Literal, z.B. true / false
struct BoolLiteral : Expr {
    bool value;                      // Wert des Literals
    explicit BoolLiteral(bool v)
        : value(v) {}
};

// Zeichen-Literal, z.B. 'a'
struct CharLiteral : Expr {
    char value;                      // Wert des Literals
    explicit CharLiteral(char v)
        : value(v) {}
};

// String-Literal, z.B. "hello"
struct StringLiteral : Expr {
    std::string value;               // Inhalt des Strings
    explicit StringLiteral(std::string v)
        : value(std::move(v)) {}     // Move, um Kopien zu vermeiden
};

// Zugriff auf eine Variable, z.B. x
struct VarExpr : Expr {
    std::string name;                // Name der Variable
    explicit VarExpr(std::string n)
        : name(std::move(n)) {}
};

// Zuweisung an eine Variable: x = expr
// Feldzuweisungen werden separat als FieldAssignExpr modelliert
struct AssignExpr : Expr {
    std::string name;                // Name der Zielvariable
    ExprPtr value;                   // Rechter Ausdruck der Zuweisung
};

// Zuweisung an ein Objektfeld: obj.f = expr
struct FieldAssignExpr : Expr {
    ExprPtr object;                  // Ausdruck, der das Objekt liefert
    std::string field;               // Name des Feldes
    ExprPtr value;                   // Zuzuweisender Ausdruck
};

// Unärer Ausdruck, z.B. -x oder !x
struct UnaryExpr : Expr {
    enum class Op {
        Neg,                         // Arithmetische Negation (-)
        Not                          // Logische Negation (!)
    } op;
    ExprPtr expr;                    // Operand
};

// Binärer Ausdruck, z.B. a + b, a && b, a < b
struct BinaryExpr : Expr {
    enum class Op {
        Add, Sub, Mul, Div, Mod,     // Arithmetische Operatoren
        Lt, Le, Gt, Ge,              // Vergleichsoperatoren
        Eq, Ne,                      // Gleich / Ungleich
        AndAnd, OrOr                 // Logische Operatoren
    } op;
    ExprPtr left;                    // Linker Operand
    ExprPtr right;                   // Rechter Operand
};

// Funktionsaufruf: f(args)
struct CallExpr : Expr {
    std::string callee;              // Name der Funktion
    std::vector<ExprPtr> args;       // Argumente des Aufrufs
};

// Objekterzeugung: T(args)
// Wird z.B. verwendet in: T x = T(args);
struct ConstructExpr : Expr {
    std::string class_name;          // Name der zu konstruierenden Klasse
    std::vector<ExprPtr> args;       // Konstruktorargumente
};

// Feldzugriff: obj.f
struct MemberAccessExpr : Expr {
    ExprPtr object;                  // Ausdruck, der das Objekt liefert
    std::string field;               // Name des Feldes
};

// Methodenaufruf: obj.m(args)
struct MethodCallExpr : Expr {
    ExprPtr object;                  // Ausdruck, der das Objekt liefert
    std::string method;              // Name der Methode
    std::vector<ExprPtr> args;       // Argumente des Methodenaufrufs
};

} // namespace ast
