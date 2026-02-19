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

// ── XDR helpers for NFS3 structures ─────────────────────────────────────────

inline void encode_fh3(XdrEncoder& enc, const Fh3& fh) {
    enc.put_opaque(fh.data);
}

inline Fh3 decode_fh3(XdrDecoder& dec) {
    return Fh3{dec.get_opaque()};
}

// Skip a post_op_attr (RFC 1813 §2.6):
//   bool(1) + optional fattr3(84 bytes = 21 uint32s)
inline void skip_post_op_attr(XdrDecoder& dec) {
    if (dec.get_uint32() != 0) {
        for (int i = 0; i < 21; ++i) dec.get_uint32();
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
