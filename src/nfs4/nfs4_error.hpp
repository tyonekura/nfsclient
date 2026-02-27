#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

// NFSv4 status codes (RFC 7530 §13)
enum class Nfsstat4 : uint32_t {
    NFS4_OK                     = 0,
    NFS4ERR_PERM                = 1,
    NFS4ERR_NOENT               = 2,
    NFS4ERR_IO                  = 5,
    NFS4ERR_NXIO                = 6,
    NFS4ERR_ACCESS              = 13,
    NFS4ERR_EXIST               = 17,
    NFS4ERR_XDEV                = 18,
    NFS4ERR_NOTDIR              = 20,
    NFS4ERR_ISDIR               = 21,
    NFS4ERR_INVAL               = 22,
    NFS4ERR_FBIG                = 27,
    NFS4ERR_NOSPC               = 28,
    NFS4ERR_ROFS                = 30,
    NFS4ERR_MLINK               = 31,
    NFS4ERR_NAMETOOLONG         = 63,
    NFS4ERR_NOTEMPTY            = 66,
    NFS4ERR_DQUOT               = 69,
    NFS4ERR_STALE               = 70,
    NFS4ERR_BADHANDLE           = 10001,
    NFS4ERR_BAD_COOKIE          = 10003,
    NFS4ERR_NOTSUPP             = 10004,
    NFS4ERR_TOOSMALL            = 10005,
    NFS4ERR_SERVERFAULT         = 10006,
    NFS4ERR_BADTYPE             = 10007,
    NFS4ERR_DELAY               = 10008,
    NFS4ERR_SAME                = 10009,
    NFS4ERR_DENIED              = 10010,
    NFS4ERR_EXPIRED             = 10011,
    NFS4ERR_LOCKED              = 10012,
    NFS4ERR_GRACE               = 10013,
    NFS4ERR_FHEXPIRED           = 10014,
    NFS4ERR_SHARE_DENIED        = 10015,
    NFS4ERR_WRONGSEC            = 10016,
    NFS4ERR_CLID_INUSE          = 10017,
    NFS4ERR_RESOURCE            = 10018,
    NFS4ERR_MOVED               = 10019,
    NFS4ERR_NOFILEHANDLE        = 10020,
    NFS4ERR_MINOR_VERS_MISMATCH = 10021,
    NFS4ERR_STALE_CLIENTID      = 10022,
    NFS4ERR_STALE_STATEID       = 10023,
    NFS4ERR_OLD_STATEID         = 10024,
    NFS4ERR_BAD_STATEID         = 10025,
    NFS4ERR_BAD_SEQID           = 10026,
    NFS4ERR_NOT_SAME            = 10027,
    NFS4ERR_LOCK_RANGE          = 10028,
    NFS4ERR_SYMLINK             = 10029,
    NFS4ERR_RESTOREFH           = 10030,
    NFS4ERR_LEASE_MOVED         = 10031,
    NFS4ERR_ATTRNOTSUPP         = 10032,
    NFS4ERR_NO_GRACE            = 10033,
    NFS4ERR_RECLAIM_BAD         = 10034,
    NFS4ERR_RECLAIM_CONFLICT    = 10035,
    NFS4ERR_BADXDR              = 10036,
    NFS4ERR_LOCKS_HELD          = 10037,
    NFS4ERR_OPENMODE            = 10038,
    NFS4ERR_BADOWNER            = 10039,
    NFS4ERR_BADCHAR             = 10040,
    NFS4ERR_BADNAME             = 10041,
    NFS4ERR_BAD_RANGE           = 10042,
    NFS4ERR_LOCK_NOTSUPP        = 10043,
    NFS4ERR_OP_ILLEGAL          = 10044,
    NFS4ERR_DEADLOCK            = 10045,
    NFS4ERR_FILE_OPEN           = 10046,
    NFS4ERR_ADMIN_REVOKED       = 10047,
    NFS4ERR_CB_PATH_DOWN        = 10048,
    // NFSv4.1 error codes (RFC 8881 §15.1.9)
    NFS4ERR_BADSESSION          = 10052,
    NFS4ERR_BADSLOT             = 10053,
    NFS4ERR_BAD_HIGH_SLOT       = 10054,
    NFS4ERR_CONN_NOT_BOUND_TO_SESSION = 10055,
    NFS4ERR_DEADSESSION         = 10056,
    NFS4ERR_SEQ_FALSE_RETRY     = 10060,
    NFS4ERR_SEQ_MISORDERED      = 10063,
};

// Exception thrown when an NFS4 operation returns a non-zero nfsstat4.
struct Nfs4Error : std::runtime_error {
    uint32_t status;

    explicit Nfs4Error(uint32_t s)
        : std::runtime_error("NFS4 error nfsstat4=" + std::to_string(s)), status(s) {}

    Nfs4Error(uint32_t s, const std::string& op)
        : std::runtime_error(op + " failed, nfsstat4=" + std::to_string(s)), status(s) {}

    bool is(Nfsstat4 code) const { return status == static_cast<uint32_t>(code); }
};
