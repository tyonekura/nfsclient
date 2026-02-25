#pragma once

#include "nfs4_types.hpp"
#include "nfs4_attr.hpp"
#include "nfs4_error.hpp"
#include "../xdr/xdr.hpp"

#include <cstdint>
#include <string>

namespace nfs4 {

// share_access flags (RFC 7530 §18.16)
constexpr uint32_t OPEN4_SHARE_ACCESS_READ  = 1;
constexpr uint32_t OPEN4_SHARE_ACCESS_WRITE = 2;
constexpr uint32_t OPEN4_SHARE_ACCESS_BOTH  = 3;
constexpr uint32_t OPEN4_SHARE_DENY_NONE    = 0;

// opentype4
constexpr uint32_t OPEN4_NOCREATE = 0;
constexpr uint32_t OPEN4_CREATE   = 1;

// createmode4
constexpr uint32_t UNCHECKED4 = 0;
constexpr uint32_t GUARDED4   = 1;
constexpr uint32_t EXCLUSIVE4 = 2;

// open_claim_type4
constexpr uint32_t CLAIM_NULL = 0;

// rflags
constexpr uint32_t OPEN4_RESULT_CONFIRM    = 2;
constexpr uint32_t OPEN4_RESULT_LOCKTYPE_POSIX = 4;

// Result of OPEN4
struct Open4Result {
    Stateid4 stateid;
    uint32_t rflags{};
};

// Encode OPEN op — NOCREATE (open existing file by name).
void encode_open_nocreate(XdrEncoder& enc,
                          uint32_t seqid,
                          uint32_t share_access,
                          uint64_t clientid,
                          const std::string& owner,
                          const std::string& name);

// Encode OPEN op — CREATE with UNCHECKED mode (create or truncate).
void encode_open_create(XdrEncoder& enc,
                        uint32_t seqid,
                        uint32_t share_access,
                        uint64_t clientid,
                        const std::string& owner,
                        const std::string& name,
                        const Sattr4& attrs = {});

// Decode OPEN per-op result.
Open4Result decode_open_result(XdrDecoder& dec);

// Encode OPEN_CONFIRM op.
void encode_open_confirm(XdrEncoder& enc, const Stateid4& stateid, uint32_t seqid);

// Decode OPEN_CONFIRM per-op result — returns confirmed stateid.
Stateid4 decode_open_confirm_result(XdrDecoder& dec);

// Encode CLOSE op.
void encode_close(XdrEncoder& enc, uint32_t seqid, const Stateid4& stateid);

// Decode CLOSE per-op result.
void decode_close_result(XdrDecoder& dec);

// Encode RENEW op.
void encode_renew(XdrEncoder& enc, uint64_t clientid);

// Decode RENEW per-op result.
void decode_renew_result(XdrDecoder& dec);

}  // namespace nfs4
