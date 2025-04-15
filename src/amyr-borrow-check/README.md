# Amayori Borrow Checker

The borrow checker is a core component of the Amayori programming language that ensures memory safety without garbage collection by enforcing strict ownership rules at compile time.

## Overview

Amayori's borrow checker implements an ownership system inspired by Rust, preventing common memory-related bugs such as:

- Use-after-free
- Double-free
- Data races
- Iterator invalidation
- Dangling pointers

The system tracks ownership of values throughout the program and validates references based on three fundamental principles:

1. Each value has a single owner at any given time
2. Values can be borrowed immutably (shared) by multiple references
3. Values can be borrowed mutably by a single reference when no shared references exist

## Key Features

### Ownership Tracking

- Tracks variable ownership through scopes
- Automatically drops values when they go out of scope
- Prevents use after move

### Borrowing Rules

- **Shared Borrows**: Multiple immutable references allowed
- **Mutable Borrows**: Single mutable reference allowed, no concurrent immutable references
- **Move Semantics**: Transfers ownership between variables

### Two-Phase Borrowing

The borrow checker supports two-phase borrowing, where a borrow is first reserved and then activated when needed, improving ergonomics for complex patterns.

## Implementation Details

### Core Components

#### BorrowKind
```cpp
enum class BorrowKind {
    Shared,   // Immutable borrow (&T)
    Mutable,  // Mutable borrow (&mut T)
    Move      // Move ownership
};
```

#### Violation Types
```cpp
enum class ViolationType {
    BorrowWhileMutable,  // Borrowing while a mutable borrow exists
    UseAfterMove,        // Using a value after it's been moved
    InvalidBorrow        // Invalid borrow (e.g., borrowing uninitialized data)
};
```

#### BorrowChecker
The main class responsible for validating borrows and detecting violations.

#### OwnershipTracker
Monitors variable ownership through different scopes and tracks borrows.

## Usage Example

```amayori
fn example() {
    let mut x = 5;        // x is mutable and owned
    let y = &mut x;       // y is a mutable borrow of x
    
    // Error: Cannot borrow x while it's mutably borrowed
    // let z = &x;
    
    *y = 10;              // Modify x through y
    
    let z = &x;           // Now we can create a shared borrow
    
    // Error: Cannot mutably borrow x while it's immutably borrowed
    // let w = &mut x;
    
    println!("{}", z);    // Use the shared borrow
    
    let a = move x;       // Move ownership of x to a
    
    // Error: Cannot use x after move
    // println!("{}", x);
}
```

## Error Reporting

The borrow checker produces clear error messages to help developers identify and fix ownership issues:

- "Cannot mutably borrow '{variable}' while it is already borrowed"
- "Cannot use '{variable}' after it has been moved"
- "Cannot borrow '{variable}' which may be uninitialized"

## Integration with LLVM

The borrow checker performs its analysis during compilation before code generation:

1. AST traversal to identify variables, borrows, and scopes
2. Ownership and borrowing validation
3. Error collection and reporting
4. Code generation only if all borrow checks pass

## Future Improvements

- Non-lexical lifetimes for more flexible borrow checking
- Enhanced pattern matching support
- Better support for complex data structures
- More detailed error messages with suggestions
- Integration with IDE tooling for real-time feedback

## Contributing

Contributions to the borrow checker are welcome! Please see our contributing guidelines for more information.

## License
MIT license
