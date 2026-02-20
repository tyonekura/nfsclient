// Unit tests for Phase 1 additions:
//   - NfsError typed exception
//   - GETATTR encode/decode
//   - CREATE encode/decode (UNCHECKED, GUARDED, EXCLUSIVE)
//   - MKDIR / REMOVE / RMDIR encode/decode
//   - AUTH_SYS credential encoding (via buildCallMessage)
//   - Sattr3 XDR encoding

#include "nfs/nfs_error.hpp"
#include "nfs/nfs3_types.hpp"
#include "nfs/getattr.hpp"
#include "nfs/create.hpp"
#include "nfs/dirop.hpp"
#include "rpc/rpc_client.hpp"
#include "rpc/rpc_types.hpp"
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
    enc.put_uint32(0u);  // pre_op_attr: not present
    enc.put_uint32(0u);  // post_op_attr: not present
}

// Build a minimal but valid fattr3 into an encoder (21 uint32s).
static void append_fattr3(XdrEncoder& enc,
                            Ftype3 type = Ftype3::NF3REG,
                            uint32_t mode = 0644, uint64_t size = 1024) {
    enc.put_uint32(static_cast<uint32_t>(type)); // type
    enc.put_uint32(mode);                        // mode
    enc.put_uint32(1u);                          // nlink
    enc.put_uint32(1000u);                       // uid
    enc.put_uint32(1000u);                       // gid
    enc.put_uint64(size);                        // size
    enc.put_uint64(size);                        // used
    enc.put_uint32(0u); enc.put_uint32(0u);      // rdev specdata1/2
    enc.put_uint64(1u);                          // fsid
    enc.put_uint64(42u);                         // fileid
    enc.put_uint32(0u); enc.put_uint32(0u);      // atime
    enc.put_uint32(0u); enc.put_uint32(0u);      // mtime
    enc.put_uint32(0u); enc.put_uint32(0u);      // ctime
}

// ── NfsError ─────────────────────────────────────────────────────────────────

TEST(NfsError, CarriesStatusCode) {
    NfsError err(2u);
    EXPECT_EQ(err.status, 2u);
    EXPECT_TRUE(err.is(Nfsstat3::NFS3ERR_NOENT));
    EXPECT_FALSE(err.is(Nfsstat3::NFS3ERR_PERM));
}

TEST(NfsError, InheritsFromRuntimeError) {
    // Catching as std::runtime_error (backward compatibility)
    try {
        throw NfsError(13u, "LOOKUP");
    } catch (const std::runtime_error& e) {
        EXPECT_NE(std::string(e.what()).find("13"), std::string::npos);
    }
}

// ── Fattr3 decode ────────────────────────────────────────────────────────────

TEST(Fattr3Decode, RoundTrip) {
    XdrEncoder enc;
    append_fattr3(enc, Ftype3::NF3DIR, 0755, 4096);

    const auto buf = enc.release();
    XdrDecoder dec(buf);
    const auto a = decode_fattr3(dec);
    EXPECT_EQ(a.type, Ftype3::NF3DIR);
    EXPECT_EQ(a.mode, 0755u);
    EXPECT_EQ(a.size, 4096u);
    EXPECT_EQ(a.uid,  1000u);
    EXPECT_EQ(a.fileid, 42u);
}

// ── GETATTR ───────────────────────────────────────────────────────────────────

TEST(GetattrEncode, ArgsIsJustFh) {
    const auto args = nfs3::encode_getattr_args(make_fh({0xCA, 0xFE}));
    // fh_len(4) + fh_data(2) + padding(2) = 8 bytes
    EXPECT_EQ(args.size(), 8u);
    XdrDecoder dec(args);
    const auto fh_bytes = dec.get_opaque();
    ASSERT_EQ(fh_bytes.size(), 2u);
    EXPECT_EQ(fh_bytes[0], 0xCAu);
}

TEST(GetattrDecode, OkReturnsAttrs) {
    XdrEncoder enc;
    enc.put_uint32(0u);  // NFS3_OK
    append_fattr3(enc, Ftype3::NF3REG, 0600, 512);

    const auto a = nfs3::decode_getattr_reply(enc.release());
    EXPECT_EQ(a.type, Ftype3::NF3REG);
    EXPECT_EQ(a.mode, 0600u);
    EXPECT_EQ(a.size, 512u);
}

TEST(GetattrDecode, NonZeroStatusThrowsNfsError) {
    XdrEncoder enc;
    enc.put_uint32(70u);  // NFS3ERR_STALE

    try {
        nfs3::decode_getattr_reply(enc.release());
        FAIL() << "Expected NfsError";
    } catch (const NfsError& e) {
        EXPECT_EQ(e.status, 70u);
        EXPECT_TRUE(e.is(Nfsstat3::NFS3ERR_STALE));
    }
}

