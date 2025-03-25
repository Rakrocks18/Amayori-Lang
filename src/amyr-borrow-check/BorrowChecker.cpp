#include "BorrowChecker.hpp"

namespace amyr {
namespace borrow {

void OwnershipTracker::exit_scope() {
    for (auto it = ownership_map.begin(); it != ownership_map.end();) {
        if (it->second.scope_level == current_scope) {
            it = ownership_map.erase(it);
        } else {
            ++it;
        }
    }
    --current_scope;
}

bool OwnershipTracker::register_variable(const std::string& name, bool is_mut) {
    if (ownership_map.find(name) != ownership_map.end()) {
        return false;
    }
    
    ownership_map[name] = {
        .is_mutable = is_mut,
        .borrowers = {},
        .scope_level = current_scope,
        .moved = false
    };
    return true;
}

bool OwnershipTracker::can_borrow(const std::string& name, BorrowKind kind) const {
    auto it = ownership_map.find(name);
    if (it == ownership_map.end() || it->second.moved) {
        return false;
    }
    
    const auto& data = it->second;
    switch (kind) {
        case BorrowKind::Shared:
            return !data.moved && data.borrowers.empty();
        case BorrowKind::Mutable:
            return !data.moved && data.borrowers.empty() && data.is_mutable;
        case BorrowKind::Move:
            return !data.moved && data.borrowers.empty();
        default:
            return false;
    }
}

bool OwnershipTracker::register_borrow(const std::string& var, 
                                     const std::string& borrower, 
                                     BorrowKind kind) {
    auto it = ownership_map.find(var);
    if (it == ownership_map.end() || !can_borrow(var, kind)) {
        return false;
    }
    
    it->second.borrowers.push_back(borrower);
    return true;
}

bool OwnershipTracker::mark_moved(const std::string& name) {
    auto it = ownership_map.find(name);
    if (it == ownership_map.end() || it->second.moved || !it->second.borrowers.empty()) {
        return false;
    }
    
    it->second.moved = true;
    return true;
}

bool BorrowChecker::check(const node::ExprAST* ast) {
    clear_errors();
    check_expr(ast);
    return errors.empty();
}

void BorrowChecker::check_expr(const node::ExprAST* expr) {
    if (auto* binary = dynamic_cast<const node::BinaryExprAST*>(expr)) {
        check_binary(binary);
    } else if (auto* var = dynamic_cast<const node::VariableExprAST*>(expr)) {
        check_variable(var);
    }
}

void BorrowChecker::check_binary(const node::BinaryExprAST* expr) {
    check_expr(expr->getLHS());
    check_expr(expr->getRHS());
}

void BorrowChecker::check_variable(const node::VariableExprAST* expr) {
    if (!tracker.can_borrow(expr->getName(), BorrowKind::Shared)) {
        errors.emplace_back(
            ViolationType::BorrowWhileMutable,
            "Cannot borrow variable '" + expr->getName() + "' - already mutably borrowed",
            0  // Line number not implemented yet
        );
    }
}

} // namespace borrow
} // namespace amyr 