#pragma once

#include "../rpc/rpc_client.hpp"
#include "../xdr/xdr.hpp"
#include "nfs4_error.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nfs4 {

// NFSv4 op codes (RFC 7530 §18 / RFC 8881 §18)
constexpr uint32_t OP_ACCESS               = 3;
constexpr uint32_t OP_CLOSE                = 4;
constexpr uint32_t OP_COMMIT               = 5;
constexpr uint32_t OP_CREATE               = 6;
constexpr uint32_t OP_GETATTR              = 9;
constexpr uint32_t OP_GETFH                = 10;
constexpr uint32_t OP_LOOKUP               = 15;
constexpr uint32_t OP_LOOKUPP              = 16;
constexpr uint32_t OP_OPEN                 = 18;
constexpr uint32_t OP_OPEN_CONFIRM         = 20;
constexpr uint32_t OP_PUTFH                = 22;
constexpr uint32_t OP_PUTROOTFH            = 24;
constexpr uint32_t OP_READ                 = 25;
constexpr uint32_t OP_READDIR              = 26;
constexpr uint32_t OP_READLINK             = 27;
constexpr uint32_t OP_REMOVE               = 28;
constexpr uint32_t OP_RENAME               = 29;
constexpr uint32_t OP_RENEW                = 30;
constexpr uint32_t OP_RESTOREFH            = 31;
constexpr uint32_t OP_SAVEFH               = 32;
constexpr uint32_t OP_SETATTR              = 34;
constexpr uint32_t OP_SETCLIENTID          = 35;
constexpr uint32_t OP_SETCLIENTID_CONFIRM  = 36;
constexpr uint32_t OP_WRITE                = 38;

// NFSv4.1 op codes (RFC 8881 §18)
constexpr uint32_t OP_BIND_CONN_TO_SESSION = 41;
constexpr uint32_t OP_EXCHANGE_ID          = 42;
constexpr uint32_t OP_CREATE_SESSION       = 43;
constexpr uint32_t OP_DESTROY_SESSION      = 44;
constexpr uint32_t OP_FREE_STATEID         = 45;
constexpr uint32_t OP_TEST_STATEID         = 56;
constexpr uint32_t OP_DESTROY_CLIENTID     = 57;
constexpr uint32_t OP_RECLAIM_COMPLETE     = 58;
constexpr uint32_t OP_SEQUENCE             = 53;

// Build and send a COMPOUND request.
//
// Wire format sent:
//   [tag:string] [minorversion:u32] [numops:u32] [ops_bytes...]
//
// minorversion=0 for NFSv4.0, minorversion=1 for NFSv4.1 (default 0).
// Returns the raw reply bytes starting from COMPOUND4res.status.
// The caller parses: outer_status(u32), tag(string), numops(u32), then per-op results.
//
// Throws Nfs4Error if the RPC transport fails.
std::vector<uint8_t> call_compound(TcpRpcClient& rpc,
                                    const std::string& tag,
                                    const std::vector<uint8_t>& ops_bytes,
                                    uint32_t num_ops,
                                    uint32_t minorversion = 0);

// Helper: parse the COMPOUND4res header from `reply` and return an XdrDecoder
// positioned at the start of the resarray.  Throws Nfs4Error on outer failure.
//
// Usage:
//   auto reply = call_compound(rpc, tag, ops, n);
//   XdrDecoder dec(reply);
//   check_compound_status(dec);   // advances past status+tag+numops
//   decode_putfh_result(dec);
//   auto fh = decode_getfh_result(dec);
void check_compound_status(XdrDecoder& dec);

}  // namespace nfs4
