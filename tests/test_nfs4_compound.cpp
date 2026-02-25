#include "nfs4/compound.hpp"
#include "nfs4/fh_ops.hpp"
#include "xdr/xdr.hpp"

#include <gtest/gtest.h>
#include <cstdint>
#include <vector>

// ── COMPOUND header encoding ─────────────────────────────────────────────────

// Verify the COMPOUND4args wire layout: tag BEFORE minorversion.
// call_compound() builds: [tag:string] [minorversion:u32=0] [numops:u32] [ops]
// We cannot call call_compound() without a live server, so we test the header
// encoding directly by replicating the exact logic in the production code.
static std::vector<uint8_t> encode_compound_header(const std::string& tag,
                                                    uint32_t num_ops) {
    XdrEncoder hdr;
    hdr.put_string(tag);
    hdr.put_uint32(0);        // minorversion
    hdr.put_uint32(num_ops);
    return hdr.release();
}

TEST(Nfs4Compound, TagBeforeMinorversion) {
    // Tag "test" encodes as: length(4) + 't''e''s''t'
    // minorversion follows immediately after.
    auto hdr = encode_compound_header("test", 0);

    // bytes 0-3: XDR string length = 4
    ASSERT_GE(hdr.size(), 16u);
    EXPECT_EQ(hdr[0], 0x00);
    EXPECT_EQ(hdr[1], 0x00);
    EXPECT_EQ(hdr[2], 0x00);
    EXPECT_EQ(hdr[3], 0x04);  // length = 4

    // bytes 4-7: 't' 'e' 's' 't'
    EXPECT_EQ(hdr[4], 't');
    EXPECT_EQ(hdr[5], 'e');
    EXPECT_EQ(hdr[6], 's');
    EXPECT_EQ(hdr[7], 't');

    // bytes 8-11: minorversion = 0
    EXPECT_EQ(hdr[8],  0x00);
    EXPECT_EQ(hdr[9],  0x00);
    EXPECT_EQ(hdr[10], 0x00);
    EXPECT_EQ(hdr[11], 0x00);

    // bytes 12-15: numops = 0
    EXPECT_EQ(hdr[12], 0x00);
    EXPECT_EQ(hdr[13], 0x00);
    EXPECT_EQ(hdr[14], 0x00);
    EXPECT_EQ(hdr[15], 0x00);
}

TEST(Nfs4Compound, EmptyTagHeader) {
    // Empty tag: string length = 0, no data bytes, no padding (0-length = 4-byte aligned)
    auto hdr = encode_compound_header("", 3);

    // length(4): 0x00000000
    ASSERT_GE(hdr.size(), 12u);
    EXPECT_EQ(hdr[0], 0x00);
    EXPECT_EQ(hdr[1], 0x00);
    EXPECT_EQ(hdr[2], 0x00);
    EXPECT_EQ(hdr[3], 0x00);  // length = 0

    // minorversion = 0
    EXPECT_EQ(hdr[4], 0x00);
    EXPECT_EQ(hdr[7], 0x00);

    // numops = 3
    EXPECT_EQ(hdr[8],  0x00);
    EXPECT_EQ(hdr[9],  0x00);
    EXPECT_EQ(hdr[10], 0x00);
    EXPECT_EQ(hdr[11], 0x03);
}

TEST(Nfs4Compound, PutrootfhEncoding) {
    // PUTROOTFH is just op code 24.
    XdrEncoder enc;
    nfs4::encode_putrootfh(enc);
    const auto& b = enc.bytes();

    ASSERT_EQ(b.size(), 4u);
    EXPECT_EQ(b[0], 0x00);
    EXPECT_EQ(b[1], 0x00);
    EXPECT_EQ(b[2], 0x00);
    EXPECT_EQ(b[3], 24);  // OP_PUTROOTFH = 24
}

TEST(Nfs4Compound, GetfhEncoding) {
    XdrEncoder enc;
    nfs4::encode_getfh(enc);
    const auto& b = enc.bytes();

    ASSERT_EQ(b.size(), 4u);
    EXPECT_EQ(b[3], 10);  // OP_GETFH = 10
}

