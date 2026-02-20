#pragma once

#include "nfs3_types.hpp"
#include "../rpc/rpc_client.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace nfs3 {

// A single entry returned by READDIR (RFC 1813 §3.3.16).
struct DirEntry3 {
    uint64_t    fileid;  // inode number
    std::string name;
    uint64_t    cookie;  // opaque pagination cursor for this entry
};

// Result of one READDIR RPC (a single page).
struct ReaddirPage {
    std::vector<DirEntry3>  entries;
    bool                    eof;
    std::array<uint8_t, 8>  cookieverf;  // must be echoed back in subsequent calls
};

// Encode/decode helpers (pure, no network)
std::vector<uint8_t> encode_readdir_args(const Fh3& dir,
                                          uint64_t cookie,
                                          const std::array<uint8_t, 8>& cookieverf,
                                          uint32_t count);
ReaddirPage decode_readdir_reply(const std::vector<uint8_t>& data);

// NFSPROC3_READDIR (proc 16) — single RPC, returns one page of entries.
// Pass cookie=0 and cookieverf={} for the first call; subsequent calls use
// the last entry's cookie and the cookieverf from the previous ReaddirPage.
ReaddirPage readdir_page(TcpRpcClient& client, const Fh3& dir,
                          uint64_t cookie = 0,
                          const std::array<uint8_t, 8>& cookieverf = {},
                          uint32_t count = 4096);

// Convenience: auto-paginate until eof and return all entries.
std::vector<DirEntry3> readdir(TcpRpcClient& client, const Fh3& dir,
                                uint32_t count = 4096);

}  // namespace nfs3
