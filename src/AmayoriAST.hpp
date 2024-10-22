//
// Created by vivek on 10-10-2024.
//
#pragma once
#include <memory>
#include <string>
#include <vector>

namespace node {
    class ExprAST {
    public:
        virtual ~ExprAST() = default;
    };

    class IntExprAST : public ExprAST {
        int val;
        
    public:
        IntExprAST(int val) : val(val) {}
        int getVal() const { return val; }
    };

    class VariableExprAST : public ExprAST {
        std::string Name;

    public:
        VariableExprAST(std::string Name) : Name(Name) {}
        const std::string& getName() const { return Name; }
    };

    class BinaryExprAST : public ExprAST {
        char Op;
        std::unique_ptr<ExprAST> LHS, RHS;

    public:
        BinaryExprAST(char Op, std::unique_ptr<ExprAST> LHS, std::unique_ptr<ExprAST> RHS) 
            : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}
        char getOp() const { return Op; }
        ExprAST* getLHS() const { return LHS.get(); }
        ExprAST* getRHS() const { return RHS.get(); }
    };

    class FuncCallExprAST : public ExprAST {  //call(int a, int b)
        std::string Callee;
        std::vector<std::unique_ptr<ExprAST>> Args;

    public:
        FuncCallExprAST(const std::string &Callee, std::vector<std::unique_ptr<ExprAST>> Args)
            : Callee(Callee), Args(std::move(Args)) {}
        const std::string& getCallee() const { return Callee; }
        const std::vector<std::unique_ptr<ExprAST>>& getArgs() const { return Args; }
    };

    class FuncPrototypeAST {
        std::string Name;
        std::vector<std::string> Args;

    public:
        FuncPrototypeAST(const std::string &Name, std::vector<std::string> Args) 
            : Name(Name), Args(std::move(Args)) {}
        const std::string& getName() const { return Name; }
        const std::vector<std::string>& getArgs() const { return Args; }
    };

    class FunctionAST {
        std::unique_ptr<FuncPrototypeAST> Proto;
        std::unique_ptr<ExprAST> Body;

    public:
        FunctionAST(std::unique_ptr<FuncPrototypeAST> Proto, std::unique_ptr<ExprAST> Body)
            : Proto(std::move(Proto)), Body(std::move(Body)) {}
        FuncPrototypeAST* getProto() const { return Proto.get(); }
        ExprAST* getBody() const { return Body.get(); }
    };
}