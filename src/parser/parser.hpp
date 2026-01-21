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
    static std::unordered_set<std::string> prescan_class_names(const std::vector<lexer::Token>& toks) {
        std::unordered_set<std::string> cn;
        for (size_t k = 0; k + 1 < toks.size(); ++k) {
            if (toks[k].lexeme == "class" && toks[k + 1].kind == lexer::TokenKind::Identifier) {
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
        if (!is_end() && peek().lexeme == lx) { ++i_; return true; }
        return false;
    }

    void expect_lex(std::string_view lx, const char* msg) {
        if (!match_lex(lx)) {
            throw err_here(std::string(msg) + " (expected '" + std::string(lx) + "', got '" + peek().lexeme + "')");
        }
    }

    bool peek_lex(std::string_view lx) const { return !is_end() && peek().lexeme == lx; }

    bool peek_is_ident() const { return !is_end() && peek().kind == lexer::TokenKind::Identifier; }

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

    // ---------- literal decoding (lexer keeps raw text with quotes/escapes) ----------
    static char remember_escape(char esc) {
        switch (esc) {
            case 'n': return '\n';
            case 't': return '\t';
            case 'r': return '\r';
            case '0': return '\0';
            case '\\': return '\\';
            case '\'': return '\'';
            case '"': return '"';
            default: throw std::runtime_error(std::string("unknown escape \\") + esc);
        }
    }

    static char decode_char_lit(const std::string& raw) {
        // raw form: 'a' or '\n' etc.
        if (raw.size() < 3 || raw.front() != '\'' || raw.back() != '\'')
            throw std::runtime_error("invalid char literal: " + raw);

        if (raw[1] == '\\') {
            if (raw.size() != 4) throw std::runtime_error("invalid escaped char literal: " + raw);
            return remember_escape(raw[2]);
        }

        if (raw.size() != 3) throw std::runtime_error("invalid char literal: " + raw);
        return raw[1];
    }

    static std::string decode_string_lit(const std::string& raw) {
        // raw form: "..." including escapes
        if (raw.size() < 2 || raw.front() != '"' || raw.back() != '"')
            throw std::runtime_error("invalid string literal: " + raw);

        std::string out;
        out.reserve(raw.size());

        for (size_t i = 1; i + 1 < raw.size(); ++i) {
            char ch = raw[i];
            if (ch != '\\') {
                out.push_back(ch);
                continue;
            }
            // escape
            if (i + 1 >= raw.size() - 1) throw std::runtime_error("unfinished escape in string literal");
            char esc = raw[++i];
            out.push_back(remember_escape(esc));
        }
        return out;
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

        if (match_lex("public")) {
            expect_lex(":", "expected ':' after 'public'");
        }

        while (!match_lex("}")) {
            if (is_end()) throw err_here("unexpected end in class body");

            bool is_virtual = false;
            if (match_lex("virtual")) is_virtual = true;

            // ctor: ClassName(...)
            if (peek_is_ident() && peek().lexeme == c.name && peek(1).lexeme == "(") {
                (void)take_ident("expected ctor name");
                expect_lex("(", "expected '(' after ctor name");
                ast::ConstructorDef ctor;
                ctor.params = parse_param_list();
                ctor.body = parse_block_stmt();
                c.ctors.push_back(std::move(ctor));
                continue;
            }

            ast::Type t = parse_type();
            std::string member_name = take_ident("expected member name");

            if (match_lex("(")) {
                ast::MethodDef m;
                m.is_virtual = is_virtual;
                m.return_type = t;
                m.name = member_name;
                m.params = parse_param_list();
                m.body = parse_block_stmt();
                c.methods.push_back(std::move(m));
            } else {
                ast::FieldDecl fld;
                fld.type = t;
                fld.name = member_name;

                if (match_lex("=")) {
                    (void)parse_expr(); // consume initializer (currently ignored)
                }

                expect_lex(";", "expected ';' after field");
                c.fields.push_back(std::move(fld));
            }
        }

        match_lex(";");
        return c;
    }

    // ---------- statements ----------
    ast::StmtPtr parse_stmt() {
        if (peek_lex("{")) return parse_block_stmt();

        if (match_lex("if")) {
            auto s = std::make_unique<ast::IfStmt>();
            expect_lex("(", "expected '(' after if");
            s->cond = parse_expr();
            expect_lex(")", "expected ')' after if condition");
            s->then_branch = parse_stmt();
            if (match_lex("else")) {
                s->else_branch = parse_stmt();
            }
            return s;
        }

        if (match_lex("while")) {
            auto s = std::make_unique<ast::WhileStmt>();
            expect_lex("(", "expected '(' after while");
            s->cond = parse_expr();
            expect_lex(")", "expected ')' after while condition");
            s->body = parse_stmt();
            return s;
        }

        if (match_lex("return")) {
            auto r = std::make_unique<ast::ReturnStmt>();
            if (!match_lex(";")) {
                r->value = parse_expr();
                expect_lex(";", "expected ';' after return");
            }
            return r;
        }

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

    // ---------- expressions ----------
    ast::ExprPtr parse_expr() { return parse_assignment(); }

    ast::ExprPtr parse_assignment() {
        auto e = parse_logical_or();

        if (match_lex("=")) {
            auto rhs = parse_assignment();

            if (auto* ve = dynamic_cast<ast::VarExpr*>(e.get())) {
                auto a = std::make_unique<ast::AssignExpr>();
                a->name = ve->name;
                a->value = std::move(rhs);
                return a;
            }

            if (auto* me = dynamic_cast<ast::MemberAccessExpr*>(e.get())) {
                auto fa = std::make_unique<ast::FieldAssignExpr>();
                fa->object = std::move(me->object);
                fa->field = me->field;
                fa->value = std::move(rhs);
                return fa;
            }

            throw err_here("left side of assignment must be a variable or field");
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
            return parse_unary();
        }
        if (match_lex("-")) {
            auto u = std::make_unique<ast::UnaryExpr>();
            u->op = ast::UnaryExpr::Op::Neg;
            u->expr = parse_unary();
            return u;
        }
        return parse_postfix();
    }

    ast::ExprPtr parse_postfix() {
        auto e = parse_primary();

        for (;;) {
            if (match_lex(".")) {
                std::string field = take_ident("expected field/method name after '.'");

                if (match_lex("(")) {
                    auto mc = std::make_unique<ast::MethodCallExpr>();
                    mc->object = std::move(e);
                    mc->method = std::move(field);

                    if (!match_lex(")")) {
                        for (;;) {
                            mc->args.push_back(parse_expr());
                            if (match_lex(")")) break;
                            expect_lex(",", "expected ',' or ')'");
                        }
                    }

                    e = std::move(mc);
                    continue;
                } else {
                    auto ma = std::make_unique<ast::MemberAccessExpr>();
                    ma->object = std::move(e);
                    ma->field = std::move(field);

                    e = std::move(ma);
                    continue;
                }
            }

            break;
        }

        return e;
    }

    ast::ExprPtr parse_primary() {
        if (match_lex("(")) {
            auto e = parse_expr();
            expect_lex(")", "expected ')'");
            return e;
        }

        if (!is_end() && peek().kind == lexer::TokenKind::IntLit) {
            int v = std::stoi(peek().lexeme);
            ++i_;
            return std::make_unique<ast::IntLiteral>(v);
        }

        if (!is_end() && peek().kind == lexer::TokenKind::StringLit) {
            std::string s = decode_string_lit(peek().lexeme);
            ++i_;
            return std::make_unique<ast::StringLiteral>(std::move(s));
        }

        if (!is_end() && peek().kind == lexer::TokenKind::CharLit) {
            char c = decode_char_lit(peek().lexeme);
            ++i_;
            return std::make_unique<ast::CharLiteral>(c);
        }

        if (match_lex("true"))  return std::make_unique<ast::BoolLiteral>(true);
        if (match_lex("false")) return std::make_unique<ast::BoolLiteral>(false);

        if (peek_is_ident()) {
            std::string name = take_ident("expected identifier");

            if (match_lex("(")) {
                std::vector<ast::ExprPtr> args;
                if (!match_lex(")")) {
                    for (;;) {
                        args.push_back(parse_expr());
                        if (match_lex(")")) break;
                        expect_lex(",", "expected ',' or ')'");
                    }
                }

                if (class_names_.count(name)) {
                    auto c = std::make_unique<ast::ConstructExpr>();
                    c->class_name = std::move(name);
                    c->args = std::move(args);
                    return c;
                } else {
                    auto call = std::make_unique<ast::CallExpr>();
                    call->callee = std::move(name);
                    call->args = std::move(args);
                    return call;
                }
            }

            return std::make_unique<ast::VarExpr>(std::move(name));
        }

        throw err_here("expected expression");
    }
};

} // namespace parser
