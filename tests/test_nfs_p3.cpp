// Unit tests for Phase 1 P3 items:
//   - READDIRPLUS encode/decode (inline attrs + file handles)
//   - READLINK / SYMLINK / LINK encode/decode
//   - MKNOD encode/decode (FIFO, socket, chr/blk device)
//   - EXPORT reply decode (linked list of export entries)

#include "nfs/nfs_error.hpp"
#include "nfs/nfs3_types.hpp"
#include "nfs/readdirplus.hpp"
#include "nfs/symlink.hpp"
#include "nfs/mknod.hpp"
#include "xdr/xdr.hpp"

#include <gtest/gtest.h>

// ── Helpers ──────────────────────────────────────────────────────────────────

static Fh3 make_fh(std::initializer_list<uint8_t> bytes) {
    return Fh3{std::vector<uint8_t>(bytes)};
}

static void append_no_attrs(XdrEncoder& enc) {
    enc.put_uint32(0u);  // attributes_follow = FALSE
}

static void append_no_wcc(XdrEncoder& enc) {
    enc.put_uint32(0u);
    enc.put_uint32(0u);
}

// Build a minimal fattr3 (21 uint32s) for a regular file.
static void append_fattr3(XdrEncoder& enc, Ftype3 type = Ftype3::NF3REG,
                            uint64_t size = 100) {
    enc.put_uint32(static_cast<uint32_t>(type));
    enc.put_uint32(0644u);           // mode
    enc.put_uint32(1u);              // nlink
    enc.put_uint32(1000u);           // uid
    enc.put_uint32(1000u);           // gid
    enc.put_uint64(size);            // size
    enc.put_uint64(size);            // used
    enc.put_uint32(0u); enc.put_uint32(0u); // rdev
    enc.put_uint64(1u);              // fsid
    enc.put_uint64(99u);             // fileid
    enc.put_uint32(0u); enc.put_uint32(0u); // atime
    enc.put_uint32(0u); enc.put_uint32(0u); // mtime
    enc.put_uint32(0u); enc.put_uint32(0u); // ctime
}

// ── READDIRPLUS encode ────────────────────────────────────────────────────────

TEST(ReaddirplusEncode, ArgsLayout) {
    const std::array<uint8_t, 8> cv{};
    const auto args = nfs3::encode_readdirplus_args(
        make_fh({0x01, 0x02, 0x03, 0x04}), 0ULL, cv, 4096u, 32768u);
    // fh(8) + cookie(8) + cookieverf(8) + dircount(4) + maxcount(4) = 32 bytes
    EXPECT_EQ(args.size(), 32u);

    XdrDecoder dec(args);
    dec.get_opaque();
    EXPECT_EQ(dec.get_uint64(), 0ULL);
    dec.get_fixed_opaque(8);
    EXPECT_EQ(dec.get_uint32(), 4096u);
    EXPECT_EQ(dec.get_uint32(), 32768u);
}

// ── READDIRPLUS decode ────────────────────────────────────────────────────────

static std::vector<uint8_t> make_readdirplus_reply(
        bool eof,
        bool with_attrs, bool with_fh,
        const std::string& name = "hello.txt",
        uint64_t fileid = 42u) {
    XdrEncoder enc;
    enc.put_uint32(0u);            // NFS3_OK
    append_no_attrs(enc);          // dir_attributes
    const std::array<uint8_t, 8> cv{1, 2, 3, 4, 5, 6, 7, 8};
    enc.put_fixed_opaque(cv.data(), 8);  // cookieverf

    // One entry
    enc.put_uint32(1u);            // value_follows = TRUE
    enc.put_uint64(fileid);
    enc.put_string(name);
    enc.put_uint64(1u);            // cookie

    // name_attributes
    enc.put_uint32(with_attrs ? 1u : 0u);
    if (with_attrs) append_fattr3(enc);

    // name_handle
    enc.put_uint32(with_fh ? 1u : 0u);
    if (with_fh) enc.put_opaque(std::vector<uint8_t>{0xAA, 0xBB});

    enc.put_uint32(0u);  // end of list
    enc.put_uint32(eof ? 1u : 0u);
    return enc.release();
}

TEST(ReaddirplusDecode, EntryWithAttrsAndFh) {
    const auto data = make_readdirplus_reply(true, true, true);
    const auto page = nfs3::decode_readdirplus_reply(data);

    ASSERT_EQ(page.entries.size(), 1u);
    const auto& e = page.entries[0];
    EXPECT_EQ(e.fileid, 42u);
    EXPECT_EQ(e.name, "hello.txt");
    EXPECT_EQ(e.cookie, 1u);
    EXPECT_TRUE(e.has_attrs);
    EXPECT_EQ(e.attrs.mode, 0644u);
    EXPECT_TRUE(e.has_fh);
    EXPECT_EQ(e.fh.data[0], 0xAAu);
    EXPECT_TRUE(page.eof);
}

