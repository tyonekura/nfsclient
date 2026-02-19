#include "nfs/lookup.hpp"
#include "nfs/read.hpp"
#include "nfs/write.hpp"
#include "xdr/xdr.hpp"

#include <gtest/gtest.h>

// ── Helpers ──────────────────────────────────────────────────────────────────

static Fh3 make_fh(std::initializer_list<uint8_t> bytes) {
    return Fh3{std::vector<uint8_t>(bytes)};
}

// Encode a post_op_attr with attributes_follow = false
static void append_no_attrs(XdrEncoder& enc) {
    enc.put_uint32(0u);  // attributes_follow = FALSE
}

// ── LOOKUP ───────────────────────────────────────────────────────────────────

TEST(LookupEncode, ArgsLayout) {
    // dir fh = 4 bytes, name = "test" (4 bytes, no padding)
    // Wire: fh_len(4) + fh_data(4) + name_len(4) + name_data(4) = 16 bytes
    const auto args = nfs3::encode_lookup_args(make_fh({0x01, 0x02, 0x03, 0x04}), "test");
    EXPECT_EQ(args.size(), 16u);

    XdrDecoder dec(args);
    const auto fh_bytes = dec.get_opaque();
    ASSERT_EQ(fh_bytes.size(), 4u);
    EXPECT_EQ(fh_bytes[0], 0x01u);

    EXPECT_EQ(dec.get_string(), "test");
}

TEST(LookupDecode, OkReturnsFileHandle) {
    const std::vector<uint8_t> expected_fh = {0xAA, 0xBB, 0xCC, 0xDD};

    XdrEncoder enc;
    enc.put_uint32(0u);              // NFS3_OK
    enc.put_opaque(expected_fh);     // object fh3
    append_no_attrs(enc);            // obj_attributes
    append_no_attrs(enc);            // dir_attributes

    const auto fh = nfs3::decode_lookup_reply(enc.release());
    EXPECT_EQ(fh.data, expected_fh);
}

TEST(LookupDecode, NonZeroStatusThrows) {
    XdrEncoder enc;
    enc.put_uint32(2u);  // NFS3ERR_NOENT
    append_no_attrs(enc);  // dir_attributes in resfail

    EXPECT_THROW(nfs3::decode_lookup_reply(enc.release()), std::runtime_error);
}

// ── READ ─────────────────────────────────────────────────────────────────────

TEST(ReadEncode, ArgsLayout) {
    // fh=4 bytes: fh_len(4)+fh_data(4) + offset(8) + count(4) = 20 bytes
    const auto args = nfs3::encode_read_args(make_fh({0x01, 0x02, 0x03, 0x04}),
                                              0x0000000100000000ULL, 512u);
    EXPECT_EQ(args.size(), 20u);

    XdrDecoder dec(args);
    dec.get_opaque();                                   // fh3
    EXPECT_EQ(dec.get_uint64(), 0x0000000100000000ULL); // offset
    EXPECT_EQ(dec.get_uint32(), 512u);                  // count
}

TEST(ReadDecode, OkReturnsData) {
    const std::vector<uint8_t> file_data = {0x11, 0x22, 0x33, 0x44};

    XdrEncoder enc;
    enc.put_uint32(0u);          // NFS3_OK
    append_no_attrs(enc);        // file_attributes
    enc.put_uint32(4u);          // count
    enc.put_uint32(1u);          // eof = true
    enc.put_opaque(file_data);   // data

    const auto result = nfs3::decode_read_reply(enc.release());
    EXPECT_EQ(result, file_data);
}

TEST(ReadDecode, NonZeroStatusThrows) {
    XdrEncoder enc;
    enc.put_uint32(5u);    // NFS3ERR_IO
    append_no_attrs(enc);  // file_attributes in resfail

    EXPECT_THROW(nfs3::decode_read_reply(enc.release()), std::runtime_error);
}

// ── WRITE ────────────────────────────────────────────────────────────────────

TEST(WriteEncode, ArgsLayout) {
    const std::vector<uint8_t> data = {0xDE, 0xAD, 0xBE, 0xEF};
    const auto args = nfs3::encode_write_args(make_fh({0x01, 0x02, 0x03, 0x04}),
                                               0ULL, Stable3::FILE_SYNC,
                                               data.data(), data.size());
    // fh_len(4)+fh_data(4) + offset(8) + count(4) + stable(4) +
    // data_len(4)+data(4) = 32 bytes
    EXPECT_EQ(args.size(), 32u);

    XdrDecoder dec(args);
    dec.get_opaque();                                     // fh3
    EXPECT_EQ(dec.get_uint64(), 0ULL);                    // offset
    EXPECT_EQ(dec.get_uint32(), 4u);                      // count
    EXPECT_EQ(dec.get_uint32(),
              static_cast<uint32_t>(Stable3::FILE_SYNC)); // stable
    EXPECT_EQ(dec.get_opaque(), data);                    // data
}

TEST(WriteDecode, OkReturnsResult) {
    const std::array<uint8_t, 8> verf = {1, 2, 3, 4, 5, 6, 7, 8};

    XdrEncoder enc;
    enc.put_uint32(0u);  // NFS3_OK
    // wcc_data: pre_op_attr (FALSE) + post_op_attr (FALSE)
    enc.put_uint32(0u);
    enc.put_uint32(0u);
    enc.put_uint32(100u);  // count written
    enc.put_uint32(static_cast<uint32_t>(Stable3::FILE_SYNC));  // committed
    enc.put_fixed_opaque(verf.data(), 8);  // writeverf3

    const auto result = nfs3::decode_write_reply(enc.release());
    EXPECT_EQ(result.count, 100u);
    EXPECT_EQ(result.committed, Stable3::FILE_SYNC);
    EXPECT_EQ(result.verf, verf);
}

TEST(WriteDecode, NonZeroStatusThrows) {
    XdrEncoder enc;
    enc.put_uint32(28u);  // NFS3ERR_NOSPC
    // wcc_data in resfail
    enc.put_uint32(0u);   // pre_op_attr FALSE
    enc.put_uint32(0u);   // post_op_attr FALSE

    EXPECT_THROW(nfs3::decode_write_reply(enc.release()), std::runtime_error);
}
