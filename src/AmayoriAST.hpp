#pragma once
#include <memory>
#include <string>
#include <vector>
#include <stdexcept>
#include <optional>

namespace node {
    // Forward declarations
    class ASTVisitor;

    // Borrow checking types
    enum class BorrowKind {
        None,
        Shared,    // &
        Mutable,   // &mut
        Move       // ownership transfer
    };

    struct BorrowInfo {
        BorrowKind kind = BorrowKind::None;
        bool is_mutable = false;
        std::string scope_id;
    };

    // Base Expression AST with Visitor support and borrow checking
    class ExprAST {
    protected:
        BorrowInfo borrow_info;
        bool has_error = false;
        std::string error_message;

    public:
        virtual ~ExprAST() = default;
        
        // Visitor pattern support
        virtual void accept(ASTVisitor* visitor) = 0;
        
        // Error handling
        virtual bool hasError() const { return has_error; }
        virtual std::string getErrorMessage() const { return error_message; }

        // Borrow checking support
        void setBorrowKind(BorrowKind kind) { borrow_info.kind = kind; }
        void setMutable(bool is_mut) { borrow_info.is_mutable = is_mut; }
        void setScopeId(const std::string& scope) { borrow_info.scope_id = scope; }
        
        BorrowKind getBorrowKind() const { return borrow_info.kind; }
        bool isMutable() const { return borrow_info.is_mutable; }
        const std::string& getScopeId() const { return borrow_info.scope_id; }
    };

    // Integer Expression
    class IntExprAST : public ExprAST {
    private:
        int val;
        
    public:
        explicit IntExprAST(int val) : val(val) {
            setBorrowKind(BorrowKind::None); // Literals don't need borrowing
        }
        
        int getValue() const { return val; }
        void accept(ASTVisitor* visitor) override;
    };

    // Variable Expression 
    class VariableExprAST : public ExprAST {
    private:
        std::string name;
    
    public:
        explicit VariableExprAST(std::string name) : name(std::move(name)) {
            setBorrowKind(BorrowKind::Shared); // Default to shared borrow
        }
        
        const std::string& getName() const { return name; }
        void accept(ASTVisitor* visitor) override;
    };

    // Let Expression for variable declarations
    class LetExprAST : public ExprAST {
    private:
        std::string name;
        std::unique_ptr<ExprAST> init_expr;
    
    public:
        LetExprAST(std::string name, bool is_mut, std::unique_ptr<ExprAST> init)
            : name(std::move(name)), init_expr(std::move(init)) {
            setMutable(is_mut);
            setBorrowKind(BorrowKind::None);
        }
        
        const std::string& getName() const { return name; }
        const ExprAST& getInitExpr() const { return *init_expr; }
        void accept(ASTVisitor* visitor) override;
    };

    // Binary Operation Expression
    class BinaryExprAST : public ExprAST {
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
        {
            setBorrowKind(BorrowKind::None);
        }
        
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
        {
            setBorrowKind(BorrowKind::None);
        }
        
        const std::string& getCallee() const { return callee; }
        const std::vector<std::unique_ptr<ExprAST>>& getArgs() const { return args; }
        
        void accept(ASTVisitor* visitor) override;
    };

    // Function Prototype with borrow checking information
    class FuncPrototypeAST {
    private:
        std::string name;
        std::vector<std::string> args;
        std::vector<BorrowInfo> arg_borrow_info;  // Borrow info for each argument
    
    public:
        FuncPrototypeAST(
            std::string name, 
            std::vector<std::string> args,
            std::vector<BorrowInfo> arg_borrows = {}
        ) : 
            name(std::move(name)), 
            args(std::move(args)),
            arg_borrow_info(std::move(arg_borrows))
        {
            if (arg_borrow_info.empty()) {
                arg_borrow_info.resize(this->args.size(), BorrowInfo{});
            }
        }
        
        const std::string& getName() const { return name; }
        const std::vector<std::string>& getArgs() const { return args; }
        const std::vector<BorrowInfo>& getArgBorrowInfo() const { return arg_borrow_info; }
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
        virtual void visitLetExpr(LetExprAST* node) = 0;
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

    inline void LetExprAST::accept(ASTVisitor* visitor) { 
        visitor->visitLetExpr(this); 
    }

    inline void BinaryExprAST::accept(ASTVisitor* visitor) { 
        visitor->visitBinaryExpr(this); 
    }

    inline void FuncCallExprAST::accept(ASTVisitor* visitor) { 
        visitor->visitFuncCallExpr(this); 
    }
}