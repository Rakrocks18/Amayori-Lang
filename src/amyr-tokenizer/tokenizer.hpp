//
// Created by vivek on 10-10-2024.
//
//
// Created by vivek on 10-10-2024.
//
#pragma once
#include <string>
#include <vector>
#include <cctype>
#include <stdexcept>
#include <unordered_map>  //Automatically identifies language keywords

enum class TokenType {
    // Keywords
    Let,
    Mut,
    Func,
    Return,
    If,
    Else,
    
    // Literals
    Identifier,
    Integer,
    Float,
    
    // Operators and delimiters
    Equals,
    RightBrace,
    LeftBrace,
    LeftParen,
    RightParen,
    Plus,
    Minus,
    Star,
    Slash,
    Semicolon,
    EOF_TOKEN
};

struct Token {
    TokenType type;
    std::string lexeme;
    int line;
    
    // Optional value for numeric tokens
    union {
        int intValue;
        double floatValue;  //Can now tokenize decimal numbers
    } value;

    Token(TokenType type, std::string lexeme, int line) 
        : type(type), lexeme(std::move(lexeme)), line(line) {}
};

class Tokenizer {
private:
    std::string source;
    std::vector<Token> tokens;
    int start = 0;
    int current = 0;
    int line = 1;

    // Keyword map for quick lookup
    static const std::unordered_map<std::string, TokenType> keywords;

    bool isAtEnd() const {
        return current >= source.length();
    }

    char advance() {
        return source[current++];
    }

    void addToken(TokenType type) {
        std::string text = source.substr(start, current - start);
        tokens.emplace_back(type, text, line);
    }

    void addNumericToken(TokenType type) {
        std::string text = source.substr(start, current - start);
        Token token(type, text, line);
        
        if (type == TokenType::Integer) {
            token.value.intValue = std::stoi(text);
        } else if (type == TokenType::Float) {
            token.value.floatValue = std::stod(text);
        }
        
        tokens.push_back(std::move(token));
    }

    char peek() const {
        if (isAtEnd()) return '\0';
        return source[current];
    }

    char peekNext() const {
        if (current + 1 >= source.length()) return '\0';
        return source[current + 1];
    }

    void number() {
        // Handle integer part
        while (std::isdigit(peek())) advance();

        // Handle floating point
        if (peek() == '.' && std::isdigit(peekNext())) {
            // Consume the dot
            advance();

            // Consume fractional part
            while (std::isdigit(peek())) advance();
            
            addNumericToken(TokenType::Float);
        } else {
            addNumericToken(TokenType::Integer);
        }
    }

    void identifier() {
        while (std::isalnum(peek()) || peek() == '_') advance();
        
        std::string text = source.substr(start, current - start);
        
        // Check if it's a keyword
        auto it = keywords.find(text);
        TokenType type = (it != keywords.end()) ? it->second : TokenType::Identifier;
        
        addToken(type);
    }

    void skipComment() {
        // Single-line comment
        while (peek() != '\n' && !isAtEnd()) advance();
    }

public:
    explicit Tokenizer(std::string source) : source(std::move(source)) {}

    std::vector<Token> tokenize() {
        while (!isAtEnd()) {
            start = current;
            char c = advance();
            switch (c) {
                // Delimiters and operators
                case '(': addToken(TokenType::LeftParen); break;
                case ')': addToken(TokenType::RightParen); break;
                case '+': addToken(TokenType::Plus); break;
                case '-': addToken(TokenType::Minus); break;
                case '*': addToken(TokenType::Star); break;
                case '/': 
                    if (peek() == '/') {
                        skipComment();
                    } else {
                        addToken(TokenType::Slash); 
                    }
                    break;
                case '=': addToken(TokenType::Equals); break;
                case ';': addToken(TokenType::Semicolon); break;

                // Whitespace handling
                case ' ':
                case '\r':
                case '\t':
                    break;
                case '\n':
                    line++;
                    break;

                // Numeric and identifier handling
                default:
                    if (std::isdigit(c)) {
                        number();
                    } else if (std::isalpha(c) || c == '_') {
                        identifier();
                    } else {
                        throw std::runtime_error(
                            "Unexpected character '" + std::string(1, c) + 
                            "' at line " + std::to_string(line)
                        );
                    }
                    break;
            }
        }
        tokens.emplace_back(TokenType::EOF_TOKEN, "", line);
        return tokens;
    }
};

// Static keyword initialization
const std::unordered_map<std::string, TokenType> Tokenizer::keywords = {
    {"let", TokenType::Let},
    {"mut", TokenType::Mut},
    {"func", TokenType::Func},
    {"return", TokenType::Return},
    {"if", TokenType::If},
    {"else", TokenType::Else}
};
