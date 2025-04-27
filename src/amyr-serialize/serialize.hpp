#pragma once

#include <cstdint>
#include <vector>
#include <string_view>
#include <system_error>
#include <stdexcept>
#include <type_traits>

/// A byte that [cannot occur in UTF8 sequences][utf8]. Used to mark the end of a string.
/// This way we can skip validation and still be relatively sure that deserialization
/// did not desynchronize.
///
/// [utf8]: https://en.wikipedia.org/w/index.php?title=UTF-8&oldid=1058865525#Codepage_layout
static constexpr uint8_t STR_SENTINEL = 0xC1;

/// A note about error handling.
///
/// Encoders may be fallible, but in practice failure is rare and there are so
/// many nested calls that typical Rust error handling (via `Result` and `?`)
/// is pervasive and has non-trivial cost. Instead, impls of this trait must
/// implement a delayed error handling strategy. If a failure occurs, they
/// should record this internally, and all subsequent encoding operations can
/// be processed or ignored, whichever is appropriate. Then they should provide
/// a `finish` method that finishes up encoding. If the encoder is fallible,
/// `finish` should return a `Result` that indicates success or failure.
///
/// This current does not support `f32` nor `f64`, as they're not needed in any
/// serialized data structures. That could be changed, but consider whether it
/// really makes sense to store floating-point values at all.
class Encoder {
protected:
    std::error_code ec;

    // Core encoding interface
    virtual void encode_bytes(const uint8_t* data, size_t size) = 0;

public:
    virtual ~Encoder() = default;

    // Primitive type encoding
    virtual void emit_usize(uint64_t v)  { encode_uint(v); }
    virtual void emit_u128(__uint128_t v) { encode_uint(v); }
    virtual void emit_u64(uint64_t v)    { encode_uint(v); }
    virtual void emit_u32(uint32_t v)    { encode_uint(v); }
    virtual void emit_u16(uint16_t v)    { encode_uint(v); }
    virtual void emit_u8(uint8_t v)      { encode_uint(v); }

    virtual void emit_isize(int64_t v)   { encode_int(v); }
    virtual void emit_i128(__int128_t v) { encode_int(v); }
    virtual void emit_i64(int64_t v)     { encode_int(v); }
    virtual void emit_i32(int32_t v)     { encode_int(v); }
    virtual void emit_i16(int16_t v)     { encode_int(v); }

    inline void emit_i8(int8_t v) {
        if (!ec) encode_uint(static_cast<uint8_t>(v));
    }

    inline void emit_bool(bool v) {
        if (!ec) encode_uint(static_cast<uint8_t>(v ? 1 : 0));
    }

    inline void emit_char(char32_t v) {
        if (!ec) encode_uint(static_cast<uint32_t>(v));
    }

    inline void emit_str(std::string_view s) {
        if (ec) return;
        
        // Emit length as 64-bit unsigned
        emit_u64(s.size());
        
        // Emit string content
        emit_raw_bytes(reinterpret_cast<const uint8_t*>(s.data()), s.size());
        
        // Emit UTF-8 sentinel
        encode_uint(STR_SENTINEL);
    }

    void emit_raw_bytes(const uint8_t* data, size_t size) {
        if (!ec) encode_bytes(data, size);
    }


protected:
    // Helper template for integer encoding (little-endian)
    template<typename T>
    void encode_uint(T value) {
        if (ec) return;
        
        uint8_t bytes[sizeof(T)];
        for (size_t i = 0; i < sizeof(T); ++i) {
            bytes[i] = static_cast<uint8_t>(value & 0xFF);
            value >>= 8;
        }
        encode_bytes(bytes, sizeof(T));
    }

    template<typename T>
    void encode_int(T value) {
        using UnsignedT = typename std::make_unsigned<T>::type;
        encode_uint(static_cast<UnsignedT>(value));
    }
};

// The following comment has been taken from [Rust Compiler's serizalize crate](https://github.com/rust-lang/rust/blob/master/compiler/rustc_serialize/src/serialize.rs)
// Note: all the methods in this trait are infallible, which may be surprising.
// They used to be fallible (i.e. return a `Result`) but many of the impls just
// panicked when something went wrong, and for the cases that didn't the
// top-level invocation would also just panic on failure. Switching to
// infallibility made things faster and lots of code a little simpler and more
// concise.
///
/// This current does not support `f32` nor `f64`, as they're not needed in any
/// serialized data structures. That could be changed, but consider whether it
/// really makes sense to store floating-point values at all.
class Decoder {
public:

