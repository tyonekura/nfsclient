#pragma once

#include "nfs3_types.hpp"
#include "../rpc/rpc_client.hpp"

#include <string>
#include <vector>

namespace nfs3 {

// Encode/decode helpers (pure, no network)
std::vector<uint8_t> encode_rename_args(const Fh3& from_dir, const std::string& from_name,
                                         const Fh3& to_dir,   const std::string& to_name);
void decode_rename_reply(const std::vector<uint8_t>& data);

// NFSPROC3_RENAME (proc 14): rename from_dir/from_name to to_dir/to_name.
// Atomically replaces the destination if it already exists (POSIX rename semantics).
void rename(TcpRpcClient& client,
            const Fh3& from_dir, const std::string& from_name,
            const Fh3& to_dir,   const std::string& to_name);

}  // namespace nfs3
