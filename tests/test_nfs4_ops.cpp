#include "nfs4/compound.hpp"
#include "nfs4/fh_ops.hpp"
#include "nfs4/setclientid.hpp"
#include "nfs4/lookup.hpp"
#include "nfs4/getattr.hpp"
#include "nfs4/access.hpp"
#include "nfs4/open.hpp"
#include "nfs4/read.hpp"
#include "nfs4/write.hpp"
#include "nfs4/commit.hpp"
#include "nfs4/dirop.hpp"
#include "nfs4/setattr.hpp"
#include "nfs4/create.hpp"
#include "nfs4/readdir.hpp"
#include "nfs4/readlink.hpp"
#include "nfs4/nfs4_types.hpp"
#include "nfs4/nfs4_attr.hpp"
#include "xdr/xdr.hpp"

#include <gtest/gtest.h>
#include <array>
#include <cstdint>
#include <vector>

using namespace nfs4;

// ── Helpers ───────────────────────────────────────────────────────────────────

static void append_u32(std::vector<uint8_t>& buf, uint32_t v) {
    buf.push_back(v >> 24); buf.push_back(v >> 16);
    buf.push_back(v >> 8);  buf.push_back(v);
}

static void append_u64(std::vector<uint8_t>& buf, uint64_t v) {
    append_u32(buf, static_cast<uint32_t>(v >> 32));
    append_u32(buf, static_cast<uint32_t>(v));
}

static void append_str(std::vector<uint8_t>& buf, const std::string& s) {
    append_u32(buf, static_cast<uint32_t>(s.size()));
    for (char c : s) buf.push_back(static_cast<uint8_t>(c));
    // XDR padding to 4 bytes
    size_t pad = (4 - s.size() % 4) % 4;
    for (size_t i = 0; i < pad; ++i) buf.push_back(0);
}

static void append_opaque(std::vector<uint8_t>& buf, const std::vector<uint8_t>& data) {
    append_u32(buf, static_cast<uint32_t>(data.size()));
    buf.insert(buf.end(), data.begin(), data.end());
    size_t pad = (4 - data.size() % 4) % 4;
    for (size_t i = 0; i < pad; ++i) buf.push_back(0);
}

static void append_fixed(std::vector<uint8_t>& buf, const uint8_t* data, size_t n) {
    buf.insert(buf.end(), data, data + n);
    size_t pad = (4 - n % 4) % 4;
    for (size_t i = 0; i < pad; ++i) buf.push_back(0);
}

// ── LOOKUP ────────────────────────────────────────────────────────────────────

TEST(Nfs4Ops, LookupEncode) {
    XdrEncoder enc;
    encode_lookup(enc, "hello.txt");
    const auto& b = enc.bytes();

    // OP_LOOKUP(15) + string("hello.txt" = 9 chars + 3 pad = 12)
    EXPECT_EQ(b[3], 15u);  // OP_LOOKUP
    // string length = 9
    EXPECT_EQ(b[7], 9u);
}

TEST(Nfs4Ops, LookupDecodeOk) {
    std::vector<uint8_t> reply;
    append_u32(reply, 15);  // OP_LOOKUP
    append_u32(reply, 0);   // NFS4_OK

    XdrDecoder dec(reply);
    EXPECT_NO_THROW(decode_lookup_result(dec));
}

TEST(Nfs4Ops, LookupDecodeError) {
    std::vector<uint8_t> reply;
    append_u32(reply, 15);  // OP_LOOKUP
    append_u32(reply, 2);   // NFS4ERR_NOENT

    XdrDecoder dec(reply);
    try {
        decode_lookup_result(dec);
        FAIL();
    } catch (const Nfs4Error& e) {
        EXPECT_EQ(e.status, 2u);
    }
}

// ── GETATTR ───────────────────────────────────────────────────────────────────

TEST(Nfs4Ops, GetAttrEncode) {
    XdrEncoder enc;
    encode_getattr(enc, {attr::TYPE, attr::SIZE});
    const auto& b = enc.bytes();

    EXPECT_EQ(b[3], 9u);   // OP_GETATTR = 9
}

