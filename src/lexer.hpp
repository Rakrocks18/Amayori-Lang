//
// Created by vivek on 10-10-2024.
//
#include <string>
#include <vector>
#include <cctype>
#include <stdexcept>

enum class TokenType {
    Integer,
    Plus,
    Minus,
    Star,
    Slash,
    LeftParen,
    RightParen,
    Identifier,
    Equals,
    Semicolon,
    EOF_TOKEN
};

struct Token {
    TokenType type;
    std::string lexeme;
    int line;

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

    char peek() const {
        if (isAtEnd()) return '\0';
        return source[current];
    }

    void number() {
        while (std::isdigit(peek())) advance();
        addToken(TokenType::Integer);
    }

    void identifier() {
        while (std::isalnum(peek()) || peek() == '_') advance();
        addToken(TokenType::Identifier);
    }

public:
    explicit Tokenizer(std::string source) : source(std::move(source)) {}

    std::vector<Token> tokenize() {
        while (!isAtEnd()) {
            start = current;
            char c = advance();
            switch (c) {
                case '(': addToken(TokenType::LeftParen); break;
                case ')': addToken(TokenType::RightParen); break;
                case '+': addToken(TokenType::Plus); break;
                case '-': addToken(TokenType::Minus); break;
                case '*': addToken(TokenType::Star); break;
                case '/': addToken(TokenType::Slash); break;
                case '=': addToken(TokenType::Equals); break;
                case ';': addToken(TokenType::Semicolon); break;
                case ' ':
                case '\r':
                case '\t':
                    // Ignore whitespace
                    break;
                case '\n':
                    line++;
                    break;
                default:
                    if (std::isdigit(c)) {
                        number();
                    } else if (std::isalpha(c) || c == '_') {
                        identifier();
                    } else {
                        throw std::runtime_error("Unexpected character at line " + std::to_string(line));
                    }
                    break;
            }
        }

        tokens.emplace_back(TokenType::EOF_TOKEN, "", line);
        return tokens;
    }
};