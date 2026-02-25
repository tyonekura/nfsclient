#pragma once

#include "nfs4_error.hpp"
#include "../xdr/xdr.hpp"

#include <array>
#include <cstdint>
#include <string>

namespace nfs4 {

// Result of SETCLIENTID — used to drive SETCLIENTID_CONFIRM.
struct SetclientidResult {
    uint64_t               clientid{};
    std::array<uint8_t, 8> confirm_verifier{};
};

// Encode SETCLIENTID op into `enc`.
//   verifier    — 8-byte client-supplied verifier (e.g. boot time)
//   client_id   — unique string identifying this client instance
//   cb_program  — callback program number (0 = no callbacks)
void encode_setclientid(XdrEncoder& enc,
                        const std::array<uint8_t, 8>& verifier,
                        const std::string& client_id,
                        uint32_t cb_program = 0);

// Decode SETCLIENTID per-op result from the compound reply.
SetclientidResult decode_setclientid_result(XdrDecoder& dec);

// Encode SETCLIENTID_CONFIRM op into `enc`.
void encode_setclientid_confirm(XdrEncoder& enc,
                                 uint64_t clientid,
                                 const std::array<uint8_t, 8>& confirm_verifier);

// Decode SETCLIENTID_CONFIRM per-op result (just checks status).
void decode_setclientid_confirm_result(XdrDecoder& dec);

}  // namespace nfs4
