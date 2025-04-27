#pragma once

#include <functional>

#include "../serialize.hpp"

#include "../../amyr-utils/vec.hpp"

struct MemEncoder: Encoder {
    Vec<uint8_t> data;

    MemEncoder() {
        data = Vec<uint8_t>(0);
    }

    size_t position() const noexcept {
        return data.length();
    }

    Vec<uint8_t> finish() {
        return std::move(data);
    }

    // Write up to `N` bytes to this encoder.
    //
    // This function can be used to avoid the overhead of calling memcpy for writes that
    // have runtime-variable length, but are small and have a small fixed upper bound.
    //
    // This can be used to do in-place encoding as is done for leb128 (without this function
    // we would need to write to a temporary buffer then memcpy into the encoder), and it can
    // also be used to implement the varint scheme we use for rmeta and dep graph encoding,
    // where we only want to encode the first few bytes of an integer. Note that common
    // architectures support fixed-size writes up to 8 bytes with one instruction, so while this
    // does in some sense do wasted work, we come out ahead.

    template<size_t N>
    inline void write_with(std::function<size_t(std::array<uint8_t, N>)>& visitor) {
        data.reserve(N);

        size_t old_len = data.length();

        data.resize(old_len + N);
        
        uint8_t* buf_ptr = data.as_mut_ptr() + old_len;
        std::memset(buf_ptr, 0, N);
        
        size_t written = visitor(buf_ptr);

        if (written > N) {
            panic_invalid_write<N>(written);
        }
        
        // Trim to actual written size
        data_.resize(old_size + written);
    }

    template <size_t N>
    void write_array(const std::array<uint8_t, N>& arr) {
        data_.insert(data_.end(), arr.begin(), arr.end());
    }

private:
    template <size_t N>
    [[noreturn]] static void panic_invalid_write(size_t written) {
        throw std::invalid_argument(
            "MemEncoder::write_with<" + std::to_string(N) + 
            "> cannot write " + std::to_string(written) + " bytes"
        );
    }
}; 
