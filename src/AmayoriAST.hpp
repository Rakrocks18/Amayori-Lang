#pragma once
#include <memory>
#include <string_view>
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

        int getVal() const { return val; }

        void accept(ASTVisitor* visitor) override;
    };

    // Variable Expression
    class VariableExprAST : public ExprAST {
    private:
        std::string_view name;

// Replacing std::string with std::string_view for identifiers, function names, and other string-based data to avoid unnecessary memory allocations and copying.
    public:
        explicit VariableExprAST(std::string_view name) : name(name) {
            setBorrowKind(BorrowKind::Shared); // Default to shared borrow
        }

        const std::string_view& getName() const { return name; }
        void accept(ASTVisitor* visitor) override;
    };

    // Let Expression for variable declarations
    class LetExprAST : public ExprAST {
    private:
        std::string_view name;
        bool is_mutable;
        std::shared_ptr<ExprAST> init_expr;

    public:
        LetExprAST(std::string_view name, bool is_mut, ExprAST* init)
            : name(name), is_mutable(is_mut), init_expr(init) {
            setMutable(is_mut);
            setBorrowKind(BorrowKind::None);
        }

        const std::string_view& getName() const { return name; }
        bool isMutable() const { return is_mutable; }
        const std::shared_ptr<ExprAST>& getInitExpr() const { return init_expr; }
        void accept(ASTVisitor* visitor) override;
    };

    // Binary Operation Expression
    class BinaryExprAST : public ExprAST {
    private:
        char op;
        std::shared_ptr<ExprAST> lhs;
        std::shared_ptr<ExprAST> rhs;

    public:
        BinaryExprAST(char op, ExprAST* lhs, ExprAST* rhs)
            : op(op), lhs(lhs), rhs(rhs) {
            setBorrowKind(BorrowKind::None);
        }

        char getOp() const { return op; }
        const std::shared_ptr<ExprAST>& getLHS() const { return lhs; }
        const std::shared_ptr<ExprAST>& getRHS() const { return rhs; }

        void accept(ASTVisitor* visitor) override;
    };

    // Block Expression for scopes
    class BlockExprAST : public ExprAST {
    private:
        std::vector<std::shared_ptr<ExprAST>> expressions;

    public:
        explicit BlockExprAST(std::vector<std::shared_ptr<ExprAST>> exprs)
            : expressions(std::move(exprs)) {}

        const std::vector<std::shared_ptr<ExprAST>>& getExpressions() const { return expressions; }

        void accept(ASTVisitor* visitor) override;
    };

    // Function Call Expression
    class FuncCallExprAST : public ExprAST {
    private:
        std::string_view callee;
        std::vector<ExprAST*> args;

    public:
        FuncCallExprAST(std::string_view callee, std::vector<ExprAST*> args)
            : callee(callee), args(std::move(args)) {
            setBorrowKind(BorrowKind::None);
        }

        const std::string_view& getCallee() const { return callee; }
        const std::vector<ExprAST*>& getArgs() const { return args; }

        void accept(ASTVisitor* visitor) override;
    };

    // Function Prototype AST
    class FuncPrototypeAST {
    private:
        std::string_view name;
        std::vector<std::string> args;

    public:
        FuncPrototypeAST(std::string_view name, std::vector<std::string> args)
            : name(name), args(std::move(args)) {}

        const std::string_view& getName() const { return name; }
        const std::vector<std::string>& getArgs() const { return args; }
    };

    // Function AST
    class FunctionAST : public ExprAST {
    private:
        std::unique_ptr<FuncPrototypeAST> proto;
        std::unique_ptr<ExprAST> body;

    public:
        FunctionAST(std::unique_ptr<FuncPrototypeAST> proto, std::unique_ptr<ExprAST> body)
            : proto(std::move(proto)), body(std::move(body)) {}

        const FuncPrototypeAST* getProto() const { return proto.get(); }
        const ExprAST& getBody() const { return *body; }

        void accept(ASTVisitor* visitor) override;
    };

    // Abstract Visitor for AST Traversal
    class ASTVisitor {
    public:
        virtual void visitIntExpr(IntExprAST* node) = 0;
        virtual void visitVariableExpr(VariableExprAST* node) = 0;
        virtual void visitLetExpr(LetExprAST* node) = 0;
        virtual void visitBinaryExpr(BinaryExprAST* node) = 0;
        virtual void visitBlockExpr(BlockExprAST* node) = 0;
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

    inline void BlockExprAST::accept(ASTVisitor* visitor) {
        visitor->visitBlockExpr(this);
    }

    inline void FuncCallExprAST::accept(ASTVisitor* visitor) {
        visitor->visitFuncCallExpr(this);
    }

    inline void FunctionAST::accept(ASTVisitor* visitor) {
        // Implement visitor logic for FunctionAST
    }
}