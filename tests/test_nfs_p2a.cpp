// Unit tests for Phase 1 P2 items:
//   - SETATTR encode/decode (with and without sattrguard3)
//   - READDIR encode/decode (single page, eof, multiple entries)

#include "nfs/nfs_error.hpp"
#include "nfs/nfs3_types.hpp"
#include "nfs/setattr.hpp"
#include "nfs/readdir.hpp"
#include "xdr/xdr.hpp"

#include <gtest/gtest.h>

// ── Helpers ──────────────────────────────────────────────────────────────────

static Fh3 make_fh(std::initializer_list<uint8_t> bytes) {
    return Fh3{std::vector<uint8_t>(bytes)};
}

static void append_no_wcc(XdrEncoder& enc) {
    enc.put_uint32(0u);  // pre_op_attr: not present
    enc.put_uint32(0u);  // post_op_attr: not present
}

static void append_no_attrs(XdrEncoder& enc) {
    enc.put_uint32(0u);  // attributes_follow = FALSE
}

// ── SETATTR ───────────────────────────────────────────────────────────────────

TEST(SetattrEncode, NoGuardLayout) {
    Sattr3 attrs{};
    attrs.set_mode = true; attrs.mode = 0644;

    const auto args = nfs3::encode_setattr_args(make_fh({0x01, 0x02}), attrs);
    XdrDecoder dec(args);
    dec.get_opaque();           // fh3
    // sattr3: set_mode(1) + mode(1) + set_uid(1) + set_gid(1) + set_size(1) +
    //         set_atime(1) + set_mtime(1) = 7 uint32s
    EXPECT_EQ(dec.get_uint32(), 1u);     // set_mode = true
    EXPECT_EQ(dec.get_uint32(), 0644u);  // mode
    // skip remaining sattr3 fields (not set)
    for (int i = 0; i < 5; ++i) dec.get_uint32();
    // sattrguard3: check = false
    EXPECT_EQ(dec.get_uint32(), 0u);
    EXPECT_EQ(dec.remaining(), 0u);
}

TEST(SetattrEncode, WithGuardLayout) {
    Sattr3 attrs{};
    nfs3::SattrGuard3 guard{};
    guard.check      = true;
    guard.ctime_sec  = 1000;
    guard.ctime_nsec = 500;

    const auto args = nfs3::encode_setattr_args(make_fh({0xAA}), attrs, guard);
    XdrDecoder dec(args);
    dec.get_opaque();  // fh3
    // skip sattr3 (6 DONT_CHANGE uint32s = 24 bytes)
    for (int i = 0; i < 6; ++i) dec.get_uint32();
    // sattrguard3: check = true + ctime
    EXPECT_EQ(dec.get_uint32(), 1u);     // check = true
    EXPECT_EQ(dec.get_uint32(), 1000u);  // ctime_sec
    EXPECT_EQ(dec.get_uint32(), 500u);   // ctime_nsec
}

TEST(SetattrDecode, OkDoesNotThrow) {
    XdrEncoder enc;
    enc.put_uint32(0u);  // NFS3_OK
    append_no_wcc(enc);  // obj_wcc

    EXPECT_NO_THROW(nfs3::decode_setattr_reply(enc.release()));
}

TEST(SetattrDecode, NonZeroStatusThrowsNfsError) {
    XdrEncoder enc;
    enc.put_uint32(10002u);  // NFS3ERR_NOT_SYNC (guard mismatch)
    append_no_wcc(enc);

    try {
        nfs3::decode_setattr_reply(enc.release());
        FAIL() << "Expected NfsError";
    } catch (const NfsError& e) {
        EXPECT_EQ(e.status, 10002u);
        EXPECT_TRUE(e.is(Nfsstat3::NFS3ERR_NOT_SYNC));
    }
}

TEST(SetattrDecode, PermissionDeniedThrowsNfsError) {
    XdrEncoder enc;
    enc.put_uint32(1u);   // NFS3ERR_PERM
    append_no_wcc(enc);

    try {
        nfs3::decode_setattr_reply(enc.release());
        FAIL() << "Expected NfsError";
    } catch (const NfsError& e) {
        EXPECT_TRUE(e.is(Nfsstat3::NFS3ERR_PERM));
    }
}

// ── READDIR encode ────────────────────────────────────────────────────────────

TEST(ReaddirEncode, FirstCallLayout) {
    const std::array<uint8_t, 8> zeroverf{};
    const auto args = nfs3::encode_readdir_args(make_fh({0x01, 0x02, 0x03, 0x04}),
                                                 0ULL, zeroverf, 4096u);
    // fh_len(4)+fh_data(4) + cookie(8) + cookieverf(8) + count(4) = 28 bytes
    EXPECT_EQ(args.size(), 28u);

    XdrDecoder dec(args);
    dec.get_opaque();                    // fh3
    EXPECT_EQ(dec.get_uint64(), 0ULL);   // cookie
    const auto cv = dec.get_fixed_opaque(8);
    EXPECT_TRUE(std::all_of(cv.begin(), cv.end(), [](uint8_t b){ return b == 0; }));
    EXPECT_EQ(dec.get_uint32(), 4096u);  // count
}

