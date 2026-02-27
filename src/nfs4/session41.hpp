#pragma once

#include "nfs4_error.hpp"
#include "nfs4_types.hpp"
#include "../xdr/xdr.hpp"

#include <array>
#include <cstdint>
#include <string>

namespace nfs4 {

// Result of EXCHANGE_ID (RFC 8881 §18.35)
struct ExchangeIdResult {
    uint64_t clientid{};
    uint32_t sequenceid{};  // used as csa_sequence in CREATE_SESSION
};

// Encode EXCHANGE_ID op into `enc`.
//   verifier  — 8-byte client-supplied boot verifier
//   client_id — unique owner string identifying this client instance
void encode_exchange_id(XdrEncoder& enc,
                        const std::array<uint8_t, 8>& verifier,
                        const std::string& client_id);

// Decode EXCHANGE_ID per-op result.
ExchangeIdResult decode_exchange_id_result(XdrDecoder& dec);

// Encode CREATE_SESSION op into `enc` (RFC 8881 §18.36).
//   clientid   — from EXCHANGE_ID response
//   sequenceid — eir_sequenceid from EXCHANGE_ID response
void encode_create_session(XdrEncoder& enc,
                           uint64_t clientid,
                           uint32_t sequenceid);

// Decode CREATE_SESSION per-op result; returns the 16-byte session ID.
SessionId41 decode_create_session_result(XdrDecoder& dec);

// Encode SEQUENCE op into `enc` (RFC 8881 §18.46).
// Must be the first op in every NFSv4.1 COMPOUND after session setup.
//   sessionid     — from CREATE_SESSION
//   sequenceid    — monotonically increasing per-slot counter (starts at 1)
//   slotid        — always 0 for single-slot sessions
//   highest_slotid— always 0 for single-slot sessions
//   cachethis     — false (no reply caching)
void encode_sequence41(XdrEncoder& enc,
                       const SessionId41& sessionid,
                       uint32_t sequenceid,
                       uint32_t slotid = 0,
                       uint32_t highest_slotid = 0,
                       bool cachethis = false);

// Decode SEQUENCE per-op result (skips all fields for single-slot use).
void decode_sequence41_result(XdrDecoder& dec);

// Encode RECLAIM_COMPLETE op into `enc` (RFC 8881 §18.51).
//   one_fs — false = global reclaim complete (use after session establishment)
void encode_reclaim_complete(XdrEncoder& enc, bool one_fs = false);

// Decode RECLAIM_COMPLETE per-op result (just checks status).
void decode_reclaim_complete_result(XdrDecoder& dec);

// Encode DESTROY_SESSION op into `enc` (RFC 8881 §18.37).
void encode_destroy_session(XdrEncoder& enc, const SessionId41& sessionid);

// Decode DESTROY_SESSION per-op result (just checks status).
void decode_destroy_session_result(XdrDecoder& dec);

}  // namespace nfs4