TEST(Nfs4Ops, GetAttrDecodeOk) {
    // Build a reply: [resop=9][status=0][fattr4: bm with SIZE][attrlist: uint64(512)]
    uint32_t bm0 = 1u << (attr::SIZE % 32);  // SIZE=4: bit 4 = 0x00000010

    std::vector<uint8_t> attrlist;
    append_u64(attrlist, 512);

    std::vector<uint8_t> reply;
    append_u32(reply, 9);   // OP_GETATTR
    append_u32(reply, 0);   // NFS4_OK
    append_u32(reply, 1);   // num_words = 1
    append_u32(reply, bm0);
    append_opaque(reply, attrlist);

    XdrDecoder dec(reply);
    Fattr4 attrs = decode_getattr_result(dec);
    ASSERT_TRUE(attrs.size.has_value());
    EXPECT_EQ(*attrs.size, 512u);
}

// ── ACCESS ────────────────────────────────────────────────────────────────────

TEST(Nfs4Ops, AccessEncode) {
    XdrEncoder enc;
    encode_access(enc, ACCESS4_READ | ACCESS4_LOOKUP);
    const auto& b = enc.bytes();

    EXPECT_EQ(b[3], 3u);   // OP_ACCESS = 3
    // access mask = 3
    EXPECT_EQ(b[7], 3u);
}

TEST(Nfs4Ops, AccessDecodeOk) {
    std::vector<uint8_t> reply;
    append_u32(reply, 3);           // OP_ACCESS
    append_u32(reply, 0);           // NFS4_OK
    append_u32(reply, 0x3Fu);       // supported = all
    append_u32(reply, ACCESS4_READ);

    XdrDecoder dec(reply);
    auto r = decode_access_result(dec);
    EXPECT_EQ(r.supported, 0x3Fu);
    EXPECT_EQ(r.access, ACCESS4_READ);
}

// ── OPEN (NOCREATE) ───────────────────────────────────────────────────────────

TEST(Nfs4Ops, OpenNocreateEncode) {
    XdrEncoder enc;
    encode_open_nocreate(enc, 1, OPEN4_SHARE_ACCESS_READ, 0xDEAD, "owner", "file.txt");
    const auto& b = enc.bytes();

    EXPECT_EQ(b[3], 18u);  // OP_OPEN = 18
}

TEST(Nfs4Ops, OpenDecodeOkNoConfirm) {
    // Build OPEN4resok: stateid4 + change_info4 + rflags + attrset + delegation=NONE
    std::vector<uint8_t> reply;
    append_u32(reply, 18);  // OP_OPEN
    append_u32(reply, 0);   // NFS4_OK

    // stateid4: seqid=1 + 12 zero bytes
    append_u32(reply, 1);
    for (int i = 0; i < 12; ++i) reply.push_back(0);

    // change_info4: atomic(bool) + before(u64) + after(u64)
    append_u32(reply, 1);   // atomic = true
    append_u64(reply, 100);
    append_u64(reply, 101);

    // rflags = 0 (no confirm required)
    append_u32(reply, 0);

    // attrset: num_words=0
    append_u32(reply, 0);

    // open_delegation4: OPEN_DELEGATE_NONE = 0
    append_u32(reply, 0);

    XdrDecoder dec(reply);
    Open4Result r = decode_open_result(dec);
    EXPECT_EQ(r.stateid.seqid, 1u);
    EXPECT_EQ(r.rflags, 0u);
}

// ── OPEN_CONFIRM ──────────────────────────────────────────────────────────────

TEST(Nfs4Ops, OpenConfirmEncode) {
    Stateid4 sid;
    sid.seqid = 1;
    XdrEncoder enc;
    encode_open_confirm(enc, sid, 2);
    const auto& b = enc.bytes();

    EXPECT_EQ(b[3], 20u);  // OP_OPEN_CONFIRM = 20
}

TEST(Nfs4Ops, OpenConfirmDecodeOk) {
    std::vector<uint8_t> reply;
    append_u32(reply, 20);  // OP_OPEN_CONFIRM
    append_u32(reply, 0);   // NFS4_OK
    // stateid4: seqid=2 + 12 zero bytes
    append_u32(reply, 2);
    for (int i = 0; i < 12; ++i) reply.push_back(0);

    XdrDecoder dec(reply);
    Stateid4 sid = decode_open_confirm_result(dec);
    EXPECT_EQ(sid.seqid, 2u);
}

// ── CLOSE ─────────────────────────────────────────────────────────────────────

TEST(Nfs4Ops, CloseEncode) {
    Stateid4 sid;
    sid.seqid = 3;
    XdrEncoder enc;
    encode_close(enc, 3, sid);
    const auto& b = enc.bytes();

    EXPECT_EQ(b[3], 4u);  // OP_CLOSE = 4
}

