#pragma once

#include <cstdint>
#include <string>
#include <vector>

// ONC RPC constants (RFC 5531)
static constexpr uint32_t RPC_VERSION   = 2;
static constexpr uint32_t AUTH_NONE     = 0;
static constexpr uint32_t AUTH_SYS_FLAV = 1;  // AUTH_SYS / AUTH_UNIX (RFC 5531 §8)

enum class MsgType : uint32_t {
    CALL  = 0,
    REPLY = 1,
};

enum class ReplyStat : uint32_t {
    MSG_ACCEPTED = 0,
    MSG_DENIED   = 1,
};

enum class AcceptStat : uint32_t {
    SUCCESS       = 0,
    PROG_UNAVAIL  = 1,
    PROG_MISMATCH = 2,
    PROC_UNAVAIL  = 3,
    GARBAGE_ARGS  = 4,
    SYSTEM_ERR    = 5,
};

// AUTH_SYS credential body (RFC 5531 §8.1 / RFC 1057).
// Identifies the caller by Unix uid/gid.
struct AuthSys {
    uint32_t              stamp       = 0;         // arbitrary id, typically time()
    std::string           machinename = "localhost";
    uint32_t              uid         = 0;
    uint32_t              gid         = 0;
    std::vector<uint32_t> gids;                    // supplemental groups (max 16)
};
