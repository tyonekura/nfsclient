#pragma once

#include "nfs3_types.hpp"
#include "../rpc/rpc_client.hpp"

#include <string>
#include <vector>

namespace nfs3 {

// ── READLINK (proc 5) ─────────────────────────────────────────────────────────

// Encode/decode helpers (pure, no network)
std::vector<uint8_t> encode_readlink_args(const Fh3& symlink_fh);
std::string          decode_readlink_reply(const std::vector<uint8_t>& data);

// NFSPROC3_READLINK (proc 5): read the target path of a symbolic link.
std::string readlink(TcpRpcClient& client, const Fh3& symlink_fh);

// ── SYMLINK (proc 10) ─────────────────────────────────────────────────────────

// Encode/decode helpers (pure, no network)
std::vector<uint8_t> encode_symlink_args(const Fh3& dir, const std::string& name,
                                          const std::string& target,
                                          const Sattr3& attrs = {});
Fh3 decode_symlink_reply(const std::vector<uint8_t>& data);

// NFSPROC3_SYMLINK (proc 10): create a symbolic link `name` in `dir` pointing
// to `target`. Returns the new symlink's file handle (if the server provides one).
Fh3 symlink(TcpRpcClient& client, const Fh3& dir, const std::string& name,
             const std::string& target, const Sattr3& attrs = {});

// ── LINK (proc 15) ───────────────────────────────────────────────────────────

// Encode/decode helpers (pure, no network)
std::vector<uint8_t> encode_link_args(const Fh3& file,
                                       const Fh3& link_dir,
                                       const std::string& link_name);
void decode_link_reply(const std::vector<uint8_t>& data);

// NFSPROC3_LINK (proc 15): create a hard link named `link_name` in `link_dir`
// that refers to the existing `file`.
void link(TcpRpcClient& client, const Fh3& file,
           const Fh3& link_dir, const std::string& link_name);

}  // namespace nfs3
