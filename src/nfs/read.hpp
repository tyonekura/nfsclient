#pragma once

#include "nfs3_types.hpp"
#include "../rpc/rpc_client.hpp"

#include <vector>

namespace nfs3 {

// Encode / decode helpers (used directly by unit tests).
std::vector<uint8_t> encode_read_args(const Fh3& fh, uint64_t offset, uint32_t count);
std::vector<uint8_t> decode_read_reply(const std::vector<uint8_t>& data);

// Send NFSPROC3_READ and return the data bytes read.
std::vector<uint8_t> read(TcpRpcClient& client, const Fh3& fh,
                           uint64_t offset, uint32_t count);

}  // namespace nfs3