TEST(Nfs4Ops, CloseDecodeOk) {
    std::vector<uint8_t> reply;
    append_u32(reply, 4);  // OP_CLOSE
    append_u32(reply, 0);  // NFS4_OK
    // returned stateid (all zeros after close)
    append_u32(reply, 0);
    for (int i = 0; i < 12; ++i) reply.push_back(0);

    XdrDecoder dec(reply);
    EXPECT_NO_THROW(decode_close_result(dec));
}

// ── READ ──────────────────────────────────────────────────────────────────────

TEST(Nfs4Ops, ReadEncode) {
    Stateid4 sid;
    XdrEncoder enc;
    encode_read(enc, sid, 0, 4096);
    const auto& b = enc.bytes();

    EXPECT_EQ(b[3], 25u);  // OP_READ = 25
}

TEST(Nfs4Ops, ReadDecodeOk) {
    std::vector<uint8_t> data_bytes = {0xAA, 0xBB, 0xCC, 0xDD};
    std::vector<uint8_t> reply;
    append_u32(reply, 25);  // OP_READ
    append_u32(reply, 0);   // NFS4_OK
    append_u32(reply, 0);   // eof = false
    append_opaque(reply, data_bytes);

    XdrDecoder dec(reply);
    auto data = decode_read_result(dec);
    ASSERT_EQ(data.size(), 4u);
    EXPECT_EQ(data[0], 0xAA);
    EXPECT_EQ(data[3], 0xDD);
}

// ── WRITE ─────────────────────────────────────────────────────────────────────

TEST(Nfs4Ops, WriteEncode) {
    Stateid4 sid;
    const uint8_t buf[] = {1, 2, 3, 4};
    XdrEncoder enc;
    encode_write(enc, sid, 0, Stable4::FILE_SYNC, buf, 4);
    const auto& b = enc.bytes();

    EXPECT_EQ(b[3], 38u);  // OP_WRITE = 38
}

TEST(Nfs4Ops, WriteDecodeOk) {
    std::vector<uint8_t> reply;
    append_u32(reply, 38);  // OP_WRITE
    append_u32(reply, 0);   // NFS4_OK
    append_u32(reply, 512); // count
    append_u32(reply, 2);   // FILE_SYNC
    // writeverf4: 8 fixed bytes
    const uint8_t verf[8] = {1,2,3,4,5,6,7,8};
    append_fixed(reply, verf, 8);

    XdrDecoder dec(reply);
    auto r = decode_write_result(dec);
    EXPECT_EQ(r.count, 512u);
    EXPECT_EQ(r.committed, Stable4::FILE_SYNC);
    EXPECT_EQ(r.verf[0], 1u);
    EXPECT_EQ(r.verf[7], 8u);
}

// ── COMMIT ────────────────────────────────────────────────────────────────────

TEST(Nfs4Ops, CommitEncode) {
    XdrEncoder enc;
    encode_commit(enc, 0, 0);
    const auto& b = enc.bytes();

    EXPECT_EQ(b[3], 5u);  // OP_COMMIT = 5
}

TEST(Nfs4Ops, CommitDecodeOk) {
    std::vector<uint8_t> reply;
    append_u32(reply, 5);   // OP_COMMIT
    append_u32(reply, 0);   // NFS4_OK
    const uint8_t verf[8] = {0,1,2,3,4,5,6,7};
    append_fixed(reply, verf, 8);

    XdrDecoder dec(reply);
    auto v = decode_commit_result(dec);
    EXPECT_EQ(v[0], 0u);
    EXPECT_EQ(v[7], 7u);
}

// ── REMOVE ────────────────────────────────────────────────────────────────────

TEST(Nfs4Ops, RemoveEncode) {
    XdrEncoder enc;
    encode_remove(enc, "victim.txt");
    const auto& b = enc.bytes();

    EXPECT_EQ(b[3], 28u);  // OP_REMOVE = 28
}

TEST(Nfs4Ops, RemoveDecodeOk) {
    std::vector<uint8_t> reply;
    append_u32(reply, 28);  // OP_REMOVE
    append_u32(reply, 0);   // NFS4_OK
    // change_info4: atomic + before + after
    append_u32(reply, 1);
    append_u64(reply, 10);
    append_u64(reply, 11);

    XdrDecoder dec(reply);
    EXPECT_NO_THROW(decode_remove_result(dec));
}

