#include "tools/dump_tokens.hpp"

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "repl/repl.hpp"
#include "repl/preprocess.hpp"

#include "parser/parser.hpp"

#include "interp/env.hpp"
#include "interp/exec.hpp"
#include "interp/functions.hpp"

#include "ast/program.hpp"
#include "ast/function.hpp"
#include "ast/stmt.hpp"
#include "ast/type.hpp"

static std::string read_file_or_throw(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("konnte Datei nicht oeffnen: " + path);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

static bool has_main(const ast::Program& p) {
    for (const auto& f : p.functions) {
        if (f.name == "main") return true;
    }
    return false;
}

static ast::FunctionDef& resolve_main(interp::FunctionTable& ft) {
    std::vector<ast::Type> arg_types;
    std::vector<bool> arg_is_lvalue;
    return ft.resolve("main", arg_types, arg_is_lvalue);
}

static int run_main_if_present(interp::Env& session_env, interp::FunctionTable& ft) {
    ast::FunctionDef& mainf = resolve_main(ft);

    std::vector<interp::Value> arg_vals;
    std::vector<interp::LValue> arg_lvals;

    interp::Value ret = interp::call_function(session_env, mainf, arg_vals, arg_lvals, ft);

    if (mainf.return_type == ast::Type::Int(false)) {
        if (auto* pi = std::get_if<int>(&ret)) return *pi;
        return 0;
    }
    return 0;
}

int main(int argc, char** argv) {
    if (mini_cpp::maybe_dump_tokens(argc, argv)) return 0;

    try {
        ast::Program global_program;
        interp::FunctionTable functions;
        interp::Env global_env(nullptr);
        interp::Env session_env(&global_env);

        // optional file load
        if (argc > 1) {
            std::string path = argv[1];
            if (!path.empty() && path[0] != '-') {
                std::string src = read_file_or_throw(path);
                src = repl::strip_preprocessor_lines(src);

                global_program = parser::Parser::parse_source(src);
                functions.add_program(global_program);

                if (has_main(global_program)) {
                    int code = run_main_if_present(session_env, functions);
                    if (code != 0) std::cerr << "main() returned " << code << "\n";
                }
            }
        } else {
            functions.add_program(global_program);
        }

        return repl::run_repl(global_program, functions, global_env, session_env);
    } catch (const std::exception& ex) {
        std::cerr << "FEHLER: " << ex.what() << "\n";
        return 1;
    }
}
