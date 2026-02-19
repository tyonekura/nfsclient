#pragma once

#include <cstdint>

// ONC RPC constants (RFC 5531)
static constexpr uint32_t RPC_VERSION   = 2;
static constexpr uint32_t AUTH_NONE     = 0;

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
