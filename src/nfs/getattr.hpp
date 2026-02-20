#pragma once

#include "nfs3_types.hpp"
#include "../rpc/rpc_client.hpp"

#include <vector>

namespace nfs3 {

// Encode/decode helpers (pure, no network — callable from tests)
std::vector<uint8_t> encode_getattr_args(const Fh3& fh);
Fattr3               decode_getattr_reply(const std::vector<uint8_t>& data);

// NFSPROC3_GETATTR (proc 1): return file attributes for fh.
Fattr3 getattr(TcpRpcClient& client, const Fh3& fh);

}  // namespace nfs3
