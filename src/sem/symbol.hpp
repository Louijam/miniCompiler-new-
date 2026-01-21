#pragma once
// Verhindert mehrfaches Einbinden dieser Header-Datei

#include <string>         // std::string
#include <vector>         // std::vector
#include <unordered_map>  // (aktuell unbenutzt, aber evtl. bewusst fuer spaetere Erweiterung drin)
#include <stdexcept>      // (aktuell unbenutzt, aber haeufig in Semantik-Code noetig)

#include "../ast/type.hpp" // ast::Type (inkl. Ref-Flag)

namespace sem {

// Symbol fuer eine Variable im Scope:
// - name: Variablenname
// - type: statischer Typ (inkl. is_ref fuer Referenzen)
struct VarSymbol {
    std::string name;
    ast::Type type;
};

// Symbol fuer eine Funktion (eine konkrete Overload-Signatur):
// - name: Funktionsname
// - return_type: Rueckgabetyp
// - param_types: Parametertypen inkl. '&' Marker (Ref-Parameter)
struct FuncSymbol {
    std::string name;
    ast::Type return_type;
    std::vector<ast::Type> param_types; // incl. & marker
};

// Signaturvergleich fuer Funktions-Overloads:
// gleiche Signatur bedeutet: gleicher Name + gleiche Parametertypen (Reihenfolge/Ref inklusive)
inline bool same_signature(const FuncSymbol& a, const FuncSymbol& b) {
    if (a.name != b.name) return false;
    if (a.param_types.size() != b.param_types.size()) return false;

    for (size_t i = 0; i < a.param_types.size(); ++i) {
        if (a.param_types[i] != b.param_types[i]) return false;
    }

    return true;
}

} // namespace sem
