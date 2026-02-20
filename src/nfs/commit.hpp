#pragma once

#include "nfs3_types.hpp"
#include "../rpc/rpc_client.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace nfs3 {

// writeverf3 returned by COMMIT — same 8-byte opaque type as in WRITE replies.
using CommitVerf3 = std::array<uint8_t, 8>;

// Encode/decode helpers (pure, no network)
std::vector<uint8_t> encode_commit_args(const Fh3& fh,
                                         uint64_t offset = 0,
                                         uint32_t count  = 0);
CommitVerf3 decode_commit_reply(const std::vector<uint8_t>& data);

// NFSPROC3_COMMIT (proc 21): flush unstable writes to stable storage.
// offset=0, count=0 means "flush everything" (RFC 1813 §3.3.21).
// Returns the server's write verifier; callers compare it to the verifier
// received from prior WRITE calls to detect a server restart.
CommitVerf3 commit(TcpRpcClient& client, const Fh3& fh,
                   uint64_t offset = 0, uint32_t count = 0);

}  // namespace nfs3
