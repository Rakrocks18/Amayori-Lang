// src/amyr-borrow-check/BorrowChecker.cpp
#pragma once  // Prevent multiple inclusion

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
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

struct Location {
    int line;
    int column;

    Location(int l, int c) : line(l), column(c) {}

    bool operator==(const Location& other) const {
        return line == other.line && column == other.column;
    }
};

struct LocationHash {
    std::size_t operator()(const Location& loc) const {
        return std::hash<int>()(loc.line) ^ std::hash<int>()(loc.column);
    }
};

class BorrowChecker {
public:
    bool check(const node::ExprAST* ast);
    const std::vector<Violation>& get_errors() const;

private:
    void check_expr(const node::ExprAST* expr);
    void check_variable(const node::VariableExprAST* var);
};

} // namespace borrow
} // namespace amyr