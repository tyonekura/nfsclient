#pragma once

#include "nfs3_types.hpp"
#include "../rpc/rpc_client.hpp"

#include <string>
#include <vector>

// Directory-mutating NFS operations: MKDIR, REMOVE, RMDIR (RFC 1813 §3.3.9–§3.3.13).
// All encode/decode helpers are pure (no network) to allow unit testing.

namespace nfs3 {

// ── MKDIR (proc 9) ───────────────────────────────────────────────────────────

std::vector<uint8_t> encode_mkdir_args(const Fh3& dir, const std::string& name,
                                        const Sattr3& attrs);
Fh3                  decode_mkdir_reply(const std::vector<uint8_t>& data);

// NFSPROC3_MKDIR: create a directory named `name` in `dir`.
// Returns the new directory's file handle.
Fh3 mkdir(TcpRpcClient& client, const Fh3& dir, const std::string& name,
           const Sattr3& attrs = {});

// ── REMOVE (proc 12) ─────────────────────────────────────────────────────────

std::vector<uint8_t> encode_remove_args(const Fh3& dir, const std::string& name);
void                 decode_remove_reply(const std::vector<uint8_t>& data);

// NFSPROC3_REMOVE: delete the file named `name` from directory `dir`.
void remove(TcpRpcClient& client, const Fh3& dir, const std::string& name);

// ── RMDIR (proc 13) ──────────────────────────────────────────────────────────

std::vector<uint8_t> encode_rmdir_args(const Fh3& dir, const std::string& name);
void                 decode_rmdir_reply(const std::vector<uint8_t>& data);

// NFSPROC3_RMDIR: remove the empty directory named `name` from directory `dir`.
void rmdir(TcpRpcClient& client, const Fh3& dir, const std::string& name);

}  // namespace nfs3
