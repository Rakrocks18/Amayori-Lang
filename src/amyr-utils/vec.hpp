#include<memory>
#include<optional>
#include<stdexcept>
#include<type_traits>
#include<utility>

#include "iterator.hpp"

template<typename T, typename Allocator = std::allocator<T>>
class Vec {
    using AllocTraits = std::allocator_traits<Allocator>;

    Allocator alloc;
    T* ptr = nullptr;
    size_t cap = 0;
    size_t len = 0;

    void assert_index(size_t index) const {
        if (index >= len) {
            throw std::out_of_range("Vec index out of range");
        }
    }

    void move_elements(T* new_ptr, size_t new_cap) {
        for(size_t i = 0; i < len; ++i) {
            AllocTraits::construct(alloc, new_ptr + i, std::move(ptr[i]));
            AllocTraits::destroy(alloc, ptr + i);
        }

        AllocTraits::deallocate(alloc, ptr, cap);
        ptr = new_ptr;
        cap = new_cap;
    }

public:
    
    //Constructors
    explicit Vec(const Allocator& alloc = Allocator()) : alloc(alloc) {}

    explicit Vec(size_t capacity, const Allocator& alloc = Allocator()) : alloc(alloc) {
        reserve(capacity);
    }

    //Destructor
    ~Vec() {
        clear();
        AllocTraits::deallocate(alloc, ptr, cap);
    }

    //Move semantics with allocator awareness
    Vec(Vec&& other) noexcept
        : alloc(std::move(other.alloc)),
          ptr(other.ptr),
          cap(other.cap),
          len(other.len) {
        other.ptr = nullptr;
        other.cap = 0;
        other.len = 0;
    }

    // Capacity
    inline size_t length() const noexcept(true) 
    { return len; }
    
    inline size_t capacity() const noexcept 
        { return cap; }
    
    inline bool is_empty() const noexcept 
        { return len == 0; }

    Vec& operator=(Vec&& other) noexcept {
        if (this != &other) {
            clear();
            AllocTraits::deallocate(alloc, ptr, cap);
            
            if (AllocTraits::propagate_on_container_move_assignment::value) {
                alloc = std::move(other.alloc);
            }
            
            ptr = other.ptr;
            cap = other.cap;
            len = other.len;
            
            other.ptr = nullptr;
            other.cap = 0;
            other.len = 0;
        }
        return *this;
    }

    // No copying
    Vec(const Vec&) = delete;
    Vec& operator=(const Vec&) = delete;

    void reserve(size_t new_cap) {
        if (new_cap <= cap) return;

        size_t actual_new_cap = cap ? cap : 1;
        while (actual_new_cap < new_cap) {
            actual_new_cap *= 2;
        }

        T* new_ptr = AllocTraits::allocate(alloc, actual_new_cap);
        
        try {
            move_elements(new_ptr, actual_new_cap);
        } catch (...) {
            AllocTraits::deallocate(alloc, new_ptr, actual_new_cap);
            throw;
        }
    }

    void shrink_to_fit() {
        if (cap == len) return;
        
        Vec new_vec(alloc);
        new_vec.reserve(len);
        
        for (size_t i = 0; i < len; ++i) {
            new_vec.push(std::move(ptr[i]));
            AllocTraits::destroy(alloc, ptr + i);
        }
        
        AllocTraits::deallocate(alloc, ptr, cap);
        ptr = new_vec.ptr;
        cap = len;
        new_vec.ptr = nullptr;
    }

    T& operator[](size_t index) {
        assert_index(index);
        return ptr[index];
    }

    const T& operator[](size_t index) const {
        assert_index(index);
        return ptr[index];
    }

    // Modifiers
    template<typename U = T>
    void push(U&& value) {
        if (len >= cap) {
            reserve(cap ? cap * 2 : 1);
        }
        AllocTraits::construct(alloc, ptr + len, std::forward<U>(value));
        len++;
    }

    std::optional<T> pop() {
        if (len == 0) return std::nullopt;
        len--;
        T value = std::move(ptr[len]);
        AllocTraits::destroy(alloc, ptr + len);
        return std::make_optional(std::move(value));
    }

    void clear() noexcept {
        for (size_t i = 0; i < len; ++i) {
            AllocTraits::destroy(alloc, ptr + i);
        }
        len = 0;
    }

    void resize(size_t new_len) {
        if (new_len > cap) {
            reserve(new_len);
        } else if (new_len < len) {
            truncate(new_len);
        }
    }

    void truncate(size_t new_len) {
        if (new_len >= len) return;
        for (size_t i = new_len; i < len; ++i) {
            AllocTraits::destroy(alloc, ptr + i);
        }
        len = new_len;
    }   

    template<typename T, typename Allocator>
    class VecIter : public Iterator<VecIter<T, Allocator>, T> {
        size_t index = 0;
        Vec<T, Allocator>& vec;

    public:
        explicit VecIter(Vec<T, Allocator>& v) : vec(v) {}

        std::optional<T> next() {
            if (index >= vec.size()) {
                return std::nullopt;
            }
            T value = std::move(vec[index]);
            vec[index].~T();
            index++;
            return std::move(value);
        }
    }

    VecIter<T, Allocator> iter() {
        return VecIter<T, Allocator>(*this);
    }

    // Additional method needed for safe iteration
    T extract(size_t index) {
        if (index >= len) throw std::out_of_range("extract index out of range");
        T value = std::move(ptr[index]);
        ptr[index].~T();
        // Shift elements to maintain contiguous storage
        for (size_t i = index; i < len - 1; ++i) {
            AllocTraits::construct(alloc, ptr + i, std::move(ptr[i + 1]));
            AllocTraits::destroy(alloc, ptr + i + 1);
        }
        len--;
        return value;
    }



    // Memory management
    T* as_mut_ptr() noexcept { return ptr; }
    const T* as_mut_ptr() const noexcept { return ptr; }

    // Allocator access
    Allocator get_allocator() const noexcept { return alloc; }

    // Special operations
    void swap(Vec& other) noexcept {
        using std::swap;
        swap(ptr, other.ptr);
        swap(cap, other.cap);
        swap(len, other.len);
        if (AllocTraits::propagate_on_container_swap::value) {
            swap(alloc, other.alloc);
        }
    }

    Vec<T> split_off(size_t at) {
        if (at > len) throw std::out_of_range("split_off index out of range");
        
        Vec new_vec(alloc);
        new_vec.reserve(len - at);
        
        for (size_t i = at; i < len; ++i) {
            new_vec.push(std::move(ptr[i]));
            AllocTraits::destroy(alloc, ptr + i);
        }
        
        len = at;
        return new_vec;
    }
};