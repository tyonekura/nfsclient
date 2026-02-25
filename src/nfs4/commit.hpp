#pragma once

#include "nfs4_error.hpp"
#include "../xdr/xdr.hpp"

#include <array>
#include <cstdint>

namespace nfs4 {

void encode_commit(XdrEncoder& enc, uint64_t offset, uint32_t count);

// Returns the 8-byte write verifier.
std::array<uint8_t, 8> decode_commit_result(XdrDecoder& dec);

}  // namespace nfs4
