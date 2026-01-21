#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <unordered_set>
#include <stdexcept>
#include <utility>

#include "../ast/program.hpp"
#include "../ast/class.hpp"
#include "../ast/function.hpp"
#include "../ast/stmt.hpp"
#include "../ast/expr.hpp"
#include "../ast/type.hpp"

// Expect lexer API:
//   namespace lexer {
//     enum class TokenKind { ... , End };
//     struct Token { TokenKind kind; std::string lexeme; int line; int col; };
//     struct Lexer { explicit Lexer(std::string_view); Token next(); };
//   }
#include "../lexer/lexer.hpp"
#include "../lexer/token.hpp"

namespace parser {

struct ParseError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

class Parser {
public:
    explicit Parser(std::vector<lexer::Token> toks, std::unordered_set<std::string> class_names)
        : tokens_(std::move(toks)), class_names_(std::move(class_names)) {}

    ast::Program parse_program() {
        ast::Program p;
        while (!is_end()) {
            if (peek_is_kw("class")) {
                p.classes.push_back(parse_class_def());
            } else {
                p.functions.push_back(parse_function_def());
            }
        }
        return p;
    }

    // Convenience: parse from source string (lex + prescan class names + parse)
    static ast::Program parse_source(std::string_view src) {
        auto toks = lex_all(src);
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
    static std::vector<lexer::Token> lex_all(std::string_view src) {
        lexer::Lexer lx(src);
        std::vector<lexer::Token> out;
        for (;;) {
            auto t = lx.next();
            out.push_back(t);
            if (is_end_token(t)) break;
        }
        return out;
    }

    static bool is_end_token(const lexer::Token& t) {
        // Prefer kind == End, but tolerate lexeme-based implementations.
        if constexpr (requires { lexer::TokenKind::End; }) {
            if (t.kind == lexer::TokenKind::End) return true;
        }
        return t.lexeme == "<eof>" || t.lexeme == "EOF";
    }

    static std::unordered_set<std::string> prescan_class_names(const std::vector<lexer::Token>& toks) {
        std::unordered_set<std::string> cn;
        for (size_t k = 0; k + 1 < toks.size(); ++k) {
            if (toks[k].lexeme == "class") {
                // next identifier
                if (k + 1 < toks.size()) cn.insert(toks[k + 1].lexeme);
            }
        }
        return cn;
    }

    bool is_end() const {
        return i_ >= tokens_.size() || is_end_token(tokens_[i_]);
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

    bool peek_is_kw(std::string_view kw) const {
        return !is_end() && peek().lexeme == kw;
    }

    bool peek_is_ident() const {
        // Prefer TokenKind::Identifier if it exists, else fallback: "not punctuation/keyword".
        if (!is_end()) {
            if constexpr (requires { lexer::TokenKind::Identifier; }) {
                if (peek().kind == lexer::TokenKind::Identifier) return true;
            }
            // fallback heuristic:
            const std::string& s = peek().lexeme;
            if (s.empty()) return false;
            if (!isalpha(static_cast<unsigned char>(s[0])) && s[0] != '_') return false;
            // keywords
            static const std::unordered_set<std::string> kws = {
                "int","bool","char","string","void",
                "true","false","if","else","while","return",
                "class","public","virtual"
            };
            return kws.find(s) == kws.end();
        }
        return false;
    }

    std::string take_ident(const char* msg) {
        if (!peek_is_ident()) throw err_here(msg);
        std::string s = peek().lexeme;
        ++i_;
        return s;
    }

    ParseError err_here(const std::string& msg) const {
        // If token has line/col use it; otherwise just message.
        const auto& t = peek();
        std::string where;
        if constexpr (requires { t.line; t.col; }) {
            where = " at " + std::to_string(t.line) + ":" + std::to_string(t.col);
        }
        return ParseError("ParseError" + where + ": " + msg);
    }

    // ---------- types ----------
    ast::Type parse_type() {
        bool is_ref = false;

        ast::Type t;
        if (match_lex("int")) t = ast::Type::Int(false);
        else if (match_lex("bool")) t = ast::Type::Bool(false);
        else if (match_lex("char")) t = ast::Type::Char(false);
        else if (match_lex("string")) t = ast::Type::String(false);
        else if (match_lex("void")) t = ast::Type::Void(false);
        else if (peek_is_ident() || (isalpha((unsigned char)peek().lexeme[0]) || peek().lexeme[0]=='_')) {
            // class type name
            std::string cn = take_ident("expected type name");
            t = ast::Type::Class(std::move(cn), false);
        } else {
            throw err_here("expected type");
        }

        if (match_lex("&")) {
            is_ref = true;
        }

        // set ref flag
        t.is_ref = is_ref;
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

        // optional ": public Base"
        if (match_lex(":")) {
            if (match_lex("public")) {
                c.base_name = take_ident("expected base class name after 'public'");
            } else {
                throw err_here("expected 'public' after ':'");
            }
        } else {
            c.base_name = "";
        }

        expect_lex("{", "expected '{' in class body");

        // optional "public:"
        if (match_lex("public")) {
            expect_lex(":", "expected ':' after 'public'");
        }

        while (!match_lex("}")) {
            if (is_end()) throw err_here("unexpected end in class body");

            bool is_virtual = false;
            if (match_lex("virtual")) is_virtual = true;

            // constructor: ClassName ( params ) block
            if (peek().lexeme == c.name && peek_is_ident()) {
                std::string ctor_name = take_ident("expected ctor name");
                (void)ctor_name;
                expect_lex("(", "expected '(' after constructor name");
                ast::ConstructorDef cd;
                cd.params = parse_param_list();
                cd.body = parse_block_stmt();
                c.ctors.push_back(std::move(cd));
                continue;
            }

            // field or method: Type name ...
            ast::Type t = parse_type();
            std::string member_name = take_ident("expected member name");

            if (match_lex("(")) {
                // method
                ast::MethodDef m;
                m.is_virtual = is_virtual;
                m.return_type = t;
                m.name = std::move(member_name);
                m.params = parse_param_list();
                m.body = parse_block_stmt();
                c.methods.push_back(std::move(m));
            } else {
                // field
                if (is_virtual) throw err_here("virtual not allowed on fields");
                ast::FieldDecl fd;
                fd.type = t;
                fd.name = std::move(member_name);
                expect_lex(";", "expected ';' after field");
                c.fields.push_back(std::move(fd));
            }
        }

        expect_lex(";", "expected ';' after class definition");
        return c;
    }

    // ---------- statements ----------
    ast::StmtPtr parse_stmt() {
        if (match_lex("{")) {
            --i_; // put back, block parser consumes '{'
            return parse_block_stmt();
        }
        if (match_lex("if")) return parse_if_stmt();
        if (match_lex("while")) return parse_while_stmt();
        if (match_lex("return")) return parse_return_stmt();

        // var decl starts with type keyword or class name
        if (peek_is_type_start()) {
            // lookahead: type + ident ... if next after type is ident and then ';' or '=' => vardecl
            // In this language, expression cannot start with a type keyword, so this is safe.
            return parse_vardecl_stmt();
        }

        // expr stmt
        auto s = std::make_unique<ast::ExprStmt>();
        s->expr = parse_expr();
        expect_lex(";", "expected ';' after expression");
        return s;
    }

    bool peek_is_type_start() const {
        const std::string& lx = peek().lexeme;
        if (lx == "int" || lx == "bool" || lx == "char" || lx == "string" || lx == "void") return true;
        // class type (identifier) is ambiguous, but vardecl syntax needs: Type ident ...
        // We'll treat identifier as type-start here only if the next token is also identifier or '&'.
        if (peek_is_ident()) {
            // If we see "X & y" or "X y"
            if (peek(1).lexeme == "&") return true;
            // next should be identifier name
            if (i_ + 1 < tokens_.size()) {
                // if next token looks like identifier and not '('
                if (tokens_[i_ + 1].lexeme != "(") return true;
            }
        }
        return false;
    }

    ast::StmtPtr parse_block_stmt() {
        auto b = std::make_unique<ast::BlockStmt>();
        expect_lex("{", "expected '{' to start block");
        while (!match_lex("}")) {
            if (is_end()) throw err_here("unexpected end in block");
            b->statements.push_back(parse_stmt());
        }
        return b;
    }

    ast::StmtPtr parse_if_stmt() {
        auto s = std::make_unique<ast::IfStmt>();
        expect_lex("(", "expected '(' after if");
        s->cond = parse_expr();
        expect_lex(")", "expected ')' after if condition");
        s->then_branch = parse_stmt();
        if (match_lex("else")) {
            s->else_branch = parse_stmt();
        } else {
            s->else_branch = nullptr;
        }
        return s;
    }

    ast::StmtPtr parse_while_stmt() {
        auto s = std::make_unique<ast::WhileStmt>();
        expect_lex("(", "expected '(' after while");
        s->cond = parse_expr();
        expect_lex(")", "expected ')' after while condition");
        s->body = parse_stmt();
        return s;
    }

    ast::StmtPtr parse_return_stmt() {
        auto s = std::make_unique<ast::ReturnStmt>();
        // allow "return;" (void)
        if (!match_lex(";")) {
            s->value = parse_expr();
            expect_lex(";", "expected ';' after return");
        } else {
            s->value = nullptr;
        }
        return s;
    }

    ast::StmtPtr parse_vardecl_stmt() {
        auto s = std::make_unique<ast::VarDeclStmt>();
        s->type = parse_type();
        s->name = take_ident("expected variable name");
        if (match_lex("=")) {
            s->init = parse_expr();
        } else {
            s->init = nullptr;
        }
        expect_lex(";", "expected ';' after variable declaration");
        return s;
    }

    // ---------- expressions (precedence climbing) ----------
    ast::ExprPtr parse_expr() { return parse_assignment(); }

    ast::ExprPtr parse_assignment() {
        auto lhs = parse_logical_or();
        if (match_lex("=")) {
            auto rhs = parse_assignment(); // right-assoc
            // lhs must be lvalue: VarExpr or MemberAccessExpr
            if (auto* v = dynamic_cast<ast::VarExpr*>(lhs.get())) {
                auto a = std::make_unique<ast::AssignExpr>();
                a->name = v->name;
                a->value = std::move(rhs);
                return a;
            }
            if (auto* m = dynamic_cast<ast::MemberAccessExpr*>(lhs.get())) {
                auto fa = std::make_unique<ast::FieldAssignExpr>();
                fa->object = std::move(m->object);
                fa->field = m->field;
                fa->value = std::move(rhs);
                return fa;
            }
            throw err_here("left side of '=' must be a variable or field access");
        }
        return lhs;
    }

    ast::ExprPtr parse_logical_or() {
        auto e = parse_logical_and();
        while (match_lex("||")) {
            auto b = std::make_unique<ast::BinaryExpr>();
            b->op = "||";
            b->lhs = std::move(e);
            b->rhs = parse_logical_and();
            e = std::move(b);
        }
        return e;
    }

    ast::ExprPtr parse_logical_and() {
        auto e = parse_equality();
        while (match_lex("&&")) {
            auto b = std::make_unique<ast::BinaryExpr>();
            b->op = "&&";
            b->lhs = std::move(e);
            b->rhs = parse_equality();
            e = std::move(b);
        }
        return e;
    }

    ast::ExprPtr parse_equality() {
        auto e = parse_relational();
        while (true) {
            if (match_lex("==")) {
                auto b = std::make_unique<ast::BinaryExpr>();
                b->op = "==";
                b->lhs = std::move(e);
                b->rhs = parse_relational();
                e = std::move(b);
            } else if (match_lex("!=")) {
                auto b = std::make_unique<ast::BinaryExpr>();
                b->op = "!=";
                b->lhs = std::move(e);
                b->rhs = parse_relational();
                e = std::move(b);
            } else break;
        }
        return e;
    }

    ast::ExprPtr parse_relational() {
        auto e = parse_additive();
        while (true) {
            if (match_lex("<") || match_lex("<=") || match_lex(">") || match_lex(">=")) {
                std::string op = tokens_[i_-1].lexeme;
                auto b = std::make_unique<ast::BinaryExpr>();
                b->op = std::move(op);
                b->lhs = std::move(e);
                b->rhs = parse_additive();
                e = std::move(b);
            } else break;
        }
        return e;
    }

    ast::ExprPtr parse_additive() {
        auto e = parse_multiplicative();
        while (true) {
            if (match_lex("+") || match_lex("-")) {
                std::string op = tokens_[i_-1].lexeme;
                auto b = std::make_unique<ast::BinaryExpr>();
                b->op = std::move(op);
                b->lhs = std::move(e);
                b->rhs = parse_multiplicative();
                e = std::move(b);
            } else break;
        }
        return e;
    }

    ast::ExprPtr parse_multiplicative() {
        auto e = parse_unary();
        while (true) {
            if (match_lex("*") || match_lex("/") || match_lex("%")) {
                std::string op = tokens_[i_-1].lexeme;
                auto b = std::make_unique<ast::BinaryExpr>();
                b->op = std::move(op);
                b->lhs = std::move(e);
                b->rhs = parse_unary();
                e = std::move(b);
            } else break;
        }
        return e;
    }

    ast::ExprPtr parse_unary() {
        if (match_lex("!")) {
            auto u = std::make_unique<ast::UnaryExpr>();
            u->op = "!";
            u->rhs = parse_unary();
            return u;
        }
        if (match_lex("+")) {
            auto u = std::make_unique<ast::UnaryExpr>();
            u->op = "+";
            u->rhs = parse_unary();
            return u;
        }
        if (match_lex("-")) {
            auto u = std::make_unique<ast::UnaryExpr>();
            u->op = "-";
            u->rhs = parse_unary();
            return u;
        }
        return parse_postfix();
    }

    ast::ExprPtr parse_postfix() {
        auto e = parse_primary();

        for (;;) {
            // member access / method call
            if (match_lex(".")) {
                std::string name = take_ident("expected member name after '.'");
                if (match_lex("(")) {
                    auto mc = std::make_unique<ast::MethodCallExpr>();
                    mc->object = std::move(e);
                    mc->method = std::move(name);
                    mc->args = parse_arg_list();
                    e = std::move(mc);
                } else {
                    auto ma = std::make_unique<ast::MemberAccessExpr>();
                    ma->object = std::move(e);
                    ma->field = std::move(name);
                    e = std::move(ma);
                }
                continue;
            }

            // function call only allowed on VarExpr in this subset
            if (match_lex("(")) {
                auto* v = dynamic_cast<ast::VarExpr*>(e.get());
                if (!v) throw err_here("call target must be an identifier");
                std::string callee = v->name;

                // build args
                auto args = parse_arg_list();

                if (class_names_.find(callee) != class_names_.end()) {
                    auto ce = std::make_unique<ast::ConstructExpr>();
                    ce->class_name = std::move(callee);
                    ce->args = std::move(args);
                    e = std::move(ce);
                } else {
                    auto c = std::make_unique<ast::CallExpr>();
                    c->callee = std::move(callee);
                    c->args = std::move(args);
                    e = std::move(c);
                }
                continue;
            }

            break;
        }

        return e;
    }

    std::vector<ast::ExprPtr> parse_arg_list() {
        std::vector<ast::ExprPtr> args;
        if (match_lex(")")) return args;
        for (;;) {
            args.push_back(parse_expr());
            if (match_lex(")")) break;
            expect_lex(",", "expected ',' or ')'");
        }
        return args;
    }

    ast::ExprPtr parse_primary() {
        // literals
        if (peek().lexeme == "true" || peek().lexeme == "false") {
            auto b = std::make_unique<ast::BoolLiteral>(peek().lexeme == "true");
            ++i_;
            return b;
        }

        // int literal
        if (is_int_literal(peek())) {
            int v = std::stoi(peek().lexeme);
            ++i_;
            return std::make_unique<ast::IntLiteral>(v);
        }

        // char literal: lexer should already decode escapes into lexeme like 'a' or keep raw "'a'"
        if (is_char_literal(peek())) {
            char c = decode_char(peek().lexeme);
            ++i_;
            return std::make_unique<ast::CharLiteral>(c);
        }

        // string literal
        if (is_string_literal(peek())) {
            std::string s = decode_string(peek().lexeme);
            ++i_;
            auto sl = std::make_unique<ast::StringLiteral>();
            sl->value = std::move(s);
            return sl;
        }

        if (match_lex("(")) {
            auto e = parse_expr();
            expect_lex(")", "expected ')'");
            return e;
        }

        // identifier => VarExpr
        if (peek_is_ident()) {
            auto v = std::make_unique<ast::VarExpr>();
            v->name = take_ident("expected identifier");
            return v;
        }

        throw err_here("expected expression");
    }

    static bool is_int_literal(const lexer::Token& t) {
        if constexpr (requires { lexer::TokenKind::IntLiteral; }) {
            if (t.kind == lexer::TokenKind::IntLiteral) return true;
        }
        const std::string& s = t.lexeme;
        if (s.empty()) return false;
        for (char ch : s) if (!isdigit((unsigned char)ch)) return false;
        return true;
    }

    static bool is_char_literal(const lexer::Token& t) {
        if constexpr (requires { lexer::TokenKind::CharLiteral; }) {
            if (t.kind == lexer::TokenKind::CharLiteral) return true;
        }
        const std::string& s = t.lexeme;
        return s.size() >= 3 && s.front() == '\'' && s.back() == '\'';
    }

    static bool is_string_literal(const lexer::Token& t) {
        if constexpr (requires { lexer::TokenKind::StringLiteral; }) {
            if (t.kind == lexer::TokenKind::StringLiteral) return true;
        }
        const std::string& s = t.lexeme;
        return s.size() >= 2 && s.front() == '"' && s.back() == '"';
    }

    static char decode_char(const std::string& raw) {
        // raw like: 'a' or '\n' or '\0'
        if (raw.size() < 3) return '\0';
        if (raw[1] != '\\') return raw[1];
        if (raw.size() < 4) return '\0';
        switch (raw[2]) {
            case 'n': return '\n';
            case 't': return '\t';
            case 'r': return '\r';
            case '0': return '\0';
            case '\\': return '\\';
            case '\'': return '\'';
            case '"': return '"';
            default: return raw[2];
        }
    }

    static std::string decode_string(const std::string& raw) {
        // raw like: "foo\nbar"
        if (raw.size() < 2) return "";
        std::string out;
        for (size_t k = 1; k + 1 < raw.size(); ++k) {
            char ch = raw[k];
            if (ch == '\\' && k + 1 < raw.size()) {
                char n = raw[k + 1];
                switch (n) {
                    case 'n': out.push_back('\n'); break;
                    case 't': out.push_back('\t'); break;
                    case 'r': out.push_back('\r'); break;
                    case '0': out.push_back('\0'); break;
                    case '\\': out.push_back('\\'); break;
                    case '"': out.push_back('"'); break;
                    default: out.push_back(n); break;
                }
                ++k;
            } else {
                out.push_back(ch);
            }
        }
        return out;
    }
};

} // namespace parser
