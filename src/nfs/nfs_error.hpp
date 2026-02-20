#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

// NFSv3 status codes (RFC 1813 §2.6)
enum class Nfsstat3 : uint32_t {
    NFS3_OK             = 0,
    NFS3ERR_PERM        = 1,
    NFS3ERR_NOENT       = 2,
    NFS3ERR_IO          = 5,
    NFS3ERR_NXIO        = 6,
    NFS3ERR_ACCES       = 13,
    NFS3ERR_EXIST       = 17,
    NFS3ERR_XDEV        = 18,
    NFS3ERR_NODEV       = 19,
    NFS3ERR_NOTDIR      = 20,
    NFS3ERR_ISDIR       = 21,
    NFS3ERR_INVAL       = 22,
    NFS3ERR_FBIG        = 27,
    NFS3ERR_NOSPC       = 28,
    NFS3ERR_ROFS        = 30,
    NFS3ERR_MLINK       = 31,
    NFS3ERR_NAMETOOLONG = 63,
    NFS3ERR_NOTEMPTY    = 66,
    NFS3ERR_DQUOT       = 69,
    NFS3ERR_STALE       = 70,
    NFS3ERR_REMOTE      = 71,
    NFS3ERR_BADHANDLE   = 10001,
    NFS3ERR_NOT_SYNC    = 10002,
    NFS3ERR_BAD_COOKIE  = 10003,
    NFS3ERR_NOTSUPP     = 10004,
    NFS3ERR_TOOSMALL    = 10005,
    NFS3ERR_SERVERFAULT = 10006,
    NFS3ERR_BADTYPE     = 10007,
    NFS3ERR_JUKEBOX     = 10008,
};

// Exception thrown when an NFS operation returns a non-zero nfsstat3.
// Callers can catch NfsError to inspect the specific status code.
struct NfsError : std::runtime_error {
    uint32_t status;

    explicit NfsError(uint32_t s)
        : std::runtime_error("NFS error nfsstat3=" + std::to_string(s)), status(s) {}

    NfsError(uint32_t s, const std::string& proc)
        : std::runtime_error(proc + " failed, nfsstat3=" + std::to_string(s)), status(s) {}

    bool is(Nfsstat3 code) const { return status == static_cast<uint32_t>(code); }
};
