// src/parser.hpp
#pragma once

// Update include paths to be relative to src directory
#include "./AmayoriAST.hpp"
#include "./amyr-tokenizer/tokenizer.hpp"
#include "./amyr-borrow-check/BorrowChecker.hpp"

#include <memory>
#include <string>
#include <vector>
#include <stdexcept>
#include <unordered_set>

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
    std::string name;
    amyr::borrow::BorrowKind borrow_type;
public:
    explicit VariableExprAST(std::string n) : name(std::move(n)) {}
    const std::string& getName() const { return name; }
    void setBorrowType(amyr::borrow::BorrowKind type) { borrow_type = type; }
};

class LetExprAST : public ExprAST {
    std::string name;
    std::unique_ptr<ExprAST> init;
public:
    LetExprAST(std::string n, std::unique_ptr<ExprAST> init)
        : name(std::move(n)), init(std::move(init)) {}
    const std::string& getName() const { return name; }
    const ExprAST& getInitExpr() const { return *init; }
};

class BlockExprAST : public ExprAST {
    std::vector<std::unique_ptr<ExprAST>> expressions;
public:
    explicit BlockExprAST(std::vector<std::unique_ptr<ExprAST>> exprs)
        : expressions(std::move(exprs)) {}
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
    // ... other token types
};

class Parser {
private:
    std::vector<Token> tokens;
    int current = 0;
    amyr::borrow::BorrowChecker borrow_checker;
    std::unordered_set<std::string> declared_variables;
    int scope_depth = 0;

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

    std::unique_ptr<node::ExprAST> primary() {
        if (match(TokenType::Integer)) {
            return std::make_unique<node::IntExprAST>(std::stoi(previous().lexeme));
        }
        
        if (match(TokenType::Identifier)) {
            std::string var_name = previous().lexeme;
            if (declared_variables.find(var_name) == declared_variables.end()) {
                throw std::runtime_error("Use of undeclared variable: " + var_name);
            }
            auto expr = std::make_unique<node::VariableExprAST>(var_name);
            expr->borrow_type = amyr::borrow::BorrowKind::Shared;  // Default to shared borrow
            return expr;
        }

        if (match(TokenType::LeftParen)) {
            auto expr = expression();
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
            std::string var_name = previous().lexeme;
            
            bool is_mutable = false;
            if (match(TokenType::Mut)) {
                is_mutable = true;
            }

            if (!match(TokenType::Equals)) {
                throw std::runtime_error("Expect '=' after variable name.");
            }

            auto init_expr = expression();
            declared_variables.insert(var_name);
            
            return std::make_unique<node::LetExprAST>(
                var_name,
                is_mutable,
                std::move(init_expr)
            );
        }

        throw std::runtime_error("Expect expression.");
    }

    std::unique_ptr<node::ExprAST> term() {
        auto expr = primary();
        
        while (match(TokenType::Star) || match(TokenType::Slash)) {
            Token op = previous();
            auto right = primary();
            
            expr = std::make_unique<node::BinaryExprAST>(
                op.lexeme[0], 
                std::move(expr), 
                std::move(right)
            );
        }
        
        return expr;
    }

    std::unique_ptr<node::ExprAST> expression() {
        auto expr = term();
        
        while (match(TokenType::Plus) || match(TokenType::Minus)) {
            Token op = previous();
            auto right = term();
            
            expr = std::make_unique<node::BinaryExprAST>(
                op.lexeme[0], 
                std::move(expr), 
                std::move(right)
            );
        }
        
        return expr;
    }

    void check_borrow_violations(const node::ExprAST* ast) {
        if (!borrow_checker.check(ast)) {
            const auto& errors = borrow_checker.get_errors();
            if (!errors.empty()) {
                throw std::runtime_error(errors[0].message);  // Throw first error
            }
        }
    }

public:
    explicit Parser(std::vector<Token> tokens) : tokens(std::move(tokens)) {}

    std::unique_ptr<node::ExprAST> parse() {
        try {
            auto ast = expression();
            check_borrow_violations(ast.get());
            return ast;
        } catch (const std::runtime_error& e) {
            // Add line number information to error
            throw std::runtime_error("Line " + 
                std::to_string(peek().line) + ": " + e.what());
        }
    }

    // Add methods for parsing additional constructs
    std::unique_ptr<node::ExprAST> parse_block() {
        enter_scope();
        std::vector<std::unique_ptr<node::ExprAST>> expressions;
        
        while (!isAtEnd() && !check_type(TokenType::RightBrace)) {
            expressions.push_back(parse());
            match(TokenType::Semicolon);  // Optional semicolon
        }
        
        if (!match(TokenType::RightBrace)) {
            throw std::runtime_error("Expect '}' after block.");
        }
        
        exit_scope();
        return std::make_unique<node::BlockExprAST>(std::move(expressions));
    }
};