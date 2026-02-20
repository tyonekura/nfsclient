#pragma once

#include "nfs3_types.hpp"
#include "../rpc/rpc_client.hpp"

#include <array>
#include <string>
#include <vector>

namespace nfs3 {

// createhow3 discriminant (RFC 1813 §3.3.8)
enum class CreateMode3 : uint32_t {
    UNCHECKED = 0,
    GUARDED   = 1,
    EXCLUSIVE = 2,
};

// createverf3: 8-byte opaque used for exactly-once CREATE semantics.
struct CreateVerf3 {
    std::array<uint8_t, 8> data{};
};

// Encode/decode helpers (pure, no network)
// UNCHECKED/GUARDED: send with sattr3
std::vector<uint8_t> encode_create_args(const Fh3& dir, const std::string& name,
                                         CreateMode3 mode, const Sattr3& attrs);
// EXCLUSIVE: send with createverf3 instead of sattr3
std::vector<uint8_t> encode_create_args_exclusive(const Fh3& dir, const std::string& name,
                                                    const CreateVerf3& verf);

// Decode the CREATE reply; returns the new object's file handle.
// Throws NfsError on failure. Throws std::runtime_error if the server does
// not return a post-op file handle (server bug / very old server).
Fh3 decode_create_reply(const std::vector<uint8_t>& data);

// NFSPROC3_CREATE (proc 8) — UNCHECKED or GUARDED mode.
Fh3 create(TcpRpcClient& client, const Fh3& dir, const std::string& name,
            CreateMode3 mode = CreateMode3::UNCHECKED, const Sattr3& attrs = {});

// NFSPROC3_CREATE (proc 8) — EXCLUSIVE mode (idempotent with a verifier).
Fh3 create_exclusive(TcpRpcClient& client, const Fh3& dir, const std::string& name,
                     const CreateVerf3& verf);

}  // namespace nfs3
