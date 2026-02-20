#pragma once

#include "nfs3_types.hpp"
#include "../rpc/rpc_client.hpp"

#include <cstdint>
#include <vector>

namespace nfs3 {

// ── FSSTAT (proc 18) ─────────────────────────────────────────────────────────

// Filesystem capacity and usage statistics (RFC 1813 §3.3.18).
struct FsstatResult {
    uint64_t tbytes;    // total filesystem capacity in bytes
    uint64_t fbytes;    // free bytes
    uint64_t abytes;    // bytes available to non-privileged users
    uint64_t tfiles;    // total number of file slots (inodes)
    uint64_t ffiles;    // free file slots
    uint64_t afiles;    // file slots available to non-privileged users
    uint32_t invarsec;  // server-estimated consistency interval (seconds)
};

std::vector<uint8_t> encode_fsstat_args(const Fh3& root);
FsstatResult         decode_fsstat_reply(const std::vector<uint8_t>& data);
FsstatResult         fsstat(TcpRpcClient& client, const Fh3& root);

// ── FSINFO (proc 19) ─────────────────────────────────────────────────────────

// FSF_* property flags returned in FsinfoResult::properties (RFC 1813 §3.3.19).
static constexpr uint32_t FSF_LINK        = 0x0001;
static constexpr uint32_t FSF_SYMLINK     = 0x0002;
static constexpr uint32_t FSF_HOMOGENEOUS = 0x0008;
static constexpr uint32_t FSF_CANSETTIME  = 0x0010;

// Server capabilities and preferred transfer sizes (RFC 1813 §3.3.19).
struct FsinfoResult {
    uint32_t rtmax;        // max bytes per READ request
    uint32_t rtpref;       // preferred READ transfer size
    uint32_t rtmult;       // suggested READ alignment multiple
    uint32_t wtmax;        // max bytes per WRITE request
    uint32_t wtpref;       // preferred WRITE transfer size
    uint32_t wtmult;       // suggested WRITE alignment multiple
    uint32_t dtpref;       // preferred READDIR reply size
    uint64_t maxfilesize;  // maximum file size the server supports
    Nfstime3 time_delta;   // server clock granularity
    uint32_t properties;   // FSF_* bitmask
};

std::vector<uint8_t> encode_fsinfo_args(const Fh3& root);
FsinfoResult         decode_fsinfo_reply(const std::vector<uint8_t>& data);
FsinfoResult         fsinfo(TcpRpcClient& client, const Fh3& root);

// ── PATHCONF (proc 20) ───────────────────────────────────────────────────────

// POSIX pathconf values for a filesystem object (RFC 1813 §3.3.20).
struct PathconfResult {
    uint32_t linkmax;          // max hard-link count for a file
    uint32_t name_max;         // max filename component length
    bool     no_trunc;         // server returns error if name > name_max
    bool     chown_restricted; // only root can change file ownership
    bool     case_insensitive; // server ignores case in name comparisons
    bool     case_preserving;  // server preserves case when storing names
};

std::vector<uint8_t> encode_pathconf_args(const Fh3& fh);
PathconfResult       decode_pathconf_reply(const std::vector<uint8_t>& data);
PathconfResult       pathconf(TcpRpcClient& client, const Fh3& fh);

}  // namespace nfs3
