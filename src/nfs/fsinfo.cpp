#include "fsinfo.hpp"
#include "nfs_error.hpp"
#include "../xdr/xdr.hpp"

namespace nfs3 {

static constexpr uint32_t NFS_PROG          = 100003;
static constexpr uint32_t NFS_VERS          = 3;
static constexpr uint32_t NFSPROC3_FSSTAT   = 18;
static constexpr uint32_t NFSPROC3_FSINFO   = 19;
static constexpr uint32_t NFSPROC3_PATHCONF = 20;

// ── FSSTAT ────────────────────────────────────────────────────────────────────

std::vector<uint8_t> encode_fsstat_args(const Fh3& root) {
    XdrEncoder enc;
    encode_fh3(enc, root);
    return enc.release();
}

FsstatResult decode_fsstat_reply(const std::vector<uint8_t>& data) {
    XdrDecoder dec(data);
    const uint32_t status = dec.get_uint32();
    // FSSTAT3res always carries obj_attributes (post_op_attr).
    skip_post_op_attr(dec);
    if (status != 0)
        throw NfsError(status, "FSSTAT");
    FsstatResult r{};
    r.tbytes   = dec.get_uint64();
    r.fbytes   = dec.get_uint64();
    r.abytes   = dec.get_uint64();
    r.tfiles   = dec.get_uint64();
    r.ffiles   = dec.get_uint64();
    r.afiles   = dec.get_uint64();
    r.invarsec = dec.get_uint32();
    return r;
}

FsstatResult fsstat(TcpRpcClient& client, const Fh3& root) {
    const auto args  = encode_fsstat_args(root);
    const auto reply = client.call(NFS_PROG, NFS_VERS, NFSPROC3_FSSTAT, args);
    return decode_fsstat_reply(reply);
}

// ── FSINFO ────────────────────────────────────────────────────────────────────

std::vector<uint8_t> encode_fsinfo_args(const Fh3& root) {
    XdrEncoder enc;
    encode_fh3(enc, root);
    return enc.release();
}

FsinfoResult decode_fsinfo_reply(const std::vector<uint8_t>& data) {
    XdrDecoder dec(data);
    const uint32_t status = dec.get_uint32();
    // FSINFO3res always carries obj_attributes (post_op_attr).
    skip_post_op_attr(dec);
    if (status != 0)
        throw NfsError(status, "FSINFO");
    FsinfoResult r{};
    r.rtmax       = dec.get_uint32();
    r.rtpref      = dec.get_uint32();
    r.rtmult      = dec.get_uint32();
    r.wtmax       = dec.get_uint32();
    r.wtpref      = dec.get_uint32();
    r.wtmult      = dec.get_uint32();
    r.dtpref      = dec.get_uint32();
    r.maxfilesize = dec.get_uint64();
    r.time_delta.seconds  = dec.get_uint32();
    r.time_delta.nseconds = dec.get_uint32();
    r.properties  = dec.get_uint32();
    return r;
}

FsinfoResult fsinfo(TcpRpcClient& client, const Fh3& root) {
    const auto args  = encode_fsinfo_args(root);
    const auto reply = client.call(NFS_PROG, NFS_VERS, NFSPROC3_FSINFO, args);
    return decode_fsinfo_reply(reply);
}

// ── PATHCONF ──────────────────────────────────────────────────────────────────

std::vector<uint8_t> encode_pathconf_args(const Fh3& fh) {
    XdrEncoder enc;
    encode_fh3(enc, fh);
    return enc.release();
}

PathconfResult decode_pathconf_reply(const std::vector<uint8_t>& data) {
    XdrDecoder dec(data);
    const uint32_t status = dec.get_uint32();
    // PATHCONF3res always carries obj_attributes (post_op_attr).
    skip_post_op_attr(dec);
    if (status != 0)
        throw NfsError(status, "PATHCONF");
    PathconfResult r{};
    r.linkmax          = dec.get_uint32();
    r.name_max         = dec.get_uint32();
    r.no_trunc         = (dec.get_uint32() != 0);
    r.chown_restricted = (dec.get_uint32() != 0);
    r.case_insensitive = (dec.get_uint32() != 0);
    r.case_preserving  = (dec.get_uint32() != 0);
    return r;
}

PathconfResult pathconf(TcpRpcClient& client, const Fh3& fh) {
    const auto args  = encode_pathconf_args(fh);
    const auto reply = client.call(NFS_PROG, NFS_VERS, NFSPROC3_PATHCONF, args);
    return decode_pathconf_reply(reply);
}

}  // namespace nfs3