// ── Sattr3 encode ─────────────────────────────────────────────────────────────

TEST(Sattr3Encode, EmptyAttrsAllFalse) {
    XdrEncoder enc;
    Sattr3 s{};  // all false / DONT_CHANGE
    encode_sattr3(enc, s);
    const auto buf = enc.release();
    // set_mode(F) + set_uid(F) + set_gid(F) + set_size(F) +
    // set_atime(DONT_CHANGE=0) + set_mtime(DONT_CHANGE=0) = 6 uint32s = 24 bytes
    EXPECT_EQ(buf.size(), 24u);
    XdrDecoder dec(buf);
    for (int i = 0; i < 6; ++i) EXPECT_EQ(dec.get_uint32(), 0u);
}

TEST(Sattr3Encode, SetModeAndUid) {
    XdrEncoder enc;
    Sattr3 s{};
    s.set_mode = true; s.mode = 0755;
    s.set_uid  = true; s.uid  = 500;
    encode_sattr3(enc, s);

    const auto buf = enc.release();
    XdrDecoder dec(buf);
    EXPECT_EQ(dec.get_uint32(), 1u);     // set_mode = true
    EXPECT_EQ(dec.get_uint32(), 0755u);  // mode
    EXPECT_EQ(dec.get_uint32(), 1u);     // set_uid = true
    EXPECT_EQ(dec.get_uint32(), 500u);   // uid
    // rest are 0 (false / DONT_CHANGE)
}

// ── CREATE ────────────────────────────────────────────────────────────────────

TEST(CreateEncode, UncheckedArgsLayout) {
    Sattr3 attrs{};
    const auto args = nfs3::encode_create_args(make_fh({0x01, 0x02, 0x03, 0x04}),
                                                "newfile", nfs3::CreateMode3::UNCHECKED,
                                                attrs);
    XdrDecoder dec(args);
    dec.get_opaque();                    // fh3
    EXPECT_EQ(dec.get_string(), "newfile");
    EXPECT_EQ(dec.get_uint32(), static_cast<uint32_t>(nfs3::CreateMode3::UNCHECKED));
    // sattr3 follows (6 DONT_CHANGE fields = 24 bytes)
    EXPECT_EQ(dec.remaining(), 24u);
}

TEST(CreateEncode, ExclusiveCarriesVerf) {
    nfs3::CreateVerf3 verf;
    verf.data = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

    const auto args = nfs3::encode_create_args_exclusive(
        make_fh({0xAA, 0xBB}), "ex", verf);

    XdrDecoder dec(args);
    dec.get_opaque();                    // fh3
    EXPECT_EQ(dec.get_string(), "ex");
    EXPECT_EQ(dec.get_uint32(), static_cast<uint32_t>(nfs3::CreateMode3::EXCLUSIVE));
    const auto v = dec.get_fixed_opaque(8);
    EXPECT_EQ(v[0], 0x01u);
    EXPECT_EQ(v[7], 0x08u);
}

TEST(CreateDecode, OkReturnsFileHandle) {
    const std::vector<uint8_t> expected_fh = {0x11, 0x22, 0x33, 0x44};

    XdrEncoder enc;
    enc.put_uint32(0u);          // NFS3_OK
    enc.put_uint32(1u);          // fh_present = TRUE
    enc.put_opaque(expected_fh); // new fh3
    append_no_attrs(enc);        // obj_attributes
    append_no_wcc(enc);          // dir_wcc

    const auto fh = nfs3::decode_create_reply(enc.release());
    EXPECT_EQ(fh.data, expected_fh);
}

TEST(CreateDecode, NonZeroStatusThrowsNfsError) {
    XdrEncoder enc;
    enc.put_uint32(17u);   // NFS3ERR_EXIST
    append_no_wcc(enc);    // obj_wcc in resfail

    try {
        nfs3::decode_create_reply(enc.release());
        FAIL() << "Expected NfsError";
    } catch (const NfsError& e) {
        EXPECT_EQ(e.status, 17u);
        EXPECT_TRUE(e.is(Nfsstat3::NFS3ERR_EXIST));
    }
}

// ── MKDIR ─────────────────────────────────────────────────────────────────────

TEST(MkdirDecode, OkReturnsHandle) {
    const std::vector<uint8_t> dir_fh = {0xDD, 0xEE, 0xFF};

    XdrEncoder enc;
    enc.put_uint32(0u);          // NFS3_OK
    enc.put_uint32(1u);          // fh_present = TRUE
    enc.put_opaque(dir_fh);      // new dir fh3
    append_no_attrs(enc);        // obj_attributes
    append_no_wcc(enc);          // dir_wcc

    const auto fh = nfs3::decode_mkdir_reply(enc.release());
    EXPECT_EQ(fh.data, dir_fh);
}

