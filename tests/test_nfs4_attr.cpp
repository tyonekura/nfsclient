#include "nfs4/nfs4_attr.hpp"
#include "nfs4/nfs4_types.hpp"
#include "xdr/xdr.hpp"

#include <gtest/gtest.h>
#include <vector>

using namespace nfs4;
using namespace nfs4::attr;

// ── Bitmap4 bit placement ────────────────────────────────────────────────────

// Attribute N → word N/32, bit (1u << (N%32)) — LSB-first per RFC 7530 §3.3.21

TEST(Nfs4Attr, Bitmap4SingleAttrType) {
    // TYPE = 1 → word 0, bit (1u << 1) = 0x00000002
    std::vector<uint32_t> bm;
    bitmap4_set(bm, TYPE);
    ASSERT_EQ(bm.size(), 1u);
    EXPECT_EQ(bm[0], 0x00000002u);
    EXPECT_TRUE(bitmap4_test(bm, TYPE));
    EXPECT_FALSE(bitmap4_test(bm, SIZE));
}

TEST(Nfs4Attr, Bitmap4SingleAttrSize) {
    // SIZE = 4 → word 0, bit (1u << 4) = 0x00000010
    std::vector<uint32_t> bm;
    bitmap4_set(bm, SIZE);
    ASSERT_EQ(bm.size(), 1u);
    EXPECT_EQ(bm[0], 0x00000010u);
}

TEST(Nfs4Attr, Bitmap4SingleAttrMode) {
    // MODE = 33 → word 1 (33/32=1), bit (1u << (33%32)) = 1u<<1 = 0x00000002
    std::vector<uint32_t> bm;
    bitmap4_set(bm, MODE);
    ASSERT_EQ(bm.size(), 2u);
    EXPECT_EQ(bm[0], 0u);
    EXPECT_EQ(bm[1], 0x00000002u);
    EXPECT_TRUE(bitmap4_test(bm, MODE));
}

TEST(Nfs4Attr, Bitmap4MultipleAttrs) {
    // type=1 (word 0), size=4 (word 0), fileid=20 (word 0), mode=33 (word 1)
    auto bm = make_bitmap4({TYPE, SIZE, FILEID, MODE});
    ASSERT_EQ(bm.size(), 2u);

    // TYPE=1: bit 1 → 0x00000002
    // SIZE=4: bit 4 → 0x00000010
    // FILEID=20: bit 20 → 0x00100000
    uint32_t expected_word0 = 0x00000002u | 0x00000010u | 0x00100000u;
    EXPECT_EQ(bm[0], expected_word0);

    // MODE=33: bit 1 (in word 1) → 0x00000002
    EXPECT_EQ(bm[1], 0x00000002u);
}

TEST(Nfs4Attr, Bitmap4TestAbsent) {
    std::vector<uint32_t> bm;
    bitmap4_set(bm, TYPE);
    EXPECT_FALSE(bitmap4_test(bm, SIZE));
    EXPECT_FALSE(bitmap4_test(bm, FILEID));
    // Out-of-range word
    EXPECT_FALSE(bitmap4_test(bm, MODE));  // MODE is word 1, bm has only word 0
}

// ── Bitmap4 XDR encode/decode ────────────────────────────────────────────────

TEST(Nfs4Attr, EncodeBitmap4) {
    auto bm = make_bitmap4({TYPE, SIZE});
    XdrEncoder enc;
    encode_bitmap4(enc, bm);
    const auto& b = enc.bytes();

    // [num_words=1][word0]
    ASSERT_EQ(b.size(), 8u);
    // num_words = 1
    EXPECT_EQ(b[3], 1u);
    // word0 = TYPE | SIZE = 0x00000002 | 0x00000010 = 0x00000012
    EXPECT_EQ(b[4], 0x00);
    EXPECT_EQ(b[5], 0x00);
    EXPECT_EQ(b[6], 0x00);
    EXPECT_EQ(b[7], 0x12);
}

TEST(Nfs4Attr, DecodeBitmap4RoundTrip) {
    auto original = make_bitmap4({TYPE, SIZE, FILEID, MODE});
    XdrEncoder enc;
    encode_bitmap4(enc, original);
    auto bytes = enc.release();

    XdrDecoder dec(bytes);
    auto decoded = decode_bitmap4(dec);
    ASSERT_EQ(decoded.size(), original.size());
    for (size_t i = 0; i < original.size(); ++i) {
        EXPECT_EQ(decoded[i], original[i]);
    }
}