// ── RENAME ────────────────────────────────────────────────────────────────────

TEST(Nfs4Ops, RenameEncode) {
    XdrEncoder enc;
    encode_rename(enc, "old.txt", "new.txt");
    const auto& b = enc.bytes();

    EXPECT_EQ(b[3], 29u);  // OP_RENAME = 29
}

TEST(Nfs4Ops, RenameDecodeOk) {
    std::vector<uint8_t> reply;
    append_u32(reply, 29);  // OP_RENAME
    append_u32(reply, 0);   // NFS4_OK
    // source_cinfo
    append_u32(reply, 1); append_u64(reply, 1); append_u64(reply, 2);
    // target_cinfo
    append_u32(reply, 1); append_u64(reply, 3); append_u64(reply, 4);

    XdrDecoder dec(reply);
    EXPECT_NO_THROW(decode_rename_result(dec));
}

// ── SETATTR ───────────────────────────────────────────────────────────────────

TEST(Nfs4Ops, SetAttrEncode) {
    Stateid4 sid;
    Sattr4 s;
    s.size = 0;
    XdrEncoder enc;
    encode_setattr(enc, sid, s);
    const auto& b = enc.bytes();

    EXPECT_EQ(b[3], 34u);  // OP_SETATTR = 34
}

TEST(Nfs4Ops, SetAttrDecodeOk) {
    std::vector<uint8_t> reply;
    append_u32(reply, 34);  // OP_SETATTR
    append_u32(reply, 0);   // NFS4_OK
    append_u32(reply, 0);   // attrsset: num_words=0

    XdrDecoder dec(reply);
    EXPECT_NO_THROW(decode_setattr_result(dec));
}

// ── CREATE (dir) ──────────────────────────────────────────────────────────────

TEST(Nfs4Ops, CreateDirEncode) {
    XdrEncoder enc;
    encode_create_dir(enc, "newdir");
    const auto& b = enc.bytes();

    EXPECT_EQ(b[3], 6u);   // OP_CREATE = 6
    // createtype4 = NF4DIR = 2
    EXPECT_EQ(b[7], 2u);
}

TEST(Nfs4Ops, CreateDecodeOk) {
    std::vector<uint8_t> reply;
    append_u32(reply, 6);   // OP_CREATE
    append_u32(reply, 0);   // NFS4_OK
    // cinfo
    append_u32(reply, 1); append_u64(reply, 5); append_u64(reply, 6);
    // attrset: num_words=0
    append_u32(reply, 0);

    XdrDecoder dec(reply);
    EXPECT_NO_THROW(decode_create_result(dec));
}

// ── READLINK ──────────────────────────────────────────────────────────────────

TEST(Nfs4Ops, ReadlinkEncode) {
    XdrEncoder enc;
    encode_readlink(enc);
    const auto& b = enc.bytes();

    ASSERT_EQ(b.size(), 4u);
    EXPECT_EQ(b[3], 27u);  // OP_READLINK = 27
}

TEST(Nfs4Ops, ReadlinkDecodeOk) {
    std::vector<uint8_t> reply;
    append_u32(reply, 27);  // OP_READLINK
    append_u32(reply, 0);   // NFS4_OK
    append_str(reply, "/target/path");

    XdrDecoder dec(reply);
    std::string target = decode_readlink_result(dec);
    EXPECT_EQ(target, "/target/path");
}

// ── READDIR ───────────────────────────────────────────────────────────────────

TEST(Nfs4Ops, ReaddirEncode) {
    std::array<uint8_t, 8> cv{};
    XdrEncoder enc;
    encode_readdir(enc, 0, cv, 4096, 32768, {attr::TYPE, attr::FILEID});
    const auto& b = enc.bytes();

    EXPECT_EQ(b[3], 26u);  // OP_READDIR = 26
}

TEST(Nfs4Ops, ReaddirDecodeEmptyEof) {
    // Build a minimal READDIR4resok: cookieverf + no entries + eof=true
    std::vector<uint8_t> reply;
    append_u32(reply, 26);  // OP_READDIR
    append_u32(reply, 0);   // NFS4_OK

    // cookieverf: 8 zero bytes (fixed opaque, no length prefix)
    for (int i = 0; i < 8; ++i) reply.push_back(0);

    // dirlist4: value_follows=0 (no entries), eof=1
    append_u32(reply, 0);  // value_follows = 0 (end of list)
    append_u32(reply, 1);  // eof = true

    XdrDecoder dec(reply);
    ReaddirPage4 page = decode_readdir_result(dec);
    EXPECT_TRUE(page.eof);
    EXPECT_TRUE(page.entries.empty());
}

