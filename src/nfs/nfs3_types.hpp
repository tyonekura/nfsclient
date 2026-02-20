#pragma once

#include "../xdr/xdr.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

// NFSv3 file handle: variable-length opaque, max 64 bytes (RFC 1813 §2.5)
struct Fh3 {
    std::vector<uint8_t> data;
    bool operator==(const Fh3& o) const { return data == o.data; }
};

// stable_how enum (RFC 1813 §3.3.7)
enum class Stable3 : uint32_t {
    UNSTABLE  = 0,
    DATA_SYNC = 1,
    FILE_SYNC = 2,
};

// Result returned by nfs3::write()
struct WriteResult {
    uint32_t              count;
    Stable3               committed;
    std::array<uint8_t, 8> verf;  // writeverf3: fixed 8-byte opaque
};

// ftype3 (RFC 1813 §2.6)
enum class Ftype3 : uint32_t {
    NF3REG  = 1,
    NF3DIR  = 2,
    NF3BLK  = 3,
    NF3CHR  = 4,
    NF3LNK  = 5,
    NF3SOCK = 6,
    NF3FIFO = 7,
};

// nfstime3 (RFC 1813 §2.6): seconds + nseconds
struct Nfstime3 {
    uint32_t seconds;
    uint32_t nseconds;
};

// specdata3 (RFC 1813 §2.6): major/minor device numbers
struct Specdata3 {
    uint32_t specdata1;  // major
    uint32_t specdata2;  // minor
};

// fattr3 (RFC 1813 §2.6): file attributes returned by GETATTR, LOOKUP, etc.
// XDR wire size: 21 uint32s = 84 bytes.
struct Fattr3 {
    Ftype3    type;
    uint32_t  mode;
    uint32_t  nlink;
    uint32_t  uid;
    uint32_t  gid;
    uint64_t  size;
    uint64_t  used;
    Specdata3 rdev;
    uint64_t  fsid;
    uint64_t  fileid;
    Nfstime3  atime;
    Nfstime3  mtime;
    Nfstime3  ctime;
};

// How to set a time field in sattr3 (RFC 1813 §2.6)
enum class SetTimeHow : uint32_t {
    DONT_CHANGE       = 0,
    SET_TO_SERVER_TIME = 1,
    SET_TO_CLIENT_TIME = 2,
};

// sattr3 (RFC 1813 §2.6): settable attributes for CREATE, MKDIR, SETATTR.
// Each field has an associated flag; if false the field is omitted from the wire.
struct Sattr3 {
    bool     set_mode  = false; uint32_t   mode       = 0644;
    bool     set_uid   = false; uint32_t   uid        = 0;
    bool     set_gid   = false; uint32_t   gid        = 0;
    bool     set_size  = false; uint64_t   size       = 0;
    SetTimeHow set_atime = SetTimeHow::DONT_CHANGE;
    uint32_t atime_sec = 0; uint32_t atime_nsec = 0;
    SetTimeHow set_mtime = SetTimeHow::DONT_CHANGE;
    uint32_t mtime_sec = 0; uint32_t mtime_nsec = 0;
};

// ── XDR helpers for NFS3 structures ─────────────────────────────────────────

inline void encode_fh3(XdrEncoder& enc, const Fh3& fh) {
    enc.put_opaque(fh.data);
}

inline Fh3 decode_fh3(XdrDecoder& dec) {
    return Fh3{dec.get_opaque()};
}

inline Fattr3 decode_fattr3(XdrDecoder& dec) {
    Fattr3 a{};
    a.type            = static_cast<Ftype3>(dec.get_uint32());
    a.mode            = dec.get_uint32();
    a.nlink           = dec.get_uint32();
    a.uid             = dec.get_uint32();
    a.gid             = dec.get_uint32();
    a.size            = dec.get_uint64();
    a.used            = dec.get_uint64();
    a.rdev.specdata1  = dec.get_uint32();
    a.rdev.specdata2  = dec.get_uint32();
    a.fsid            = dec.get_uint64();
    a.fileid          = dec.get_uint64();
    a.atime.seconds   = dec.get_uint32();
    a.atime.nseconds  = dec.get_uint32();
    a.mtime.seconds   = dec.get_uint32();
    a.mtime.nseconds  = dec.get_uint32();
    a.ctime.seconds   = dec.get_uint32();
    a.ctime.nseconds  = dec.get_uint32();
    return a;
}

inline void encode_sattr3(XdrEncoder& enc, const Sattr3& s) {
    enc.put_uint32(s.set_mode ? 1u : 0u);
    if (s.set_mode) enc.put_uint32(s.mode);

    enc.put_uint32(s.set_uid ? 1u : 0u);
    if (s.set_uid) enc.put_uint32(s.uid);

    enc.put_uint32(s.set_gid ? 1u : 0u);
    if (s.set_gid) enc.put_uint32(s.gid);

    enc.put_uint32(s.set_size ? 1u : 0u);
    if (s.set_size) enc.put_uint64(s.size);

    enc.put_uint32(static_cast<uint32_t>(s.set_atime));
    if (s.set_atime == SetTimeHow::SET_TO_CLIENT_TIME) {
        enc.put_uint32(s.atime_sec);
        enc.put_uint32(s.atime_nsec);
    }

    enc.put_uint32(static_cast<uint32_t>(s.set_mtime));
    if (s.set_mtime == SetTimeHow::SET_TO_CLIENT_TIME) {
        enc.put_uint32(s.mtime_sec);
        enc.put_uint32(s.mtime_nsec);
    }
}

// Skip a post_op_attr (RFC 1813 §2.6):
//   bool(1) + optional fattr3(84 bytes = 21 uint32s)
inline void skip_post_op_attr(XdrDecoder& dec) {
    if (dec.get_uint32() != 0) {
        decode_fattr3(dec);  // read and discard
    }
}

// Skip a pre_op_attr (RFC 1813 §2.6):
//   bool(1) + optional wcc_attr: size(uint64) + mtime(nfstime3) + ctime(nfstime3) = 6 uint32s
inline void skip_pre_op_attr(XdrDecoder& dec) {
    if (dec.get_uint32() != 0) {
        for (int i = 0; i < 6; ++i) dec.get_uint32();
    }
}

// Skip wcc_data (pre_op_attr + post_op_attr)
inline void skip_wcc_data(XdrDecoder& dec) {
    skip_pre_op_attr(dec);
    skip_post_op_attr(dec);
}