TEST(ReaddirplusDecode, EntryWithoutAttrsOrFh) {
    const auto data = make_readdirplus_reply(false, false, false, "subdir", 7u);
    const auto page = nfs3::decode_readdirplus_reply(data);

    ASSERT_EQ(page.entries.size(), 1u);
    EXPECT_EQ(page.entries[0].fileid, 7u);
    EXPECT_FALSE(page.entries[0].has_attrs);
    EXPECT_FALSE(page.entries[0].has_fh);
    EXPECT_FALSE(page.eof);
}

TEST(ReaddirplusDecode, CookieverfPreserved) {
    const auto data = make_readdirplus_reply(true, false, false);
    const auto page = nfs3::decode_readdirplus_reply(data);
    EXPECT_EQ(page.cookieverf[0], 1u);
    EXPECT_EQ(page.cookieverf[7], 8u);
}

TEST(ReaddirplusDecode, NonZeroStatusThrowsNfsError) {
    XdrEncoder enc;
    enc.put_uint32(20u);   // NFS3ERR_NOTDIR
    append_no_attrs(enc);  // dir_attributes in resfail

    EXPECT_THROW(nfs3::decode_readdirplus_reply(enc.release()), NfsError);
}

// ── READLINK ─────────────────────────────────────────────────────────────────

TEST(ReadlinkEncode, ArgsIsJustFh) {
    const auto args = nfs3::encode_readlink_args(make_fh({0xDE, 0xAD}));
    // fh_len(4) + fh_data(2) + pad(2) = 8 bytes
    EXPECT_EQ(args.size(), 8u);
}

TEST(ReadlinkDecode, OkReturnsPath) {
    XdrEncoder enc;
    enc.put_uint32(0u);                   // NFS3_OK
    append_no_attrs(enc);                 // symlink_attributes
    enc.put_string("/usr/local/bin/sh");  // nfspath3

    EXPECT_EQ(nfs3::decode_readlink_reply(enc.release()), "/usr/local/bin/sh");
}

TEST(ReadlinkDecode, NonZeroStatusThrowsNfsError) {
    XdrEncoder enc;
    enc.put_uint32(22u);   // NFS3ERR_INVAL
    append_no_attrs(enc);

    EXPECT_THROW(nfs3::decode_readlink_reply(enc.release()), NfsError);
}

// ── SYMLINK ───────────────────────────────────────────────────────────────────

TEST(SymlinkEncode, ArgsLayout) {
    const auto args = nfs3::encode_symlink_args(
        make_fh({0x01, 0x02}), "mylink", "/etc/hosts");

    XdrDecoder dec(args);
    dec.get_opaque();
    EXPECT_EQ(dec.get_string(), "mylink");
    // sattr3 (all DONT_CHANGE = 24 bytes)
    for (int i = 0; i < 6; ++i) dec.get_uint32();
    EXPECT_EQ(dec.get_string(), "/etc/hosts");
}

TEST(SymlinkDecode, OkReturnsHandle) {
    const std::vector<uint8_t> fh_data = {0x11, 0x22, 0x33};

    XdrEncoder enc;
    enc.put_uint32(0u);          // NFS3_OK
    enc.put_uint32(1u);          // fh_present = TRUE
    enc.put_opaque(fh_data);     // new fh3
    append_no_attrs(enc);        // obj_attributes
    append_no_wcc(enc);          // dir_wcc

    const auto fh = nfs3::decode_symlink_reply(enc.release());
    EXPECT_EQ(fh.data, fh_data);
}

TEST(SymlinkDecode, NonZeroStatusThrowsNfsError) {
    XdrEncoder enc;
    enc.put_uint32(17u);  // NFS3ERR_EXIST
    // resfail: dir_wcc
    append_no_wcc(enc);

    EXPECT_THROW(nfs3::decode_symlink_reply(enc.release()), NfsError);
}

// ── LINK ──────────────────────────────────────────────────────────────────────

TEST(LinkEncode, ArgsLayout) {
    const auto args = nfs3::encode_link_args(
        make_fh({0x01}), make_fh({0x02}), "hardlink");

    XdrDecoder dec(args);
    dec.get_opaque();  // file fh
    dec.get_opaque();  // link_dir fh
    EXPECT_EQ(dec.get_string(), "hardlink");
}

