#pragma once

#include <cstddef>
#include <memory>
#include <utility>

class ArenaAllocator {
public:
    explicit ArenaAllocator(const size_t max_num_bytes) {
        : m_size { max_num_bytes }
    }
};
