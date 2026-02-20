#pragma once

#include "nfs3_types.hpp"
#include "../rpc/rpc_client.hpp"

#include <string>
#include <vector>

namespace nfs3 {

// sattrguard3 (RFC 1813 §3.3.2): optional guard that rejects the SETATTR if the
// server's current ctime differs from the supplied value.
struct SattrGuard3 {
    bool     check      = false;
    uint32_t ctime_sec  = 0;
    uint32_t ctime_nsec = 0;
};

// Encode/decode helpers (pure, no network)
std::vector<uint8_t> encode_setattr_args(const Fh3& fh, const Sattr3& attrs,
                                          const SattrGuard3& guard = {});
void decode_setattr_reply(const std::vector<uint8_t>& data);

// NFSPROC3_SETATTR (proc 2): set attributes on fh.
// Throws NfsError on failure (including NFS3ERR_NOT_SYNC if the guard fails).
void setattr(TcpRpcClient& client, const Fh3& fh, const Sattr3& attrs,
             const SattrGuard3& guard = {});

}  // namespace nfs3
