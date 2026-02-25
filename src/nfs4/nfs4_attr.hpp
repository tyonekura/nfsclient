#pragma once

#include "nfs4_types.hpp"
#include "../xdr/xdr.hpp"

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <vector>

namespace nfs4 {

// ── Attribute IDs (RFC 7530 §5.8) ────────────────────────────────────────────
namespace attr {
    constexpr uint32_t TYPE              = 1;
    constexpr uint32_t CHANGE            = 3;
    constexpr uint32_t SIZE              = 4;
    constexpr uint32_t FSID              = 8;
    constexpr uint32_t FILEID            = 20;
    constexpr uint32_t MODE              = 33;
    constexpr uint32_t NUMLINKS          = 35;
    constexpr uint32_t OWNER             = 36;
    constexpr uint32_t OWNER_GROUP       = 37;
    constexpr uint32_t SPACE_USED        = 45;
    constexpr uint32_t TIME_ACCESS       = 47;
    constexpr uint32_t TIME_METADATA     = 52;
    constexpr uint32_t TIME_MODIFY       = 53;
    constexpr uint32_t MOUNTED_ON_FILEID = 55;
    constexpr uint32_t TIME_ACCESS_SET   = 64;
    constexpr uint32_t TIME_MODIFY_SET   = 65;
}  // namespace attr

// ── Bitmap4 helpers ───────────────────────────────────────────────────────────

// Attribute N lives in word N/32, bit (1u << (31 - N%32)) — big-endian within word.
void bitmap4_set(std::vector<uint32_t>& bm, uint32_t id);
bool bitmap4_test(const std::vector<uint32_t>& bm, uint32_t id);

void                  encode_bitmap4(XdrEncoder& enc, const std::vector<uint32_t>& bm);
std::vector<uint32_t> decode_bitmap4(XdrDecoder& dec);

std::vector<uint32_t> make_bitmap4(std::initializer_list<uint32_t> ids);

// Encode a GETATTR/READDIR attr request bitmap.
void encode_attr_request(XdrEncoder& enc, std::initializer_list<uint32_t> ids);

// ── fattr4 decode ─────────────────────────────────────────────────────────────

// Decode a server-returned fattr4 (bitmap + opaque attrlist) into Fattr4.
Fattr4 decode_fattr4(XdrDecoder& dec);

// ── fattr4 encode (for SETATTR / CREATE) ─────────────────────────────────────

// Settable attributes for SETATTR / CREATE.
struct Sattr4 {
    std::optional<uint64_t>    size;
    std::optional<uint32_t>    mode;
    std::optional<std::string> owner;
    std::optional<std::string> owner_group;
    std::optional<Nfstime4>    time_access;   // SET_TO_CLIENT_TIME if set
    std::optional<Nfstime4>    time_modify;   // SET_TO_CLIENT_TIME if set
};

// Encode fattr4 (bitmap4 + opaque attrlist) for SETATTR/CREATE args.
void encode_fattr4(XdrEncoder& enc, const Sattr4& attrs);

}  // namespace nfs4
