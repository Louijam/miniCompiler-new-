#pragma once

#include <cctype>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include "../lexer/lexer.hpp"
#include "../lexer/token.hpp"

#include "../ast/program.hpp"
#include "../ast/class.hpp"
#include "../ast/function.hpp"
#include "../ast/stmt.hpp"
#include "../ast/expr.hpp"
#include "../ast/type.hpp"

namespace parser {

struct ParseError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

class Parser {
public:
    explicit Parser(std::vector<lexer::Token> toks, std::unordered_set<std::string> class_names = {})
        : tokens_(std::move(toks)), class_names_(std::move(class_names)) {}

    ast::Program parse_program() {
        ast::Program p;
        while (!is_end()) {
            if (peek_lex("class")) {
                p.classes.push_back(parse_class_def());
            } else {
                p.functions.push_back(parse_function_def());
            }
        }
        return p;
    }

    static ast::Program parse_source(std::string_view src) {
        lexer::Lexer lx(src);
        auto toks = lx.tokenize();
        auto cn = prescan_class_names(toks);
        Parser ps(std::move(toks), std::move(cn));
        return ps.parse_program();
    }

private:
    std::vector<lexer::Token> tokens_;
    std::unordered_set<std::string> class_names_;
    size_t i_ = 0;

private:
    // ---------- token helpers ----------
    static std::unordered_set<std::string> prescan_class_names(const std::vector<lexer::Token>& toks) {
        std::unordered_set<std::string> cn;
        for (size_t k = 0; k + 1 < toks.size(); ++k) {
            if (toks[k].lexeme == "class") {
                cn.insert(toks[k + 1].lexeme);
            }
        }
        return cn;
    }

    bool is_end() const {
        return i_ >= tokens_.size() || tokens_[i_].kind == lexer::TokenKind::End;
    }

    const lexer::Token& peek(int off = 0) const {
        size_t j = i_ + static_cast<size_t>(off);
        if (j >= tokens_.size()) return tokens_.back();
        return tokens_[j];
    }

    bool match_lex(std::string_view lx) {
        if (!is_end() && peek().lexeme == lx) {
            ++i_;
            return true;
        }
        return false;
    }

    void expect_lex(std::string_view lx, const char* msg) {
        if (!match_lex(lx)) {
            throw err_here(std::string(msg) + " (expected '" + std::string(lx) + "', got '" + peek().lexeme + "')");
        }
    }

    bool peek_lex(std::string_view lx) const {
        return !is_end() && peek().lexeme == lx;
    }

    bool peek_is_ident() const {
        return !is_end() && peek().kind == lexer::TokenKind::Identifier;
    }

    std::string take_ident(const char* msg) {
        if (!peek_is_ident()) throw err_here(msg);
        std::string s = peek().lexeme;
        ++i_;
        return s;
    }

    ParseError err_here(const std::string& msg) const {
        const auto& t = peek();
        return ParseError("ParseError at " + std::to_string(t.line) + ":" + std::to_string(t.col) + ": " + msg);
    }

    // ---------- types ----------
    ast::Type parse_type() {
        ast::Type t;

        if (match_lex("int")) t = ast::Type::Int(false);
        else if (match_lex("bool")) t = ast::Type::Bool(false);
        else if (match_lex("char")) t = ast::Type::Char(false);
        else if (match_lex("string")) t = ast::Type::String(false);
        else if (match_lex("void")) t = ast::Type::Void();
        else if (peek_is_ident()) {
            std::string cn = take_ident("expected type name");
            t = ast::Type::Class(std::move(cn), false);
        } else {
            throw err_here("expected type");
        }

        if (match_lex("&")) t.is_ref = true;
        return t;
    }

    ast::Param parse_param() {
        ast::Param p;
        p.type = parse_type();
        p.name = take_ident("expected parameter name");
        return p;
    }

    std::vector<ast::Param> parse_param_list() {
        std::vector<ast::Param> ps;
        if (match_lex(")")) return ps;
        for (;;) {
            ps.push_back(parse_param());
            if (match_lex(")")) break;
            expect_lex(",", "expected ',' or ')'");
        }
        return ps;
    }

    // ---------- program-level ----------
    ast::FunctionDef parse_function_def() {
        ast::FunctionDef f;
        f.return_type = parse_type();
        f.name = take_ident("expected function name");
        expect_lex("(", "expected '(' after function name");
        f.params = parse_param_list();
        f.body = parse_block_stmt();
        return f;
    }

