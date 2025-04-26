#pragma once

#include "./AmayoriAST.hpp"
#include "./amyr-tokenizer/tokenizer.hpp"
#include "./amyr-borrow-check/BorrowChecker.hpp"
#include "./amyr-parser/arena.hpp"
#include "./amyr-utils/result.hpp"

#include <string_view>
#include <vector>
#include <stdexcept>
#include <unordered_set>
#include <memory> // For shared_ptr

namespace node {

class ExprAST {
public:
    virtual ~ExprAST() = default;
};

class IntExprAST : public ExprAST {
    int value;
public:
    explicit IntExprAST(int val) : value(val) {}
    int getValue() const { return value; }
};

class BinaryExprAST : public ExprAST {
    ExprAST* LHS;
    ExprAST* RHS;
    char op;
public:
    BinaryExprAST(ExprAST* lhs, ExprAST* rhs, char op) 
        : LHS(lhs), RHS(rhs), op(op) {}
    ExprAST* getLHS() const { return LHS; }
    ExprAST* getRHS() const { return RHS; }
    char getOp() const { return op; }
};

class VariableExprAST : public ExprAST {
    std::string_view name;
    amyr::borrow::BorrowKind borrow_type;
public:
    explicit VariableExprAST(std::string_view n) : name(n) {}
    const std::string_view& getName() const { return name; }
    void setBorrowType(amyr::borrow::BorrowKind type) { borrow_type = type; }
};

class LetExprAST : public ExprAST {
    std::string_view name;
    bool is_mutable;
    ExprAST* init;
public:
    LetExprAST(std::string_view n, bool is_mut, ExprAST* init)
        : name(n), is_mutable(is_mut), init(init) {}
    const std::string_view& getName() const { return name; }
    bool isMutable() const { return is_mutable; }
    const ExprAST* getInitExpr() const { return init; }
};

class BlockExprAST : public ExprAST {
    std::vector<std::shared_ptr<ExprAST>> expressions;
public:
    explicit BlockExprAST(std::vector<std::shared_ptr<ExprAST>> exprs)
        : expressions(std::move(exprs)) {}
};

class Parser {
private:
    std::vector<Token> tokens;
    size_t current = 0;
    amyr::borrow::BorrowChecker borrow_checker;
    std::unordered_set<std::string_view> declared_variables;
    int scope_depth = 0;

    TypedArena<ExprAST> arena; // Arena allocator for AST nodes

    Token peek() const {
        return tokens[current];
    }

    Token previous() const {
        return tokens[current - 1];
    }

    bool isAtEnd() const {
        return peek().type == TokenType::EOF_TOKEN;
    }

    Token advance() {
        if (!isAtEnd()) current++;
        return previous();
    }

    bool check_type(TokenType type) const {
        if (isAtEnd()) return false;
        return peek().type == type;
    }

    bool match(TokenType type) {
        if (check_type(type)) {
            advance();
            return true;
        }
        return false;
    }

    void enter_scope() {
        scope_depth++;
    }

    void exit_scope() {
        scope_depth--;
    }

    std::shared_ptr<ExprAST> parse_primary() {
        if (match(TokenType::Integer)) {
            int value = std::stoi(previous().lexeme);
            return std::make_shared<IntExprAST>(value);
        }

        if (match(TokenType::Identifier)) {
            std::string_view var_name = previous().lexeme;
            if (declared_variables.find(var_name) == declared_variables.end()) {
                throw std::runtime_error("Use of undeclared variable: " + std::string(var_name));
            }
            auto expr = std::make_shared<VariableExprAST>(var_name);
            expr->setBorrowType(amyr::borrow::BorrowKind::Shared); // Default to shared borrow
            return expr;
        }

        if (match(TokenType::LeftParen)) {
            auto expr = parse_expression();
            if (!match(TokenType::RightParen)) {
                throw std::runtime_error("Expect ')' after expression.");
            }
            return expr;
        }

        if (match(TokenType::Let)) {
            // Handle variable declaration
            if (!match(TokenType::Identifier)) {
                throw std::runtime_error("Expect identifier after 'let'.");
            }
            std::string_view var_name = previous().lexeme;

            bool is_mutable = false;
            if (match(TokenType::Mut)) {
                is_mutable = true;
            }

            if (!match(TokenType::Equals)) {
                throw std::runtime_error("Expect '=' after variable name.");
            }

            auto init_expr = parse_expression();
            declared_variables.insert(var_name);
            return std::make_shared<LetExprAST>(var_name, is_mutable, init_expr.get());
        }

        throw std::runtime_error("Expect expression.");
    }

    std::shared_ptr<ExprAST> parse_term() {
        auto expr = parse_primary();

        while (match(TokenType::Star) || match(TokenType::Slash)) {
            char op = previous().lexeme[0];
            auto right = parse_primary();
            expr = std::make_shared<BinaryExprAST>(op, expr.get(), right.get());
        }

        return expr;
    }

    std::shared_ptr<ExprAST> parse_expression() {
        auto expr = parse_term();

        while (match(TokenType::Plus) || match(TokenType::Minus)) {
            char op = previous().lexeme[0];
            auto right = parse_term();
            expr = std::make_shared<BinaryExprAST>(op, expr.get(), right.get());
        }

        return expr;
    }

    void check_borrow_violations(const ExprAST* ast) {
        if (!borrow_checker.check(ast)) {
            const auto& errors = borrow_checker.get_errors();
            if (!errors.empty()) {
                throw std::runtime_error(errors[0].message); // Throw first error
            }
        }
    }

public:
    explicit Parser(std::vector<Token> tokens) : tokens(std::move(tokens)) {}

    std::shared_ptr<ExprAST> parse() {
        try {
            auto ast = parse_expression();
            check_borrow_violations(ast.get());
            return ast;
        } catch (const std::runtime_error& e) {
            throw std::runtime_error("Line " + std::to_string(peek().line) + ": " + e.what());
        }
    }

    std::shared_ptr<ExprAST> parse_block() {
        enter_scope();
        std::vector<std::shared_ptr<ExprAST>> expressions;

        while (!isAtEnd() && !check_type(TokenType::RightBrace)) {
            auto expr = parse();
            expressions.push_back(expr);
            match(TokenType::Semicolon); // Optional semicolon
        }

        if (!match(TokenType::RightBrace)) {
            throw std::runtime_error("Expect '}' after block.");
        }

        exit_scope();
        return std::make_shared<BlockExprAST>(std::move(expressions));
    }
};

} // namespace node

// Add TokenType enum if not already defined
enum class TokenType {
    Let,
    Mut,
    Identifier,
    Integer,
    Equals,
    RightBrace,
    Plus,
    Minus,
    Star,
    Slash,
    LeftParen,
    RightParen,
    Semicolon,
    EOF_TOKEN
};