    virtual ~Decoder() = default;

    // Primitive type decoding
    virtual uint64_t read_usize() = 0;
    virtual __uint128_t read_u128() = 0;
    virtual uint64_t read_u64() = 0;
    virtual uint32_t read_u32() = 0;
    virtual uint16_t read_u16() = 0;
    virtual uint8_t read_u8() = 0;

    virtual int64_t read_isize() = 0;
    virtual __int128_t read_i128() = 0;
    virtual int64_t read_i64() = 0;
    virtual int32_t read_i32() = 0;
    virtual int16_t read_i16() = 0;

    int8_t read_i8() {
        return static_cast<int8_t>(read_u8());
    }

    bool read_bool() {
        return read_u8() != 0;
    }

    char32_t read_char() {
        uint32_t code_point = read_u32();
        if (code_point > 0x10FFFF) {
            throw std::runtime_error("Invalid Unicode code point");
        }
        return static_cast<char32_t>(code_point);
    }

    std::string_view read_str() {
        const uint64_t len = read_usize();
        const uint8_t* bytes = read_raw_bytes(len + 1);
        
        if (bytes[len] != STR_SENTINEL) {
            throw std::runtime_error("Missing string sentinel");
        }
        
        return std::string_view(reinterpret_cast<const char*>(bytes), len);
    }

    virtual const uint8_t* read_raw_bytes(size_t len) = 0;
    virtual uint8_t peek_byte() const = 0;
    virtual size_t position() const = 0;
};
    
class BufferDecoder : public Decoder {
    const uint8_t* data_;
    size_t size_;
    size_t pos_ = 0;

public:
    BufferDecoder(const uint8_t* data, size_t size) 
        : data_(data), size_(size) {}

    uint64_t read_usize() override { return read_little_endian<uint64_t>(); }
    uint64_t read_u64() override { return read_little_endian<uint64_t>(); }
    uint32_t read_u32() override { return read_little_endian<uint32_t>(); }
    uint16_t read_u16() override { return read_little_endian<uint16_t>(); }
    uint8_t read_u8() override { return read_byte(); }

    __uint128_t read_u128() override {
        uint64_t low = read_u64();
        uint64_t high = read_u64();
        return (static_cast<__uint128_t>(high) << 64) | low;
    }

    int64_t read_isize() override { return static_cast<int64_t>(read_usize()); }
    int64_t read_i64() override { return static_cast<int64_t>(read_u64()); }
    int32_t read_i32() override { return static_cast<int32_t>(read_u32()); }
    int16_t read_i16() override { return static_cast<int16_t>(read_u16()); }
    __int128_t read_i128() override { return static_cast<__int128_t>(read_u128()); }

    const uint8_t* read_raw_bytes(size_t len) override {
        check_available(len);
        const uint8_t* result = data_ + pos_;
        pos_ += len;
        return result;
    }

    uint8_t peek_byte() const override {
        check_available(1);
        return data_[pos_];
    }

    size_t position() const override { return pos_; }

private:
    template<typename T>
    T read_little_endian() {
        static_assert(std::is_unsigned_v<T>, "T must be unsigned");
        check_available(sizeof(T));
        
        T value = 0;
        for (size_t i = 0; i < sizeof(T); ++i) {
            value |= static_cast<T>(data_[pos_++]) << (8 * i);
        }
        return value;
    }

    uint8_t read_byte() {
        check_available(1);
        return data_[pos_++];
    }

    void check_available(size_t required) const {
        if (pos_ + required > size_) {
            throw std::out_of_range("Insufficient data in buffer");
        }
    }
};

template <typename T, typename E = Encoder>
struct Encodable {
    void encode(E& encoder) const {
        static_cast<const T*>(this)->encode(encoder);
    }
};

/// Decodable concept implementation using CRTP
template <typename T, typename D = Decoder>
struct Decodable {
    static T decode(D& decoder) {
        return T::decode(decoder);
    }
};