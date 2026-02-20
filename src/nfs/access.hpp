#pragma once

#include "nfs3_types.hpp"
#include "../rpc/rpc_client.hpp"

#include <cstdint>
#include <vector>

namespace nfs3 {

// Access permission bits (RFC 1813 §3.3.4).
// Combine with bitwise OR to request multiple permissions at once.
static constexpr uint32_t ACCESS3_READ    = 0x0001;
static constexpr uint32_t ACCESS3_LOOKUP  = 0x0002;
static constexpr uint32_t ACCESS3_MODIFY  = 0x0004;
static constexpr uint32_t ACCESS3_EXTEND  = 0x0008;
static constexpr uint32_t ACCESS3_DELETE  = 0x0010;
static constexpr uint32_t ACCESS3_EXECUTE = 0x0020;

// Encode/decode helpers (pure, no network)
std::vector<uint8_t> encode_access_args(const Fh3& fh, uint32_t access_mask);
uint32_t             decode_access_reply(const std::vector<uint8_t>& data);

// NFSPROC3_ACCESS (proc 4): check access permissions.
// Returns the bitmask of permissions actually granted — which may be a subset
// of access_mask (server can deny some bits), or even a superset (servers are
// allowed to return extra bits).
uint32_t access(TcpRpcClient& client, const Fh3& fh, uint32_t access_mask);

}  // namespace nfs3