TEST(LinkDecode, OkDoesNotThrow) {
    XdrEncoder enc;
    enc.put_uint32(0u);      // NFS3_OK
    append_no_attrs(enc);    // file_attributes
    append_no_wcc(enc);      // linkdir_wcc

    EXPECT_NO_THROW(nfs3::decode_link_reply(enc.release()));
}

TEST(LinkDecode, NonZeroStatusThrowsNfsError) {
    XdrEncoder enc;
    enc.put_uint32(18u);     // NFS3ERR_XDEV
    append_no_attrs(enc);
    append_no_wcc(enc);

    try {
        nfs3::decode_link_reply(enc.release());
        FAIL() << "Expected NfsError";
    } catch (const NfsError& e) {
        EXPECT_TRUE(e.is(Nfsstat3::NFS3ERR_XDEV));
    }
}

// ── MKNOD ─────────────────────────────────────────────────────────────────────

TEST(MknodEncode, FifoLayout) {
    const auto args = nfs3::encode_mknod_args(
        make_fh({0x01, 0x02}), "mypipe", Ftype3::NF3FIFO);

    XdrDecoder dec(args);
    dec.get_opaque();
    EXPECT_EQ(dec.get_string(), "mypipe");
    EXPECT_EQ(dec.get_uint32(), static_cast<uint32_t>(Ftype3::NF3FIFO));
    // sattr3 follows (24 bytes = 6 uint32s)
    EXPECT_EQ(dec.remaining(), 24u);
}

TEST(MknodEncode, DeviceIncludesSpecdata) {
    nfs3::DeviceSpec3 spec{8u, 1u};  // sda1
    const auto args = nfs3::encode_mknod_device_args(
        make_fh({0x01}), "sda1", Ftype3::NF3BLK, {}, spec);

    XdrDecoder dec(args);
    dec.get_opaque();
    EXPECT_EQ(dec.get_string(), "sda1");
    EXPECT_EQ(dec.get_uint32(), static_cast<uint32_t>(Ftype3::NF3BLK));
    // skip sattr3
    for (int i = 0; i < 6; ++i) dec.get_uint32();
    EXPECT_EQ(dec.get_uint32(), 8u);   // major
    EXPECT_EQ(dec.get_uint32(), 1u);   // minor
}

TEST(MknodDecode, OkReturnsHandle) {
    const std::vector<uint8_t> fh_data = {0xDE, 0xAD};

    XdrEncoder enc;
    enc.put_uint32(0u);           // NFS3_OK
    enc.put_uint32(1u);           // fh_present = TRUE
    enc.put_opaque(fh_data);
    append_no_attrs(enc);         // obj_attributes
    append_no_wcc(enc);           // dir_wcc

    const auto fh = nfs3::decode_mknod_reply(enc.release());
    EXPECT_EQ(fh.data, fh_data);
}

TEST(MknodDecode, NotSuppThrowsNfsError) {
    XdrEncoder enc;
    enc.put_uint32(10004u);  // NFS3ERR_NOTSUPP
    append_no_wcc(enc);

    try {
        nfs3::decode_mknod_reply(enc.release());
        FAIL() << "Expected NfsError";
    } catch (const NfsError& e) {
        EXPECT_TRUE(e.is(Nfsstat3::NFS3ERR_NOTSUPP));
    }
}

// ── EXPORT reply (decoded via mount.cpp internals via XDR) ────────────────────
// We test the XDR linked-list format directly without a live server.

TEST(ExportReply, LinkedListFormat) {
    // Build an EXPORT reply with two entries:
    //   /export  (no group restrictions)
    //   /data    (group: "trusted")
    XdrEncoder enc;
    // Entry 1
    enc.put_uint32(1u);            // value_follows = TRUE
    enc.put_string("/export");
    enc.put_uint32(0u);            // groups: end of list
    // Entry 2
    enc.put_uint32(1u);            // value_follows = TRUE
    enc.put_string("/data");
    enc.put_uint32(1u);            // groups: value_follows = TRUE
    enc.put_string("trusted");
    enc.put_uint32(0u);            // groups: end
    // End of exports
    enc.put_uint32(0u);

    // Replicate the decode logic from mount.cpp export_list()
    const auto buf = enc.release();
    XdrDecoder dec(buf);
    std::vector<std::pair<std::string, std::vector<std::string>>> result;
    while (dec.get_uint32() != 0) {
        std::string path = dec.get_string();
        std::vector<std::string> groups;
        while (dec.get_uint32() != 0)
            groups.push_back(dec.get_string());
        result.push_back({path, groups});
    }

    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(result[0].first, "/export");
    EXPECT_TRUE(result[0].second.empty());
    EXPECT_EQ(result[1].first, "/data");
    ASSERT_EQ(result[1].second.size(), 1u);
    EXPECT_EQ(result[1].second[0], "trusted");
}