    // Minimal class parse (enough to not crash if class exists)
    ast::ClassDef parse_class_def() {
        expect_lex("class", "expected 'class'");
        ast::ClassDef c;
        c.name = take_ident("expected class name");

        if (match_lex(":")) {
            expect_lex("public", "expected 'public' after ':'");
            c.base_name = take_ident("expected base class name");
        } else {
            c.base_name = "";
        }

        expect_lex("{", "expected '{' in class body");

        // optional "public:"
        if (match_lex("public")) expect_lex(":", "expected ':' after 'public'");

        // Skip members for now (consume tokens until '}')
        while (!match_lex("}")) {
            if (is_end()) throw err_here("unexpected end in class body");
            ++i_;
        }

        // optional ';'
        match_lex(";");
        return c;
    }

    // ---------- statements ----------
    ast::StmtPtr parse_stmt() {
        if (match_lex("{")) {
            --i_; // let parse_block consume '{'
            return parse_block_stmt();
        }

        if (match_lex("return")) {
            auto r = std::make_unique<ast::ReturnStmt>();
            if (!match_lex(";")) {
                r->value = parse_expr();
                expect_lex(";", "expected ';' after return");
            }
            return r;
        }

        // variable declaration: Type ident (= expr)? ;
        // Heuristic: if next token is a type keyword OR identifier that is a known class name
        if (peek_lex("int") || peek_lex("bool") || peek_lex("char") || peek_lex("string") || peek_lex("void") ||
            (peek_is_ident() && class_names_.count(peek().lexeme))) {

            ast::Type t = parse_type();
            std::string name = take_ident("expected variable name");

            auto s = std::make_unique<ast::VarDeclStmt>();
            s->decl_type = t;
            s->name = name;

            if (match_lex("=")) {
                s->init = parse_expr();
            }
            expect_lex(";", "expected ';' after variable declaration");
            return s;
        }

        // expression statement
        auto es = std::make_unique<ast::ExprStmt>();
        es->expr = parse_expr();
        expect_lex(";", "expected ';' after expression");
        return es;
    }

    std::unique_ptr<ast::BlockStmt> parse_block_stmt() {
        expect_lex("{", "expected '{' to start block");
        auto b = std::make_unique<ast::BlockStmt>();
        while (!match_lex("}")) {
            if (is_end()) throw err_here("unexpected end in block");
            b->statements.push_back(parse_stmt());
        }
        return b;
    }

    // ---------- expressions (precedence) ----------
    ast::ExprPtr parse_expr() { return parse_assignment(); }

    ast::ExprPtr parse_assignment() {
        auto e = parse_logical_or();
        if (match_lex("=")) {
            // Only support simple name = expr for now
            auto* ve = dynamic_cast<ast::VarExpr*>(e.get());
            if (!ve) throw err_here("left side of assignment must be a variable");
            auto a = std::make_unique<ast::AssignExpr>();
            a->name = ve->name;
            a->value = parse_assignment();
            return a;
        }
        return e;
    }

    ast::ExprPtr parse_logical_or() {
        auto e = parse_logical_and();
        while (match_lex("||")) {
            auto b = std::make_unique<ast::BinaryExpr>();
            b->op = ast::BinaryExpr::Op::OrOr;
            b->left = std::move(e);
            b->right = parse_logical_and();
            e = std::move(b);
        }
        return e;
    }

    ast::ExprPtr parse_logical_and() {
        auto e = parse_equality();
        while (match_lex("&&")) {
            auto b = std::make_unique<ast::BinaryExpr>();
            b->op = ast::BinaryExpr::Op::AndAnd;
            b->left = std::move(e);
            b->right = parse_equality();
            e = std::move(b);
        }
        return e;
    }

    ast::ExprPtr parse_equality() {
        auto e = parse_relational();
        for (;;) {
            if (match_lex("==")) {
                auto b = std::make_unique<ast::BinaryExpr>();
                b->op = ast::BinaryExpr::Op::Eq;
                b->left = std::move(e);
                b->right = parse_relational();
                e = std::move(b);
            } else if (match_lex("!=")) {
                auto b = std::make_unique<ast::BinaryExpr>();
                b->op = ast::BinaryExpr::Op::Ne;
                b->left = std::move(e);
                b->right = parse_relational();
                e = std::move(b);
            } else break;
        }
        return e;
    }

