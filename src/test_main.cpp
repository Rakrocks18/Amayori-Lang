#include <gtest/gtest.h>
#include "amyr-tokenizer/tokenizer.hpp"
#include "parser.hpp"
#include "amyr-borrow-check/BorrowChecker.hpp"
#include "amayori-llvm.hpp"

// Tokenizer Tests
class TokenizerTest : public ::testing::Test {
protected:
    std::vector<Token> tokenize(const std::string& source) {
        Tokenizer tokenizer(source);
        return tokenizer.tokenize();
    }
};

TEST_F(TokenizerTest, BasicTokenization) {
    auto tokens = tokenize("let x = 42;");
    
    ASSERT_EQ(tokens.size(), 5); // Including EOF
    EXPECT_EQ(tokens[0].type, TokenType::Let);
    EXPECT_EQ(tokens[1].type, TokenType::Identifier);
    EXPECT_EQ(tokens[1].lexeme, "x");
    EXPECT_EQ(tokens[2].type, TokenType::Equals);
    EXPECT_EQ(tokens[3].type, TokenType::Integer);
    EXPECT_EQ(tokens[3].lexeme, "42");
}

// Parser Tests
class ParserTest : public ::testing::Test {
protected:
    std::unique_ptr<node::ExprAST> parse(const std::string& source) {
        Tokenizer tokenizer(source);
        auto tokens = tokenizer.tokenize();
        Parser parser(std::move(tokens));
        return parser.parse();
    }
};

TEST_F(ParserTest, BasicParsing) {
    auto ast = parse("let x = 42;");
    ASSERT_NE(ast, nullptr);
    
    auto* let_expr = dynamic_cast<node::LetExprAST*>(ast.get());
    ASSERT_NE(let_expr, nullptr);
    EXPECT_EQ(let_expr->getName(), "x");
    
    auto* init_expr = dynamic_cast<node::IntExprAST*>(&let_expr->getInitExpr());
    ASSERT_NE(init_expr, nullptr);
    EXPECT_EQ(init_expr->getValue(), 42);
}

// Borrow Checker Tests
class BorrowCheckerTest : public ::testing::Test {
protected:
    amyr::borrow::BorrowChecker checker;
    
    std::unique_ptr<node::ExprAST> parse(const std::string& source) {
        Tokenizer tokenizer(source);
        auto tokens = tokenizer.tokenize();
        Parser parser(std::move(tokens));
        return parser.parse();
    }
    
    bool checkCode(const std::string& source) {
        auto ast = parse(source);
        return checker.check(ast.get());
    }
};

TEST_F(BorrowCheckerTest, BasicBorrowChecking) {
    EXPECT_TRUE(checkCode("let x = 42;"));
    EXPECT_FALSE(checkCode("let x = y;")); // undefined variable
    
    // Test mutable borrow
    EXPECT_TRUE(checkCode("let mut x = 42;"));
    
    // Test multiple borrows
    const char* code = R"(
        let mut x = 42;
        let y = &x;  // OK: shared borrow
        let z = &mut x;  // Error: cannot have mutable borrow while shared
    )";
    EXPECT_FALSE(checkCode(code));
}

// LLVM IR Generation Tests
class IRGeneratorTest : public ::testing::Test {
protected:
    void verifyIR(const std::string& source) {
        auto ast = parse(source);
        IRGenerator generator;
        auto* value = generator.generateIR(ast.get());
        ASSERT_NE(value, nullptr);
    }
};

TEST_F(IRGeneratorTest, BasicIRGeneration) {
    verifyIR("42");
    verifyIR("1 + 2");
    verifyIR("let x = 42;");
}

// Integration Tests
TEST(IntegrationTest, CompleteCompilation) {
    const char* source = R"(
        let x = 40;
        let y = 2;
        x + y
    )";
    
    // Test full compilation pipeline
    Tokenizer tokenizer(source);
    auto tokens = tokenizer.tokenize();
    ASSERT_FALSE(tokens.empty());
    
    Parser parser(std::move(tokens));
    auto ast = parser.parse();
    ASSERT_NE(ast, nullptr);
    
    amyr::borrow::BorrowChecker checker;
    EXPECT_TRUE(checker.check(ast.get()));
    
    IRGenerator generator;
    auto* ir = generator.generateIR(ast.get());
    ASSERT_NE(ir, nullptr);
}

// Error Handling Tests
TEST(ErrorHandlingTest, TokenizerErrors) {
    Tokenizer tokenizer("@"); // Invalid character
    EXPECT_THROW(tokenizer.tokenize(), std::runtime_error);
}

TEST(ErrorHandlingTest, ParserErrors) {
    const char* invalid_code = "let;"; // Missing identifier
    Tokenizer tokenizer(invalid_code);
    auto tokens = tokenizer.tokenize();
    Parser parser(std::move(tokens));
    EXPECT_THROW(parser.parse(), std::runtime_error);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}