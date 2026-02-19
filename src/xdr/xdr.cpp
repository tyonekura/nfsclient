#include "xdr.hpp"

#include <cstring>

// ── XdrEncoder ──────────────────────────────────────────────────────────────

void XdrEncoder::put_uint32(uint32_t v) {
    buf_.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    buf_.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    buf_.push_back(static_cast<uint8_t>((v >>  8) & 0xFF));
    buf_.push_back(static_cast<uint8_t>( v        & 0xFF));
}

void XdrEncoder::put_uint64(uint64_t v) {
    put_uint32(static_cast<uint32_t>(v >> 32));
    put_uint32(static_cast<uint32_t>(v & 0xFFFFFFFFu));
}

void XdrEncoder::put_opaque(const uint8_t* data, size_t size) {
    put_uint32(static_cast<uint32_t>(size));
    buf_.insert(buf_.end(), data, data + size);
    size_t pad = (4 - (size % 4)) % 4;
    for (size_t i = 0; i < pad; ++i) buf_.push_back(0);
}

void XdrEncoder::put_fixed_opaque(const uint8_t* data, size_t size) {
    buf_.insert(buf_.end(), data, data + size);
    size_t pad = (4 - (size % 4)) % 4;
    for (size_t i = 0; i < pad; ++i) buf_.push_back(0);
}

// ── XdrDecoder ──────────────────────────────────────────────────────────────

void XdrDecoder::require(size_t n) const {
    if (offset_ + n > size_)
        throw std::runtime_error("XdrDecoder: buffer underflow");
}

uint32_t XdrDecoder::get_uint32() {
    require(4);
    uint32_t v = (static_cast<uint32_t>(data_[offset_    ]) << 24) |
                 (static_cast<uint32_t>(data_[offset_ + 1]) << 16) |
                 (static_cast<uint32_t>(data_[offset_ + 2]) <<  8) |
                  static_cast<uint32_t>(data_[offset_ + 3]);
    offset_ += 4;
    return v;
}

uint64_t XdrDecoder::get_uint64() {
    uint64_t hi = get_uint32();
    uint64_t lo = get_uint32();
    return (hi << 32) | lo;
}

std::vector<uint8_t> XdrDecoder::get_opaque() {
    uint32_t len = get_uint32();
    require(len);
    std::vector<uint8_t> result(data_ + offset_, data_ + offset_ + len);
    offset_ += len;
    size_t pad = (4 - (len % 4)) % 4;
    require(pad);
    offset_ += pad;
    return result;
}

std::string XdrDecoder::get_string() {
    auto bytes = get_opaque();
    return std::string(bytes.begin(), bytes.end());
}

std::vector<uint8_t> XdrDecoder::get_fixed_opaque(size_t n) {
    require(n);
    std::vector<uint8_t> result(data_ + offset_, data_ + offset_ + n);
    offset_ += n;
    size_t pad = (4 - (n % 4)) % 4;
    require(pad);
    offset_ += pad;
    return result;
}

std::vector<uint8_t> XdrDecoder::get_remaining() {
    std::vector<uint8_t> result(data_ + offset_, data_ + size_);
    offset_ = size_;
    return result;
}
