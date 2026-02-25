#pragma once

#include "../xdr/xdr.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

// NFSv4 file handle: variable-length opaque, max 128 bytes (RFC 7530 §4.2.1)
struct Nfs4Fh {
    std::vector<uint8_t> data;
    bool operator==(const Nfs4Fh& o) const { return data == o.data; }
};

// NFSv4 stateid4: seqid + 12-byte opaque (RFC 7530 §9.1.2)
struct Stateid4 {
    uint32_t              seqid{};
    std::array<uint8_t, 12> other{};
};

// nfstime4: seconds (int64) + nseconds (uint32) (RFC 7530 §6.2.5)
struct Nfstime4 {
    int64_t  seconds{};
    uint32_t nseconds{};
};

// ftype4 (RFC 7530 §5.3)
enum class Ftype4 : uint32_t {
    NF4REG       = 1,
    NF4DIR       = 2,
    NF4BLK       = 3,
    NF4CHR       = 4,
    NF4LNK       = 5,
    NF4SOCK      = 6,
    NF4FIFO      = 7,
    NF4ATTRDIR   = 8,
    NF4NAMEDATTR = 9,
};

// stable_how4 for WRITE (RFC 7530 §18.32)
enum class Stable4 : uint32_t {
    UNSTABLE  = 0,
    DATA_SYNC = 1,
    FILE_SYNC = 2,
};

// Decoded file attributes from GETATTR / READDIR (RFC 7530 §5)
// Fields are present only when the server returned them in the bitmap.
struct Fattr4 {
    std::optional<Ftype4>      type;
    std::optional<uint64_t>    change;
    std::optional<uint64_t>    size;
    std::optional<uint64_t>    fileid;
    std::optional<uint32_t>    mode;
    std::optional<uint32_t>    numlinks;
    std::optional<std::string> owner;
    std::optional<std::string> owner_group;
    std::optional<uint64_t>    space_used;
    std::optional<Nfstime4>    time_access;
    std::optional<Nfstime4>    time_metadata;
    std::optional<Nfstime4>    time_modify;
    std::optional<uint64_t>    mounted_on_fileid;
};

// Represents an open file in NFSv4 (holds file handle + stateid from OPEN)
struct Nfs4File {
    Nfs4Fh   fh;
    Stateid4 stateid;
    uint32_t seqid{};  // tracks the open seqid (needed for CLOSE)
};

// Result returned by nfs4::write()
struct Nfs4WriteResult {
    uint32_t               count{};
    Stable4                committed{};
    std::array<uint8_t, 8> verf{};  // writeverf4
};

// A directory entry from READDIR
struct Nfs4DirEntry {
    uint64_t    cookie{};
    std::string name;
    Fattr4      attrs;
};

// ── XDR helpers for NFSv4 structures ─────────────────────────────────────────

inline void encode_nfs4fh(XdrEncoder& enc, const Nfs4Fh& fh) {
    enc.put_opaque(fh.data);
}

inline Nfs4Fh decode_nfs4fh(XdrDecoder& dec) {
    return Nfs4Fh{dec.get_opaque()};
}

inline void encode_stateid4(XdrEncoder& enc, const Stateid4& sid) {
    enc.put_uint32(sid.seqid);
    enc.put_fixed_opaque(sid.other.data(), 12);
}

inline Stateid4 decode_stateid4(XdrDecoder& dec) {
    Stateid4 sid;
    sid.seqid    = dec.get_uint32();
    auto raw     = dec.get_fixed_opaque(12);
    std::copy(raw.begin(), raw.end(), sid.other.begin());
    return sid;
}

inline Nfstime4 decode_nfstime4(XdrDecoder& dec) {
    Nfstime4 t;
    t.seconds  = static_cast<int64_t>(dec.get_uint64());
    t.nseconds = dec.get_uint32();
    return t;
}

// change_info4: atomic(bool) + before(uint64) + after(uint64) — skip it
inline void skip_change_info4(XdrDecoder& dec) {
    dec.get_uint32();  // atomic
    dec.get_uint64();  // before
    dec.get_uint64();  // after
}