// ── SETCLIENTID ───────────────────────────────────────────────────────────────

TEST(Nfs4Ops, SetclientidEncode) {
    std::array<uint8_t, 8> verf{1,2,3,4,5,6,7,8};
    XdrEncoder enc;
    encode_setclientid(enc, verf, "test-client", 0);
    const auto& b = enc.bytes();

    EXPECT_EQ(b[3], 35u);  // OP_SETCLIENTID = 35
    // verifier bytes start at offset 4
    EXPECT_EQ(b[4], 1u);
    EXPECT_EQ(b[11], 8u);
}

TEST(Nfs4Ops, SetclientidDecodeOk) {
    std::vector<uint8_t> reply;
    append_u32(reply, 35);  // OP_SETCLIENTID
    append_u32(reply, 0);   // NFS4_OK
    append_u64(reply, 0xABCDEF01);  // clientid
    // confirm_verifier: 8 bytes (fixed)
    const uint8_t cv[8] = {9,8,7,6,5,4,3,2};
    append_fixed(reply, cv, 8);

    XdrDecoder dec(reply);
    auto r = decode_setclientid_result(dec);
    EXPECT_EQ(r.clientid, 0xABCDEF01u);
    EXPECT_EQ(r.confirm_verifier[0], 9u);
    EXPECT_EQ(r.confirm_verifier[7], 2u);
}

TEST(Nfs4Ops, SetclientidConfirmEncode) {
    std::array<uint8_t, 8> cv{1,2,3,4,5,6,7,8};
    XdrEncoder enc;
    encode_setclientid_confirm(enc, 42, cv);
    const auto& b = enc.bytes();

    EXPECT_EQ(b[3], 36u);  // OP_SETCLIENTID_CONFIRM = 36
}

TEST(Nfs4Ops, SetclientidConfirmDecodeOk) {
    std::vector<uint8_t> reply;
    append_u32(reply, 36);  // OP_SETCLIENTID_CONFIRM
    append_u32(reply, 0);   // NFS4_OK

    XdrDecoder dec(reply);
    EXPECT_NO_THROW(decode_setclientid_confirm_result(dec));
}

// ── RENEW ─────────────────────────────────────────────────────────────────────

TEST(Nfs4Ops, RenewEncode) {
    XdrEncoder enc;
    encode_renew(enc, 0xDEADBEEF);
    const auto& b = enc.bytes();

    EXPECT_EQ(b[3], 30u);  // OP_RENEW = 30
}

TEST(Nfs4Ops, RenewDecodeOk) {
    std::vector<uint8_t> reply;
    append_u32(reply, 30);  // OP_RENEW
    append_u32(reply, 0);   // NFS4_OK

    XdrDecoder dec(reply);
    EXPECT_NO_THROW(decode_renew_result(dec));
}

// ── SAVEFH / RESTOREFH / LOOKUPP ─────────────────────────────────────────────

TEST(Nfs4Ops, SavefhEncode) {
    XdrEncoder enc;
    encode_savefh(enc);
    EXPECT_EQ(enc.bytes()[3], 32u);  // OP_SAVEFH = 32
}

TEST(Nfs4Ops, RestorefhEncode) {
    XdrEncoder enc;
    encode_restorefh(enc);
    EXPECT_EQ(enc.bytes()[3], 31u);  // OP_RESTOREFH = 31
}

TEST(Nfs4Ops, LookuppEncode) {
    XdrEncoder enc;
    encode_lookupp(enc);
    EXPECT_EQ(enc.bytes()[3], 16u);  // OP_LOOKUPP = 16
}

TEST(Nfs4Ops, SavefhDecodeOk) {
    std::vector<uint8_t> reply;
    append_u32(reply, 32); append_u32(reply, 0);
    XdrDecoder dec(reply);
    EXPECT_NO_THROW(decode_savefh_result(dec));
}

TEST(Nfs4Ops, RestorefhDecodeOk) {
    std::vector<uint8_t> reply;
    append_u32(reply, 31); append_u32(reply, 0);
    XdrDecoder dec(reply);
    EXPECT_NO_THROW(decode_restorefh_result(dec));
}
