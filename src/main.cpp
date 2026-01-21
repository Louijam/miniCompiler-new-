#include "tools/dump_tokens.hpp" // optional CLI-Tool: --dump-tokens <file>

#include <fstream>   // std::ifstream
#include <iostream>  // std::cout / std::cerr
#include <sstream>   // std::ostringstream
#include <stdexcept> // std::runtime_error
#include <string>    // std::string

#include "repl/repl.hpp"        // interactive REPL driver
#include "repl/preprocess.hpp"  // strip_preprocessor_lines()

#include "parser/parser.hpp"    // Parser::parse_source()

#include "interp/env.hpp"       // runtime environment (scopes + refs)
#include "interp/exec.hpp"      // eval/exec + call_function
#include "interp/functions.hpp" // FunctionTable + overload resolution

#include "ast/program.hpp"  // ast::Program
#include "ast/function.hpp" // ast::FunctionDef
#include "ast/stmt.hpp"     // statement nodes (for completeness / includes used elsewhere)
#include "ast/type.hpp"     // ast::Type

// Reads a complete file into a string.
// Throws a runtime_error if the file cannot be opened.
static std::string read_file_or_throw(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("konnte Datei nicht oeffnen: " + path);

    std::ostringstream ss;
    ss << in.rdbuf(); // stream entire file buffer into ss
    return ss.str();
}

// Checks whether the parsed program contains any function named "main".
static bool has_main(const ast::Program& p) {
    for (const auto& f : p.functions) {
        if (f.name == "main") return true;
    }
    return false;
}

// Finds the selected "main" overload via the runtime function table.
// Here: main() with zero args (no argc/argv in this project).
static ast::FunctionDef& resolve_main(interp::FunctionTable& ft) {
    std::vector<ast::Type> arg_types;     // empty => main()
    std::vector<bool> arg_is_lvalue;      // empty => no args
    return ft.resolve("main", arg_types, arg_is_lvalue);
}

// Executes main() and converts its return to process exit code.
// Policy:
// - int main(): return its integer result
// - void main(): always 0
static int run_main_if_present(interp::Env& session_env, interp::FunctionTable& ft) {
    ast::FunctionDef& mainf = resolve_main(ft);

    std::vector<interp::Value>  arg_vals;  // empty => main()
    std::vector<interp::LValue> arg_lvals; // empty => main()

    // Execute main in the current session environment
    interp::Value ret = interp::call_function(session_env, mainf, arg_vals, arg_lvals, ft);

    // Only int-returning main influences exit code
    if (mainf.return_type == ast::Type::Int(false)) {
        if (auto* pi = std::get_if<int>(&ret)) return *pi;
        return 0; // defensive fallback if runtime returns non-int
    }
    return 0;
}

int main(int argc, char** argv) {
    // Early-out: debug mode prints tokens and stops normal execution
    if (mini_cpp::maybe_dump_tokens(argc, argv)) return 0;

    try {
        // Global program holds parsed global definitions (classes + functions)
        ast::Program global_program;

        // Runtime tables (functions + class runtime metadata)
        interp::FunctionTable functions;

        // Two environments:
        // - global_env: root scope
        // - session_env: REPL/session scope that chains to global_env
        interp::Env global_env(nullptr);
        interp::Env session_env(&global_env);

        // Optional: load and run a file if a non-flag argument is provided
        if (argc > 1) {
            std::string path = argv[1];

            // Treat argv[1] as a file path only if it doesn't look like an option
            if (!path.empty() && path[0] != '-') {
                // Read + preprocess (strip #include etc.)
                std::string src = read_file_or_throw(path);
                src = repl::strip_preprocessor_lines(src);

                // Parse the whole file into a Program and build runtime tables from it
                global_program = parser::Parser::parse_source(src);
                functions.add_program(global_program);

                // If the file defines main(), run it once before entering REPL
                if (has_main(global_program)) {
                    int code = run_main_if_present(session_env, functions);
                    if (code != 0) std::cerr << "main() returned " << code << "\n";
                }
            }
        } else {
            // No file: start with an empty program, but still build the runtime tables
            functions.add_program(global_program);
        }

        // Start interactive REPL with current program + runtime tables + environments
        return repl::run_repl(global_program, functions, global_env, session_env);
    } catch (const std::exception& ex) {
        // Top-level error handling for file loading/parsing/runtime init
        std::cerr << "FEHLER: " << ex.what() << "\n";
        return 1;
    }
}
