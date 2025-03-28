#pragma once
#include<optional>
#include<functional>
#include<utility>

// Base iterator using CRTP.
// The Derived type must implement:
//    std::optional<T> next();
template<typename Derived, typename T>
class Iterator {
public:
    using Item = T;

    //Returns next element
    std::optional<T> next() {
        //Call derived's implementation
        return static_cast<Derived*>(this) -> next();
    }

    // Map: transforms each element using the provided function.
    // Returns a new iterator that applies the function.
    // Lazy map: returns an iterator that applies 'func' to each element on-the-fly.
    template<typename Func>
    auto map(Func func) {
        //The type of the mapped items
        using U = decltype(func(std::declval<T>()));

        // Define a nested iterator type that wraps the original.
        struct MapIterator: Iterator<MapIterator, U> {
            Derived* base;
            Func func;

            MapIterator (Derived* base, Func func) 
                :base(base), func(func) {}
            
            
            
            // Lazily apply 'func' to the next value of the underlying iterator.
            std::optional<U> next() {
                if(auto val= base->next())
                    return func(*val);
                return std::nullopt;
            }
        };

        return MapIterator(static_cast<Derived*>(this), func);
    }

    // Lazy filter: returns an iterator that only yields elements satisfying 'pred'.
    template<typename Pred>
    auto filter(Pred pred) {
        // Local type for the lazy filtered iterator.
        struct FilterIterator : Iterator<FilterIterator, T> {
            Derived* base; // underlying iterator
            Pred pred;     // predicate to test each element

            FilterIterator(Derived* base, Pred pred)
                : base(base), pred(pred) {}

            // Lazily skip values until one satisfies the predicate.
            std::optional<T> next() {
                while (auto val = base->next()) {
                    if (pred(*val))
                        return val;
                }
                return std::nullopt;
            }
        };
        return FilterIterator(static_cast<Derived*>(this), pred);
    }

    template<typename Pred>
    bool all(Pred pred) {
        struct AllIterator: Iterator<AllIterator, U> {
            Derived* base;
            Pred pred;

            AllIterator(Derived* base, Pred pred)
                : base(base), pred(pred)

            //Lazily apply a predicate to all members
            bool next() {
                while (auto val = base->next()) {
                    if(!pred(*val)) {
                        return false;
                    }
                }
                return true;
            }
        }
    }
};