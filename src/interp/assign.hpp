#pragma once
#include <stdexcept>
#include <string>

#include "env.hpp"
#include "functions.hpp"
#include "object.hpp"
#include "value.hpp"
#include "../ast/type.hpp"

namespace interp {

// Assign to a named variable (NOT a field) with correct slicing behavior.
// - If LHS is class value (not ref) and RHS is class value and types differ by inheritance,
//   copy only the LHS static class fields (slicing) and set dynamic_class to LHS static class.
// - Otherwise: normal assignment.
inline void assign_value_slicing_aware(Env& env,
                                      const std::string& name,
                                      const Value& rhs,
                                      FunctionTable& functions) {
    ast::Type lhs_t = env.static_type_of(name);

    if (lhs_t.base == ast::Type::Base::Class && !lhs_t.is_ref) {
        if (auto* rhs_obj = std::get_if<ObjectPtr>(&rhs)) {
            if (!*rhs_obj) throw std::runtime_error("assignment from null object");

            // read current lhs value
            Value cur = env.read_value(name);
            auto* lhs_obj = std::get_if<ObjectPtr>(&cur);
            if (!lhs_obj || !*lhs_obj) throw std::runtime_error("assignment to non-object");

            // If types match exactly: deep-ish copy (field map + dynamic class)
            // If types differ: slicing to lhs static type
            const std::string lhs_static = lhs_t.class_name;
            const std::string rhs_dynamic = (*rhs_obj)->dynamic_class;

            if (lhs_static == rhs_dynamic) {
                (*lhs_obj)->dynamic_class = rhs_dynamic;
                (*lhs_obj)->fields = (*rhs_obj)->fields;
                return;
            }

            // slicing: keep only lhs_static merged fields
            const auto& lhs_ci = functions.class_rt.get(lhs_static);

            (*lhs_obj)->fields = (*rhs_obj)->fields;        // copy all first
            (*lhs_obj)->slice_to(lhs_static, lhs_ci.merged_fields);
            (*lhs_obj)->dynamic_class = lhs_static;          // now its a base object
            return;
        }
    }

    // fallback: normal assignment
    env.assign_value(name, rhs);
}

} // namespace interp
