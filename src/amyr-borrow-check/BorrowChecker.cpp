#include "BorrowChecker.hpp"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <stdexcept>
#include <cassert>

namespace amyr {
namespace borrow {

// TwoPhaseActivation enum
enum class TwoPhaseActivation {
    NotTwoPhase,
    NotActivated,
    ActivatedAt
};

// BorrowData structure
struct BorrowData {
    Location reserve_location;
    TwoPhaseActivation activation_location;
    BorrowKind kind;
    std::string region;
    std::string borrowed_place;
    std::string assigned_place;

    BorrowData(Location reserve, TwoPhaseActivation activation, BorrowKind kind,
               std::string region, std::string borrowed, std::string assigned)
        : reserve_location(reserve), activation_location(activation), kind(kind),
          region(std::move(region)), borrowed_place(std::move(borrowed)),
          assigned_place(std::move(assigned)) {}
};

// BorrowSet class
class BorrowSet {
public:
    std::unordered_map<Location, BorrowData> location_map;
    std::unordered_map<Location, std::vector<int>> activation_map;
    std::unordered_map<std::string, std::unordered_set<int>> local_map;

    void add_borrow(Location location, BorrowData borrow) {
        location_map[location] = std::move(borrow);
    }

    void add_activation(Location location, int borrow_index) {
        activation_map[location].push_back(borrow_index);
    }

    void add_local_borrow(const std::string& local, int borrow_index) {
        local_map[local].insert(borrow_index);
    }

    const BorrowData& get_borrow(int index) const {
        auto it = location_map.begin();
        std::advance(it, index);
        return it->second;
    }

    size_t size() const {
        return location_map.size();
    }
};

// BorrowChecker class
class BorrowChecker {
private:
    BorrowSet borrow_set;
    std::vector<Violation> errors;

    void check_borrow(const BorrowData& borrow) {
        if (borrow.kind == BorrowKind::Mutable && borrow.activation_location == TwoPhaseActivation::ActivatedAt) {
            errors.emplace_back(ViolationType::BorrowWhileMutable,
                                "Cannot mutably borrow '" + borrow.borrowed_place + "' while it is already borrowed",
                                borrow.reserve_location.line);
        }
    }

public:
    bool check(const node::ExprAST* ast) {
        errors.clear();
        check_expr(ast);
        return errors.empty();
    }

    void check_expr(const node::ExprAST* expr) {
        if (auto* binary = dynamic_cast<const node::BinaryExprAST*>(expr)) {
            check_expr(binary->getLHS());
            check_expr(binary->getRHS());
        } else if (auto* var = dynamic_cast<const node::VariableExprAST*>(expr)) {
            check_variable(var);
        }
    }

    void check_variable(const node::VariableExprAST* var) {
        auto it = borrow_set.local_map.find(var->getName());
        if (it != borrow_set.local_map.end()) {
            for (int borrow_index : it->second) {
                const BorrowData& borrow = borrow_set.get_borrow(borrow_index);
                check_borrow(borrow);
            }
        }
    }

    const std::vector<Violation>& get_errors() const {
        return errors;
    }
};

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

} // namespace borrow
} // namespace amyr