// ── fattr4 decode ────────────────────────────────────────────────────────────

// Build a hand-crafted fattr4 reply for given attributes and decode it.
static void append_u32(std::vector<uint8_t>& buf, uint32_t v) {
    buf.push_back(v >> 24); buf.push_back(v >> 16);
    buf.push_back(v >> 8);  buf.push_back(v);
}

static void append_u64(std::vector<uint8_t>& buf, uint64_t v) {
    append_u32(buf, static_cast<uint32_t>(v >> 32));
    append_u32(buf, static_cast<uint32_t>(v));
}

TEST(Nfs4Attr, DecodeFattr4SizeFileid) {
    // Attributes: SIZE=4, FILEID=20
    // bitmap: word 0 = (1<<4) | (1<<20) = 0x00000010 | 0x00100000 = 0x00100010
    uint32_t bm0 = (1u << 4) | (1u << 20);

    // attrlist: uint64(4096) + uint64(99)
    std::vector<uint8_t> attrlist;
    append_u64(attrlist, 4096);
    append_u64(attrlist, 99);

    // fattr4 wire: [num_words=1][bm0][attrlist_len][attrlist_bytes]
    std::vector<uint8_t> wire;
    append_u32(wire, 1);    // num_words
    append_u32(wire, bm0);
    append_u32(wire, static_cast<uint32_t>(attrlist.size()));
    wire.insert(wire.end(), attrlist.begin(), attrlist.end());

    XdrDecoder dec(wire);
    Fattr4 attrs = decode_fattr4(dec);

    EXPECT_FALSE(attrs.type.has_value());
    ASSERT_TRUE(attrs.size.has_value());
    EXPECT_EQ(*attrs.size, 4096u);
    ASSERT_TRUE(attrs.fileid.has_value());
    EXPECT_EQ(*attrs.fileid, 99u);
    EXPECT_FALSE(attrs.mode.has_value());
}

TEST(Nfs4Attr, DecodeFattr4Type) {
    // Attribute: TYPE=1 → bit 1 → 0x00000002
    uint32_t bm0 = 0x00000002u;  // TYPE

    std::vector<uint8_t> attrlist;
    append_u32(attrlist, 1);  // NF4REG = 1

    std::vector<uint8_t> wire;
    append_u32(wire, 1);
    append_u32(wire, bm0);
    append_u32(wire, static_cast<uint32_t>(attrlist.size()));
    wire.insert(wire.end(), attrlist.begin(), attrlist.end());

    XdrDecoder dec(wire);
    Fattr4 attrs = decode_fattr4(dec);

    ASSERT_TRUE(attrs.type.has_value());
    EXPECT_EQ(*attrs.type, Ftype4::NF4REG);
}

// ── fattr4 encode (Sattr4) ────────────────────────────────────────────────────

TEST(Nfs4Attr, EncodeSattr4Size) {
    Sattr4 s;
    s.size = 8192;

    XdrEncoder enc;
    encode_fattr4(enc, s);
    auto bytes = enc.release();

    // Decode back: [num_words][bm_words...][attrlist_len][attrlist_bytes]
    XdrDecoder dec(bytes);
    Fattr4 attrs = decode_fattr4(dec);

    ASSERT_TRUE(attrs.size.has_value());
    EXPECT_EQ(*attrs.size, 8192u);
    EXPECT_FALSE(attrs.mode.has_value());
}

TEST(Nfs4Attr, EncodeSattr4Mode) {
    Sattr4 s;
    s.mode = 0644;

    XdrEncoder enc;
    encode_fattr4(enc, s);
    auto bytes = enc.release();

    XdrDecoder dec(bytes);
    Fattr4 attrs = decode_fattr4(dec);

    ASSERT_TRUE(attrs.mode.has_value());
    EXPECT_EQ(*attrs.mode, 0644u);
}

TEST(Nfs4Attr, EncodeSattr4Empty) {
    // No attributes set — bitmap should be empty, attrlist zero length.
    Sattr4 s;
    XdrEncoder enc;
    encode_fattr4(enc, s);
    const auto& b = enc.bytes();

    // [num_words=0][attrlist_len=0]
    ASSERT_GE(b.size(), 8u);
    EXPECT_EQ(b[3], 0u);  // num_words = 0
    EXPECT_EQ(b[7], 0u);  // attrlist len = 0
}
