#pragma once
#include <memory>
#include <string>
#include <vector>

namespace node {
    class ExprAST {
        public:
            virtual ~ExprAST() = default;
    };




    // AST for integers
    class IntExprAST: public ExprAST {
        int val;
        
        public:
            IntExprAST(int val) : val(val) {}
    };


    // AST for variable names
    class VariableExprAST: public ExprAST {
        std::string Name;

        public:
            VariableExprAST (std::string Name) : Name(Name) {}
    };

    // AST for binary expression like +, -, / , *
    //In future will include relational operators as they are binary too
    class BinaryExprAST: public ExprAST {
        char Op;
        std::unique_ptr<ExprAST> LHS, RHS;

        public:
            BinaryExprAST (char Op, std::unique_ptr<ExprAST> LHS, std::unique_ptr<ExprAST> RHS) 
            : Op(Op), LHS (std::move(LHS)), RHS (std::move(RHS)) {}

    };


    // Following three classes are just for function

    // This class is for function calls
    class FuncCallExprAST: public ExprAST {
        std::string Callee;
        std::vector<std::unique_ptr<ExprAST>> Args;

        public:
            FuncCallExprAST (const std::string &Callee, std::vector<std::unique_ptr<ExprAST>> Args)
            : Callee(Callee), Args(std::move(Args)) {}
    };

    // This class is for prototype of a function. So it only takes names, parameters and their number.
    class FuncPrototypeAST {
        std::string Name;
        std::vector<std::string> Args;

        public:
            FuncPrototypeAST (const std::string &Name, std::vector<std::string> Args) 
            : Name(Name), Args(std::move(Args)) {}

            const std::string &getName() const { return Name; }
    };

    // This class represents a function itself
    class FunctionAST {
        std::unique_ptr<FuncPrototypeAST> Proto;
        std::unique_ptr<ExprAST> Body;

        public:
            FunctionAST (std::unique_ptr<FuncPrototypeAST> Proto, std::unique_ptr<ExprAST> Body)
            : Proto(std::move(Proto)), Body(std::move(Body)) {}
    };
}