TEST(MkdirDecode, NonZeroStatusThrowsNfsError) {
    XdrEncoder enc;
    enc.put_uint32(17u);  // NFS3ERR_EXIST
    append_no_wcc(enc);

    EXPECT_THROW(nfs3::decode_mkdir_reply(enc.release()), NfsError);
}

// ── REMOVE ────────────────────────────────────────────────────────────────────

TEST(RemoveDecode, OkDoesNotThrow) {
    XdrEncoder enc;
    enc.put_uint32(0u);    // NFS3_OK
    append_no_wcc(enc);    // dir_wcc

    EXPECT_NO_THROW(nfs3::decode_remove_reply(enc.release()));
}

TEST(RemoveDecode, NonZeroStatusThrowsNfsError) {
    XdrEncoder enc;
    enc.put_uint32(2u);   // NFS3ERR_NOENT
    append_no_wcc(enc);

    try {
        nfs3::decode_remove_reply(enc.release());
        FAIL() << "Expected NfsError";
    } catch (const NfsError& e) {
        EXPECT_TRUE(e.is(Nfsstat3::NFS3ERR_NOENT));
    }
}

// ── RMDIR ─────────────────────────────────────────────────────────────────────

TEST(RmdirDecode, OkDoesNotThrow) {
    XdrEncoder enc;
    enc.put_uint32(0u);
    append_no_wcc(enc);

    EXPECT_NO_THROW(nfs3::decode_rmdir_reply(enc.release()));
}

TEST(RmdirDecode, NonZeroStatusThrowsNfsError) {
    XdrEncoder enc;
    enc.put_uint32(66u);  // NFS3ERR_NOTEMPTY
    append_no_wcc(enc);

    try {
        nfs3::decode_rmdir_reply(enc.release());
        FAIL() << "Expected NfsError";
    } catch (const NfsError& e) {
        EXPECT_TRUE(e.is(Nfsstat3::NFS3ERR_NOTEMPTY));
    }
}

// ── AUTH_SYS credential encoding ─────────────────────────────────────────────

TEST(AuthSys, BuildCallMessageWithAuthSys) {
    AuthSys auth;
    auth.stamp = 0xDEAD;
    auth.machinename = "testhost";
    auth.uid  = 1001;
    auth.gid  = 1001;
    auth.gids = {100, 200};

    const auto msg = TcpRpcClient::buildCallMessage(42, 100003, 3, 6, {}, &auth);

    XdrDecoder dec(msg);
    EXPECT_EQ(dec.get_uint32(), 42u);   // xid
    EXPECT_EQ(dec.get_uint32(), 0u);    // CALL
    EXPECT_EQ(dec.get_uint32(), 2u);    // RPC version
    EXPECT_EQ(dec.get_uint32(), 100003u); // prog
    EXPECT_EQ(dec.get_uint32(), 3u);    // vers
    EXPECT_EQ(dec.get_uint32(), 6u);    // proc

    // Credential: flavor = AUTH_SYS (1)
    EXPECT_EQ(dec.get_uint32(), 1u);    // AUTH_SYS flavor
    const auto cred_bytes = dec.get_opaque();

    // Parse credential body
    XdrDecoder cred(cred_bytes);
    EXPECT_EQ(cred.get_uint32(), 0xDEADu);            // stamp
    EXPECT_EQ(cred.get_string(), "testhost");          // machinename
    EXPECT_EQ(cred.get_uint32(), 1001u);               // uid
    EXPECT_EQ(cred.get_uint32(), 1001u);               // gid
    EXPECT_EQ(cred.get_uint32(), 2u);                  // gids count
    EXPECT_EQ(cred.get_uint32(), 100u);                // gids[0]
    EXPECT_EQ(cred.get_uint32(), 200u);                // gids[1]

    // Verifier must be AUTH_NONE
    EXPECT_EQ(dec.get_uint32(), 0u);    // AUTH_NONE flavor
    EXPECT_EQ(dec.get_uint32(), 0u);    // body_len = 0
}

TEST(AuthSys, NullAuthProducesAuthNone) {
    const auto msg = TcpRpcClient::buildCallMessage(1, 100003, 3, 1, {}, nullptr);

    XdrDecoder dec(msg);
    dec.get_uint32(); // xid
    dec.get_uint32(); // CALL
    dec.get_uint32(); // RPC version
    dec.get_uint32(); // prog
    dec.get_uint32(); // vers
    dec.get_uint32(); // proc

    EXPECT_EQ(dec.get_uint32(), 0u);  // AUTH_NONE flavor
    EXPECT_EQ(dec.get_uint32(), 0u);  // body_len = 0
}
