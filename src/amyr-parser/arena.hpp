#pragma once

#include <vector>
#include <memory>
#include <cstdlib>
#include <cassert>
#include <type_traits>
#include <new>
#include <cstring>

// ArenaChunk: Represents a single chunk of memory in the arena.
template <typename T>
class ArenaChunk {
public:
    explicit ArenaChunk(size_t capacity)
        : capacity_(capacity), size_(0) {
        data_ = static_cast<T*>(std::aligned_alloc(alignof(T), capacity_ * sizeof(T)));
        if (!data_) {
            throw std::bad_alloc();
        }
    }

    ~ArenaChunk() {
        clear();
        std::free(data_);
    }

    T* allocate() {
        assert(size_ < capacity_ && "Chunk capacity exceeded!");
        return &data_[size_++];
    }

    bool has_space() const {
        return size_ < capacity_;
    }

    void clear() {
        if constexpr (!std::is_trivially_destructible_v<T>) {
            for (size_t i = 0; i < size_; ++i) {
                data_[i].~T();
            }
        }
        size_ = 0;
    }

private:
    T* data_;
    size_t capacity_;
    size_t size_;
};

// TypedArena: Allocator for objects of a single type.
template <typename T>
class TypedArena {
public:
    TypedArena() = default;

    ~TypedArena() {
        for (auto& chunk : chunks_) {
            chunk->clear();
        }
    }

    template <typename... Args>
    T* allocate(Args&&... args) {
        if (chunks_.empty() || !chunks_.back()->has_space()) {
            grow();
        }
        T* obj = chunks_.back()->allocate();
        new (obj) T(std::forward<Args>(args)...); // Placement new
        return obj;
    }

    void clear() {
        for (auto& chunk : chunks_) {
            chunk->clear();
        }
    }

private:
    static constexpr size_t INITIAL_CAPACITY = 1024; // Initial chunk size
    static constexpr size_t GROWTH_FACTOR = 2;       // Growth factor for chunk sizes

    std::vector<std::unique_ptr<ArenaChunk<T>>> chunks_;

    void grow() {
        size_t capacity = chunks_.empty() ? INITIAL_CAPACITY : chunks_.back()->capacity() * GROWTH_FACTOR;
        chunks_.emplace_back(std::make_unique<ArenaChunk<T>>(capacity));
    }
};

// DroplessArena: Allocator for objects of multiple types (no destructors).
class DroplessArena {
public:
    DroplessArena() = default;

    ~DroplessArena() {
        for (auto& chunk : chunks_) {
            std::free(chunk.data);
        }
    }

    void* allocate(size_t size, size_t alignment) {
        if (chunks_.empty() || !has_space(size, alignment)) {
            grow(size, alignment);
        }
        void* ptr = chunks_.back().current;
        chunks_.back().current = static_cast<char*>(chunks_.back().current) + size;
        return ptr;
    }

    void clear() {
        for (auto& chunk : chunks_) {
            chunk.current = chunk.start;
        }
    }

private:
    struct Chunk {
        void* start;
        void* current;
        void* end;

        Chunk(size_t size) {
            start = std::aligned_alloc(alignof(max_align_t), size);
            if (!start) {
                throw std::bad_alloc();
            }
            current = start;
            end = static_cast<char*>(start) + size;
        }

        ~Chunk() {
            std::free(start);
        }
    };

    std::vector<Chunk> chunks_;

    bool has_space(size_t size, size_t alignment) const {
        void* aligned = std::align(alignment, size, chunks_.back().current, remaining_space());
        return aligned != nullptr;
    }

    size_t remaining_space() const {
        return static_cast<char*>(chunks_.back().end) - static_cast<char*>(chunks_.back().current);
    }

    void grow(size_t size, size_t alignment) {
        size_t chunk_size = std::max(size + alignment, static_cast<size_t>(4096)); // Default to 4KB chunks
        chunks_.emplace_back(chunk_size);
    }
};
