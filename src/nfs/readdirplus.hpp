#pragma once

#include "nfs3_types.hpp"
#include "../rpc/rpc_client.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace nfs3 {

// A single entry returned by READDIRPLUS (RFC 1813 §3.3.17).
// Unlike plain READDIR entries, each entry carries inline attributes and an
// optional file handle, saving a round-trip GETATTR/LOOKUP per entry.
struct DirEntryPlus3 {
    uint64_t    fileid;
    std::string name;
    uint64_t    cookie;     // pagination cursor
    bool        has_attrs;
    Fattr3      attrs;      // valid only when has_attrs == true
    bool        has_fh;
    Fh3         fh;         // valid only when has_fh == true
};

// Result of one READDIRPLUS RPC (a single page).
struct ReaddirplusPage {
    std::vector<DirEntryPlus3> entries;
    bool                       eof;
    std::array<uint8_t, 8>     cookieverf;
};

// Encode/decode helpers (pure, no network)
// dircount: max bytes of entry names + cookies in the reply.
// maxcount: max total reply bytes (including attributes and file handles).
std::vector<uint8_t> encode_readdirplus_args(const Fh3& dir,
                                              uint64_t cookie,
                                              const std::array<uint8_t, 8>& cookieverf,
                                              uint32_t dircount,
                                              uint32_t maxcount);
ReaddirplusPage decode_readdirplus_reply(const std::vector<uint8_t>& data);

// NFSPROC3_READDIRPLUS (proc 17) — single page.
ReaddirplusPage readdirplus_page(TcpRpcClient& client, const Fh3& dir,
                                  uint64_t cookie = 0,
                                  const std::array<uint8_t, 8>& cookieverf = {},
                                  uint32_t dircount = 4096,
                                  uint32_t maxcount = 32768);

// Convenience: auto-paginate until eof and return all entries.
std::vector<DirEntryPlus3> readdirplus(TcpRpcClient& client, const Fh3& dir,
                                        uint32_t dircount = 4096,
                                        uint32_t maxcount = 32768);

}  // namespace nfs3
