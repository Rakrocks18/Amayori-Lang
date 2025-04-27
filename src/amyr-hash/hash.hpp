#pragma once

#include <cstdint>
#include <iomanip>
#include <iostream>
#include <functional>

class Hash64 {
    uint64_t value;

public:
    static const Hash64 ZERO;

    explicit Hash64(uint64_t val) : value(val) {}

    u_int64_t as_u64() const { return value; }

    Hash64 wrapping_add(Hash64 other) const {
        return Hash64(value + other.value);
    }
    
    Hash64& operator^=(uint64_t rhs) {
        value ^= rhs;
        return *this;
    }

    friend std::ostream& operator<<(std::ostream& os, const Hash64& h) {
        return os << "Hash64(" << h.value << ")";
    }

    std::string to_hex() const {
        std::stringstream ss;
        ss << std::hex << std::setw(16) << std::setfill('0') << value;
        return ss.str();
    }
};

const Hash64 Hash64::ZERO = Hash64(0);

class Hash128 {
    unsigned __int128 value;

public:
    explicit Hash128(unsigned __int128 val) : value(val) {}
    explicit Hash128(uint64_t low, uint64_t high)
        : value((static_cast<unsigned __int128>(high) << 64) | low) {}

    Hash64 truncate() const {
        return Hash64(static_cast<uint64_t>(value));
    }

    Hash128 wrapping_add(Hash128 other) const {
        return Hash128(value + other.value);
    }

    unsigned __int128 as_u128() const { return value; }

    friend std::ostream& operator<<(std::ostream& os, const Hash128& h) {
        return os << "Hash128(" << static_cast<uint64_t>(h.value >> 64) 
                  << ":" << static_cast<uint64_t>(h.value) << ")";
    }

    std::string to_hex() const {
        std::stringstream ss;
        ss << std::hex << std::setw(16) << std::setfill('0') 
           << static_cast<uint64_t>(value >> 64)
           << std::setw(16) << static_cast<uint64_t>(value);
        return ss.str();
    }
};

namespace std {
    template<> 
    struct hash<Hash128> {
        size_t operator()(const Hash128& h) const {
            return hash<uint64_t>{}(h.truncate().as_u64());
        }
    };
}