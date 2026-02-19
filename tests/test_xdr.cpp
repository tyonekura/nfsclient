#include "xdr/xdr.hpp"

#include <gtest/gtest.h>

// ── XdrEncoder ───────────────────────────────────────────────────────────────

TEST(XdrEncoder, PutUint32BigEndian) {
    XdrEncoder enc;
    enc.put_uint32(0x01020304u);
    const auto& b = enc.bytes();
    ASSERT_EQ(b.size(), 4u);
    EXPECT_EQ(b[0], 0x01);
    EXPECT_EQ(b[1], 0x02);
    EXPECT_EQ(b[2], 0x03);
    EXPECT_EQ(b[3], 0x04);
}

TEST(XdrEncoder, PutUint64BigEndian) {
    XdrEncoder enc;
    enc.put_uint64(0x0102030405060708ULL);
    const auto& b = enc.bytes();
    ASSERT_EQ(b.size(), 8u);
    EXPECT_EQ(b[0], 0x01);
    EXPECT_EQ(b[1], 0x02);
    EXPECT_EQ(b[2], 0x03);
    EXPECT_EQ(b[3], 0x04);
    EXPECT_EQ(b[4], 0x05);
    EXPECT_EQ(b[5], 0x06);
    EXPECT_EQ(b[6], 0x07);
    EXPECT_EQ(b[7], 0x08);
}

TEST(XdrEncoder, PutOpaqueNoPadding) {
    // 4-byte opaque: no padding needed
    const std::vector<uint8_t> data = {0xAA, 0xBB, 0xCC, 0xDD};
    XdrEncoder enc;
    enc.put_opaque(data);
    const auto& b = enc.bytes();
    ASSERT_EQ(b.size(), 8u);  // 4 (length) + 4 (data)
    EXPECT_EQ(b[3], 0x04);    // length field = 4
    EXPECT_EQ(b[4], 0xAA);
}

TEST(XdrEncoder, PutOpaqueWithPadding) {
    // 3-byte opaque: 1 byte of padding
    const std::vector<uint8_t> data = {0xAA, 0xBB, 0xCC};
    XdrEncoder enc;
    enc.put_opaque(data);
    const auto& b = enc.bytes();
    ASSERT_EQ(b.size(), 8u);  // 4 (length) + 3 (data) + 1 (pad)
    EXPECT_EQ(b[3], 0x03);    // length = 3
    EXPECT_EQ(b[7], 0x00);    // padding byte
}

TEST(XdrEncoder, PutString) {
    XdrEncoder enc;
    enc.put_string("hi");  // 2 bytes → 2 bytes padding
    const auto& b = enc.bytes();
    ASSERT_EQ(b.size(), 8u);  // 4 (length) + 2 (data) + 2 (pad)
    EXPECT_EQ(b[3], 0x02);    // length = 2
    EXPECT_EQ(b[4], 'h');
    EXPECT_EQ(b[5], 'i');
    EXPECT_EQ(b[6], 0x00);
    EXPECT_EQ(b[7], 0x00);
}

TEST(XdrEncoder, PutFixedOpaque) {
    const uint8_t data[3] = {0x01, 0x02, 0x03};
    XdrEncoder enc;
    enc.put_fixed_opaque(data, 3);
    const auto& b = enc.bytes();
    ASSERT_EQ(b.size(), 4u);  // 3 (data) + 1 (pad)
    EXPECT_EQ(b[0], 0x01);
    EXPECT_EQ(b[3], 0x00);  // padding
}

// ── XdrDecoder ───────────────────────────────────────────────────────────────

TEST(XdrDecoder, RoundTripUint32) {
    XdrEncoder enc;
    enc.put_uint32(0xDEADBEEFu);
    const auto data = enc.release();
    XdrDecoder dec(data);
    EXPECT_EQ(dec.get_uint32(), 0xDEADBEEFu);
}

TEST(XdrDecoder, RoundTripUint64) {
    XdrEncoder enc;
    enc.put_uint64(0xCAFEBABE12345678ULL);
    const auto data = enc.release();
    XdrDecoder dec(data);
    EXPECT_EQ(dec.get_uint64(), 0xCAFEBABE12345678ULL);
}

TEST(XdrDecoder, RoundTripOpaque) {
    const std::vector<uint8_t> orig = {0x01, 0x02, 0x03, 0x04, 0x05};
    XdrEncoder enc;
    enc.put_opaque(orig);
    const auto data = enc.release();
    XdrDecoder dec(data);
    EXPECT_EQ(dec.get_opaque(), orig);
    EXPECT_EQ(dec.remaining(), 0u);
}

TEST(XdrDecoder, RoundTripString) {
    XdrEncoder enc;
    enc.put_string("hello");
    const auto data = enc.release();
    XdrDecoder dec(data);
    EXPECT_EQ(dec.get_string(), "hello");
}

TEST(XdrDecoder, RoundTripFixedOpaque) {
    const uint8_t raw[8] = {0xAA, 0xBB, 0xCC, 0xDD, 0x11, 0x22, 0x33, 0x44};
    XdrEncoder enc;
    enc.put_fixed_opaque(raw, 8);
    const auto data = enc.release();
    XdrDecoder dec(data);
    const auto result = dec.get_fixed_opaque(8);
    ASSERT_EQ(result.size(), 8u);
    EXPECT_EQ(std::vector<uint8_t>(raw, raw + 8), result);
}

TEST(XdrDecoder, MultipleValues) {
    XdrEncoder enc;
    enc.put_uint32(1u);
    enc.put_uint64(2u);
    enc.put_string("abc");
    const auto data = enc.release();
    XdrDecoder dec(data);
    EXPECT_EQ(dec.get_uint32(), 1u);
    EXPECT_EQ(dec.get_uint64(), 2u);
    EXPECT_EQ(dec.get_string(), "abc");
    EXPECT_EQ(dec.remaining(), 0u);
}

TEST(XdrDecoder, UnderflowThrows) {
    const std::vector<uint8_t> data = {0x00, 0x00};  // only 2 bytes
    XdrDecoder dec(data);
    EXPECT_THROW(dec.get_uint32(), std::runtime_error);
}

TEST(XdrDecoder, GetRemaining) {
    XdrEncoder enc;
    enc.put_uint32(42u);
    enc.put_uint32(99u);
    const auto data = enc.release();
    XdrDecoder dec(data);
    dec.get_uint32();  // consume first value
    const auto rest = dec.get_remaining();
    ASSERT_EQ(rest.size(), 4u);
    XdrDecoder dec2(rest);
    EXPECT_EQ(dec2.get_uint32(), 99u);
}