TEST(ReaddirEncode, SubsequentCallPassesCookie) {
    std::array<uint8_t, 8> cv{1, 2, 3, 4, 5, 6, 7, 8};
    const auto args = nfs3::encode_readdir_args(make_fh({0x01}), 0xDEADULL, cv, 512u);

    XdrDecoder dec(args);
    dec.get_opaque();                       // fh3
    EXPECT_EQ(dec.get_uint64(), 0xDEADULL); // cookie
    const auto got_cv = dec.get_fixed_opaque(8);
    EXPECT_EQ(got_cv[0], 1u);
    EXPECT_EQ(got_cv[7], 8u);
    EXPECT_EQ(dec.get_uint32(), 512u);      // count
}

// ── READDIR decode ────────────────────────────────────────────────────────────

// Build a valid READDIR3resok reply with `entries` entries and eof.
static std::vector<uint8_t> make_readdir_reply(
        const std::vector<std::pair<uint64_t, std::string>>& entries,
        bool eof,
        std::array<uint8_t, 8> cookieverf = {}) {
    XdrEncoder enc;
    enc.put_uint32(0u);          // NFS3_OK
    append_no_attrs(enc);        // dir_attributes
    enc.put_fixed_opaque(cookieverf.data(), 8);  // cookieverf

    uint64_t cookie = 1;
    for (const auto& [fileid, name] : entries) {
        enc.put_uint32(1u);                  // value_follows = TRUE
        enc.put_uint64(fileid);
        enc.put_string(name);
        enc.put_uint64(cookie++);            // cookie
    }
    enc.put_uint32(0u);                      // value_follows = FALSE (end of list)
    enc.put_uint32(eof ? 1u : 0u);           // eof
    return enc.release();
}

TEST(ReaddirDecode, EmptyDirectoryEof) {
    const auto data = make_readdir_reply({}, true);
    const auto page = nfs3::decode_readdir_reply(data);
    EXPECT_TRUE(page.entries.empty());
    EXPECT_TRUE(page.eof);
}

TEST(ReaddirDecode, SingleEntryEof) {
    const auto data = make_readdir_reply({{100u, "hello.txt"}}, true);
    const auto page = nfs3::decode_readdir_reply(data);
    ASSERT_EQ(page.entries.size(), 1u);
    EXPECT_EQ(page.entries[0].fileid, 100u);
    EXPECT_EQ(page.entries[0].name, "hello.txt");
    EXPECT_EQ(page.entries[0].cookie, 1u);
    EXPECT_TRUE(page.eof);
}

TEST(ReaddirDecode, MultipleEntriesNotEof) {
    const auto data = make_readdir_reply(
        {{1u, "."}, {2u, ".."}, {42u, "subdir"}}, false);
    const auto page = nfs3::decode_readdir_reply(data);
    ASSERT_EQ(page.entries.size(), 3u);
    EXPECT_EQ(page.entries[0].name, ".");
    EXPECT_EQ(page.entries[1].name, "..");
    EXPECT_EQ(page.entries[2].name, "subdir");
    EXPECT_EQ(page.entries[2].fileid, 42u);
    EXPECT_FALSE(page.eof);
}

TEST(ReaddirDecode, CookieverfPreserved) {
    const std::array<uint8_t, 8> cv{0xDE, 0xAD, 0xBE, 0xEF, 1, 2, 3, 4};
    const auto data = make_readdir_reply({{1u, "a"}}, true, cv);
    const auto page = nfs3::decode_readdir_reply(data);
    EXPECT_EQ(page.cookieverf, cv);
}

TEST(ReaddirDecode, CookieOrderedSequentially) {
    // Verify the cookie on each entry increments (from make_readdir_reply)
    const auto data = make_readdir_reply({{10u, "a"}, {20u, "b"}, {30u, "c"}}, true);
    const auto page = nfs3::decode_readdir_reply(data);
    ASSERT_EQ(page.entries.size(), 3u);
    EXPECT_EQ(page.entries[0].cookie, 1u);
    EXPECT_EQ(page.entries[1].cookie, 2u);
    EXPECT_EQ(page.entries[2].cookie, 3u);
}

TEST(ReaddirDecode, NonZeroStatusThrowsNfsError) {
    XdrEncoder enc;
    enc.put_uint32(20u);     // NFS3ERR_NOTDIR
    append_no_attrs(enc);    // dir_attributes in resfail

    try {
        nfs3::decode_readdir_reply(enc.release());
        FAIL() << "Expected NfsError";
    } catch (const NfsError& e) {
        EXPECT_EQ(e.status, 20u);
        EXPECT_TRUE(e.is(Nfsstat3::NFS3ERR_NOTDIR));
    }
}
