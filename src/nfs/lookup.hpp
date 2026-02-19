#pragma once

#include "nfs3_types.hpp"
#include "../rpc/rpc_client.hpp"

#include <string>
#include <vector>

namespace nfs3 {

// Encode / decode helpers (used directly by unit tests).
std::vector<uint8_t> encode_lookup_args(const Fh3& dir, const std::string& name);
Fh3 decode_lookup_reply(const std::vector<uint8_t>& data);

// Send NFSPROC3_LOOKUP and return the file handle of `name` inside `dir`.
Fh3 lookup(TcpRpcClient& client, const Fh3& dir, const std::string& name);

}  // namespace nfs3
