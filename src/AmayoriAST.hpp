//
// Created by vivek on 10-10-2024.
//
#pragma once
#include <memory>
#include <string>
#include <vector>
#include <stdexcept>
#include <optional>

namespace node {
    // Forward declarations
    class ASTVisitor;

    // Base Expression AST with Visitor support
    class ExprAST {
    public:
        virtual ~ExprAST() = default;
        
        // Visitor pattern support
        virtual void accept(ASTVisitor* visitor) = 0;
        
        // Error handling
        virtual bool hasError() const { return false; }
        virtual std::string getErrorMessage() const { return ""; }
    };

    // Integer Expression
    class IntExprAST : public ExprAST {
    private:
        int val;
        
    public:
        explicit IntExprAST(int val) : val(val) {}
        
        int getValue() const { return val; }
        
        void accept(ASTVisitor* visitor) override;
    };

    // Variable Expression 
    class VariableExprAST : public ExprAST {
    private:
        std::string name;
    
    public:
        explicit VariableExprAST(std::string name) : name(std::move(name)) {}
        
        const std::string& getName() const { return name; }
        
        void accept(ASTVisitor* visitor) override;
    };

    // Binary Operation Expression
    class BinaryExprAST : public ExprAST {  //Previously it was returning raw pointers, which could lead to ownership confusion, therefore added a unique pointer for managing memory
    private:
        char op;
        std::unique_ptr<ExprAST> lhs;
        std::unique_ptr<ExprAST> rhs;
    
    public:
        BinaryExprAST(
            char op, 
            std::unique_ptr<ExprAST> lhs, 
            std::unique_ptr<ExprAST> rhs
        ) : 
            op(op), 
            lhs(std::move(lhs)), 
            rhs(std::move(rhs)) 
        {}
        
        char getOperator() const { return op; }
        const ExprAST& getLeftHandSide() const { return *lhs; }
        const ExprAST& getRightHandSide() const { return *rhs; }
        
        void accept(ASTVisitor* visitor) override;
    };

    // Function Call Expression
    class FuncCallExprAST : public ExprAST {
    private:
        std::string callee;
        std::vector<std::unique_ptr<ExprAST>> args;
    
    public:
        FuncCallExprAST(
            std::string callee, 
            std::vector<std::unique_ptr<ExprAST>> args
        ) : 
            callee(std::move(callee)), 
            args(std::move(args)) 
        {}
        
        const std::string& getCallee() const { return callee; }
        const std::vector<std::unique_ptr<ExprAST>>& getArgs() const { return args; }
        
        void accept(ASTVisitor* visitor) override;
    };

    // Function Prototype
    class FuncPrototypeAST {
    private:
        std::string name;
        std::vector<std::string> args;
    
    public:
        FuncPrototypeAST(
            std::string name, 
            std::vector<std::string> args
        ) : 
            name(std::move(name)), 
            args(std::move(args)) 
        {}
        
        const std::string& getName() const { return name; }
        const std::vector<std::string>& getArgs() const { return args; }
    };

    // Function AST
    class FunctionAST {
    private:
        std::unique_ptr<FuncPrototypeAST> prototype;
        std::unique_ptr<ExprAST> body;
    
    public:
        FunctionAST(
            std::unique_ptr<FuncPrototypeAST> prototype, 
            std::unique_ptr<ExprAST> body
        ) : 
            prototype(std::move(prototype)), 
            body(std::move(body)) 
        {}
        
        const FuncPrototypeAST& getPrototype() const { return *prototype; }
        const ExprAST& getBody() const { return *body; }
    };

    // Abstract Visitor for AST Traversal
    class ASTVisitor {
    public:
        virtual void visitIntExpr(IntExprAST* node) = 0;
        virtual void visitVariableExpr(VariableExprAST* node) = 0;
        virtual void visitBinaryExpr(BinaryExprAST* node) = 0;
        virtual void visitFuncCallExpr(FuncCallExprAST* node) = 0;
        virtual ~ASTVisitor() = default;
    };

    // Visitor Method Implementations
    inline void IntExprAST::accept(ASTVisitor* visitor) { 
        visitor->visitIntExpr(this); 
    }

    inline void VariableExprAST::accept(ASTVisitor* visitor) { 
        visitor->visitVariableExpr(this); 
    }

    inline void BinaryExprAST::accept(ASTVisitor* visitor) { 
        visitor->visitBinaryExpr(this); 
    }

    inline void FuncCallExprAST::accept(ASTVisitor* visitor) { 
        visitor->visitFuncCallExpr(this); 
    }
}
