#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Verifier.h"
#include "AmayoriAST.hpp"

using namespace llvm;

class IRGenerator {
private:
    std::unique_ptr<LLVMContext> TheContext;
    std::unique_ptr<Module> TheModule;
    std::unique_ptr<IRBuilder<>> Builder;

public:
    IRGenerator() : TheContext(std::make_unique<LLVMContext>()),
                    TheModule(std::make_unique<Module>("MyLLVMModule", *TheContext)),
                    Builder(std::make_unique<IRBuilder<>>(*TheContext)) {}

    Value* generateIR(node::ExprAST* Expr) {
    if (auto* IntExpr = dynamic_cast<node::IntExprAST*>(Expr)) {
        return ConstantInt::get(*TheContext, APInt(32, IntExpr->getVal(), true));
    }
    if (auto* BinaryExpr = dynamic_cast<node::BinaryExprAST*>(Expr)) {
        Value* L = generateIR(BinaryExpr->getLHS());
        Value* R = generateIR(BinaryExpr->getRHS());
        if (!L || !R) return nullptr;

        switch (BinaryExpr->getOp()) {
            case '+': return Builder->CreateAdd(L, R, "addtmp");
            case '-': return Builder->CreateSub(L, R, "subtmp");
            case '*': return Builder->CreateMul(L, R, "multmp");
            case '/': return Builder->CreateSDiv(L, R, "divtmp");
            default: return nullptr;
        }
    }
    // Handle other expression types as needed
    return nullptr;
}

    Function* generateFunctionIR(node::FunctionAST* FnAST) {
        std::vector<Type*> Ints(FnAST->getProto()->getArgs().size(), Type::getInt32Ty(*TheContext));
        FunctionType* FT = FunctionType::get(Type::getInt32Ty(*TheContext), Ints, false);
        Function* F = Function::Create(FT, Function::ExternalLinkage, FnAST->getProto()->getName(), TheModule.get());

        BasicBlock* BB = BasicBlock::Create(*TheContext, "entry", F);
        Builder->SetInsertPoint(BB);

        // Generate IR for function body
        if (Value* RetVal = generateIR(FnAST->getBody())) {
            Builder->CreateRet(RetVal);
            verifyFunction(*F);
            return F;
        }

        F->eraseFromParent();
        return nullptr;
    }

    void dumpModule() {
        TheModule->print(errs(), nullptr);
    }
};

#ifndef TESTING
int main() {
    std::cout << "Created a new LLVM module named: MyLLVMModule" << std::endl;

    IRGenerator IRGen;

    // Create AST nodes for the integers 10 and 20
    auto LHS = std::make_unique<node::IntExprAST>(10);
    auto RHS = std::make_unique<node::IntExprAST>(20);

    // Create a binary expression AST node for addition
    auto BinaryExpr = std::make_unique<node::BinaryExprAST>('+', std::move(LHS), std::move(RHS));

    // Create a function prototype (no arguments in this case)
    std::vector<std::string> Args = {"int a"};
    auto Proto = std::make_unique<node::FuncPrototypeAST>("add_example", std::move(Args));

    // Create the function AST node
    auto FnAST = std::make_unique<node::FunctionAST>(std::move(Proto), std::move(BinaryExpr));

    // Generate IR for the function
    IRGen.generateFunctionIR(FnAST.get()); 

    std::cout << "Generated LLVM IR:" << std::endl;
    IRGen.dumpModule();

    return 0;
}
#endif