TEST(Nfs4Compound, PutfhEncoding) {
    Nfs4Fh fh;
    fh.data = {0x01, 0x02, 0x03, 0x04};
    XdrEncoder enc;
    nfs4::encode_putfh(enc, fh);
    const auto& b = enc.bytes();

    // OP_PUTFH(u32=22) + opaque(length=4 + data 4 bytes)
    ASSERT_EQ(b.size(), 12u);
    EXPECT_EQ(b[3], 22);   // OP_PUTFH
    EXPECT_EQ(b[7], 0x04); // fh length = 4
    EXPECT_EQ(b[8],  0x01);
    EXPECT_EQ(b[9],  0x02);
    EXPECT_EQ(b[10], 0x03);
    EXPECT_EQ(b[11], 0x04);
}

// ── Per-op result decode (from hand-crafted reply bytes) ─────────────────────

static std::vector<uint8_t> make_u32(uint32_t v) {
    return {
        static_cast<uint8_t>(v >> 24), static_cast<uint8_t>(v >> 16),
        static_cast<uint8_t>(v >> 8),  static_cast<uint8_t>(v)
    };
}

static void append_u32(std::vector<uint8_t>& buf, uint32_t v) {
    auto b = make_u32(v);
    buf.insert(buf.end(), b.begin(), b.end());
}

TEST(Nfs4Compound, DecodePutfhOk) {
    // [resop=22][status=0]
    std::vector<uint8_t> reply;
    append_u32(reply, 22);  // OP_PUTFH
    append_u32(reply, 0);   // NFS4_OK

    XdrDecoder dec(reply);
    EXPECT_NO_THROW(nfs4::decode_putfh_result(dec));
}

TEST(Nfs4Compound, DecodePutfhError) {
    std::vector<uint8_t> reply;
    append_u32(reply, 22);    // OP_PUTFH
    append_u32(reply, 70);    // NFS4ERR_STALE

    XdrDecoder dec(reply);
    try {
        nfs4::decode_putfh_result(dec);
        FAIL() << "Expected Nfs4Error";
    } catch (const Nfs4Error& e) {
        EXPECT_EQ(e.status, 70u);
    }
}

TEST(Nfs4Compound, DecodeGetfhOk) {
    // [resop=10][status=0][fh_opaque: len=4 + bytes]
    std::vector<uint8_t> reply;
    append_u32(reply, 10);  // OP_GETFH
    append_u32(reply, 0);   // NFS4_OK
    append_u32(reply, 4);   // fh length
    reply.insert(reply.end(), {0xAA, 0xBB, 0xCC, 0xDD});

    XdrDecoder dec(reply);
    Nfs4Fh fh = nfs4::decode_getfh_result(dec);
    ASSERT_EQ(fh.data.size(), 4u);
    EXPECT_EQ(fh.data[0], 0xAA);
    EXPECT_EQ(fh.data[3], 0xDD);
}

TEST(Nfs4Compound, CheckCompoundStatusOk) {
    // Simulate a full COMPOUND4res header: [status=0][tag=""][numops=1]
    std::vector<uint8_t> reply;
    append_u32(reply, 0);  // outer status = NFS4_OK
    append_u32(reply, 0);  // tag length = 0
    append_u32(reply, 1);  // numops = 1

    XdrDecoder dec(reply);
    EXPECT_NO_THROW(nfs4::check_compound_status(dec));
    // After check_compound_status, dec is positioned at start of resarray
    EXPECT_EQ(dec.remaining(), 0u);
}

TEST(Nfs4Compound, CheckCompoundStatusError) {
    std::vector<uint8_t> reply;
    append_u32(reply, static_cast<uint32_t>(Nfsstat4::NFS4ERR_RESOURCE));
    append_u32(reply, 0);  // tag (still present even on error)
    append_u32(reply, 0);  // numops

    XdrDecoder dec(reply);
    EXPECT_THROW(nfs4::check_compound_status(dec), Nfs4Error);
}
