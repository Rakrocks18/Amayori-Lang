#pragma once

#include <vector>
#include <functional>
#include <algorithm>
#include <utility>

#include "../amyr-utils/vec.hpp"

// Uses a sorted slice `data: &[E]` as a kind of "multi-map". The
// `key_fn` extracts a key of type `K` from the data, and this
// function finds the range of elements that match the key. `data`
// must have been sorted as if by a call to `sort_by_key` for this to
// work.
template <typename E, typename K, typename KeyFn>
std::pair<const E*, const E*> binary_search_slice(const Vec<E>& data, KeyFn&& key_fn, const K& key) {
    const E* begin = data.data();
    const E* end = begin + data.size();

    // Find first element with key >= target
    const E* lower = std::lower_bound(begin, end, key,
        [&](const E& elem, const K& k) { return key_fn(elem) < k; });

    // Check if element actually exists
    if (lower == end || key_fn(*lower) != key) {
        return {end, end}; // Empty range
    }

    // Find first element with key > target
    const E* upper = std::upper_bound(lower, end, key,
        [&](const K& k, const E& elem) { return k < key_fn(elem); });

    return {lower, upper};
}