    ast::ExprPtr parse_relational() {
        auto e = parse_additive();
        for (;;) {
            if (match_lex("<")) {
                auto b = std::make_unique<ast::BinaryExpr>();
                b->op = ast::BinaryExpr::Op::Lt;
                b->left = std::move(e);
                b->right = parse_additive();
                e = std::move(b);
            } else if (match_lex("<=")) {
                auto b = std::make_unique<ast::BinaryExpr>();
                b->op = ast::BinaryExpr::Op::Le;
                b->left = std::move(e);
                b->right = parse_additive();
                e = std::move(b);
            } else if (match_lex(">")) {
                auto b = std::make_unique<ast::BinaryExpr>();
                b->op = ast::BinaryExpr::Op::Gt;
                b->left = std::move(e);
                b->right = parse_additive();
                e = std::move(b);
            } else if (match_lex(">=")) {
                auto b = std::make_unique<ast::BinaryExpr>();
                b->op = ast::BinaryExpr::Op::Ge;
                b->left = std::move(e);
                b->right = parse_additive();
                e = std::move(b);
            } else break;
        }
        return e;
    }

    ast::ExprPtr parse_additive() {
        auto e = parse_multiplicative();
        for (;;) {
            if (match_lex("+")) {
                auto b = std::make_unique<ast::BinaryExpr>();
                b->op = ast::BinaryExpr::Op::Add;
                b->left = std::move(e);
                b->right = parse_multiplicative();
                e = std::move(b);
            } else if (match_lex("-")) {
                auto b = std::make_unique<ast::BinaryExpr>();
                b->op = ast::BinaryExpr::Op::Sub;
                b->left = std::move(e);
                b->right = parse_multiplicative();
                e = std::move(b);
            } else break;
        }
        return e;
    }

    ast::ExprPtr parse_multiplicative() {
        auto e = parse_unary();
        for (;;) {
            if (match_lex("*")) {
                auto b = std::make_unique<ast::BinaryExpr>();
                b->op = ast::BinaryExpr::Op::Mul;
                b->left = std::move(e);
                b->right = parse_unary();
                e = std::move(b);
            } else if (match_lex("/")) {
                auto b = std::make_unique<ast::BinaryExpr>();
                b->op = ast::BinaryExpr::Op::Div;
                b->left = std::move(e);
                b->right = parse_unary();
                e = std::move(b);
            } else if (match_lex("%")) {
                auto b = std::make_unique<ast::BinaryExpr>();
                b->op = ast::BinaryExpr::Op::Mod;
                b->left = std::move(e);
                b->right = parse_unary();
                e = std::move(b);
            } else break;
        }
        return e;
    }

    ast::ExprPtr parse_unary() {
        if (match_lex("!")) {
            auto u = std::make_unique<ast::UnaryExpr>();
            u->op = ast::UnaryExpr::Op::Not;
            u->expr = parse_unary();
            return u;
        }
        if (match_lex("+")) {
            // unary plus: no-op
            return parse_unary();
        }
        if (match_lex("-")) {
            auto u = std::make_unique<ast::UnaryExpr>();
            u->op = ast::UnaryExpr::Op::Neg;
            u->expr = parse_unary();
            return u;
        }
        return parse_primary();
    }

    ast::ExprPtr parse_primary() {
        if (match_lex("(")) {
            auto e = parse_expr();
            expect_lex(")", "expected ')'");
            return e;
        }

        // literals / identifiers
        if (!is_end() && peek().kind == lexer::TokenKind::IntLit) {
            int v = std::stoi(peek().lexeme);
            ++i_;
            return std::make_unique<ast::IntLiteral>(v);
        }
        if (!is_end() && peek().kind == lexer::TokenKind::StringLit) {
            std::string s = peek().lexeme;
            ++i_;
            return std::make_unique<ast::StringLiteral>(std::move(s));
        }
        if (match_lex("true"))  return std::make_unique<ast::BoolLiteral>(true);
        if (match_lex("false")) return std::make_unique<ast::BoolLiteral>(false);

        if (peek_is_ident()) {
            std::string name = take_ident("expected identifier");

            // call: name(...)
            if (match_lex("(")) {
                auto call = std::make_unique<ast::CallExpr>();
                call->callee = name;

                if (!match_lex(")")) {
                    for (;;) {
                        call->args.push_back(parse_expr());
                        if (match_lex(")")) break;
                        expect_lex(",", "expected ',' or ')'");
                    }
                }
                return call;
            }

            return std::make_unique<ast::VarExpr>(std::move(name));
        }

        throw err_here("expected expression");
    }
};

} // namespace parser
