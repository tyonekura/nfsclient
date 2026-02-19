#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

// XDR encoder: serializes values into a big-endian byte buffer.
class XdrEncoder {
public:
    void put_uint32(uint32_t v);
    void put_uint64(uint64_t v);

    // Variable-length opaque: 4-byte length prefix + data + 4-byte alignment padding.
    void put_opaque(const uint8_t* data, size_t size);
    void put_opaque(const std::vector<uint8_t>& data) {
        put_opaque(data.data(), data.size());
    }

    // String: same wire encoding as variable-length opaque.
    void put_string(const std::string& s) {
        put_opaque(reinterpret_cast<const uint8_t*>(s.data()), s.size());
    }

    // Fixed-length opaque: data + 4-byte alignment padding, no length prefix.
    void put_fixed_opaque(const uint8_t* data, size_t size);

    const std::vector<uint8_t>& bytes() const { return buf_; }
    std::vector<uint8_t> release() { return std::move(buf_); }

private:
    std::vector<uint8_t> buf_;
};

// XDR decoder: deserializes values from a big-endian byte buffer.
// Throws std::runtime_error on buffer underflow.
class XdrDecoder {
public:
    XdrDecoder(const uint8_t* data, size_t size) : data_(data), size_(size), offset_(0) {}
    explicit XdrDecoder(const std::vector<uint8_t>& v)
        : XdrDecoder(v.data(), v.size()) {}

    uint32_t get_uint32();
    uint64_t get_uint64();

    // Variable-length opaque: reads 4-byte length, data, and alignment padding.
    std::vector<uint8_t> get_opaque();

    // String: same wire encoding as variable-length opaque.
    std::string get_string();

    // Fixed-length opaque: reads exactly n bytes + alignment padding, no length prefix.
    std::vector<uint8_t> get_fixed_opaque(size_t n);

    // Returns remaining bytes and advances the cursor to end.
    std::vector<uint8_t> get_remaining();

    size_t remaining() const { return size_ - offset_; }

private:
    void require(size_t n) const;

    const uint8_t* data_;
    size_t size_;
    size_t offset_;
};
