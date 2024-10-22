//
// Created by vivek on 10-10-2024.
//
#pragma once
#include <memory>
#include <stdexcept>
#include "AmayoriAST.hpp"
#include "tokenizer.hpp"


class Parser {
private:
    std::vector<Token> tokens;
    int current = 0;

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

    std::unique_ptr<node::ExprAST> primary() {
        if (match(TokenType::Integer)) {
            return std::make_unique<node::IntExprAST>(std::stoi(previous().lexeme));
        }

        if (match(TokenType::LeftParen)) {
            auto expr = expression();  //7 + 8 * 9
            if (!match(TokenType::RightParen)) {
                throw std::runtime_error("Expect ')' after expression.");
            }
            return expr;
        }

        throw std::runtime_error("Expect expression.");
    }

    std::unique_ptr<node::ExprAST> term() { // *7 x 9
        auto expr = primary(); //7 *x 9

        while (match(TokenType::Star) || match(TokenType::Slash)) { //7 x *9
            Token op = previous(); //op =  token(7), 7 x *9
            auto right = primary(); // 7 x 9
            expr = std::make_unique<node::BinaryExprAST>(op.lexeme[0], std::move(expr), std::move(right));
        }

        return expr;
    }

    std::unique_ptr<node::ExprAST> expression() {
        auto expr = term(); //tok

        while (match(TokenType::Plus) || match(TokenType::Minus)) {
            Token op = previous();
            auto right = term();
            expr = std::make_unique<node::BinaryExprAST>(op.lexeme[0], std::move(expr), std::move(right));
        }

        return expr;
    }

public:
    explicit Parser(std::vector<Token> tokens) : tokens(std::move(tokens)) {}

    std::unique_ptr<node::ExprAST> parse() {
        return expression();
    }
};