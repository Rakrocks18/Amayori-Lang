// src/amyr-borrow-check/BorrowChecker.cpp
#pragma once  // Prevent multiple inclusion

#include <string>
#include <vector>
#include <map>
#include "../parser.hpp"  // Include needed headers instead of self-including

namespace amyr {
namespace borrow {

enum class BorrowKind {
    Shared,
    Mutable,
    Move
};

enum class ViolationType {
    BorrowWhileMutable,
    UseAfterMove,
    InvalidBorrow
};

struct Violation {
    ViolationType type;
    std::string message;
    int line;
    
    Violation(ViolationType t, std::string msg, int ln) 
        : type(t), message(std::move(msg)), line(ln) {}
};

struct OwnershipData {
    bool is_mutable;
    std::vector<std::string> borrowers;
    int scope_level;
    bool moved;
};

class OwnershipTracker {
public:
    void exit_scope();
    bool register_variable(const std::string& name, bool is_mut);
    bool can_borrow(const std::string& name, BorrowKind kind) const;
    bool register_borrow(const std::string& var, 
                         const std::string& borrower, 
                         BorrowKind kind);
    bool mark_moved(const std::string& name);

private:
    std::map<std::string, OwnershipData> ownership_map;
    int current_scope = 0;
};

class BorrowChecker {
public:
    bool check(const node::ExprAST* ast);
    void check_expr(const node::ExprAST* expr);
    void check_binary(const node::BinaryExprAST* expr);
    void check_variable(const node::VariableExprAST* expr);
    const std::vector<Violation>& get_errors() const { return errors; }

private:
    void clear_errors() { errors.clear(); }
    std::vector<Violation> errors;
    OwnershipTracker tracker;
};

} // namespace borrow
} // namespace amyr