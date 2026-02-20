#include "dirop.hpp"
#include "nfs_error.hpp"
#include "../xdr/xdr.hpp"

namespace nfs3 {

static constexpr uint32_t NFS_PROG        = 100003;
static constexpr uint32_t NFS_VERS        = 3;
static constexpr uint32_t NFSPROC3_MKDIR  = 9;
static constexpr uint32_t NFSPROC3_REMOVE = 12;
static constexpr uint32_t NFSPROC3_RMDIR  = 13;

// ── MKDIR ─────────────────────────────────────────────────────────────────────

std::vector<uint8_t> encode_mkdir_args(const Fh3& dir, const std::string& name,
                                        const Sattr3& attrs) {
    XdrEncoder enc;
    encode_fh3(enc, dir);
    enc.put_string(name);
    encode_sattr3(enc, attrs);
    return enc.release();
}

Fh3 decode_mkdir_reply(const std::vector<uint8_t>& data) {
    XdrDecoder dec(data);
    const uint32_t status = dec.get_uint32();
    if (status != 0)
        throw NfsError(status, "MKDIR");
    // MKDIR3resok: obj (post_op_fh3), obj_attributes (post_op_attr), dir_wcc
    const uint32_t fh_present = dec.get_uint32();
    if (!fh_present)
        throw std::runtime_error("MKDIR: server returned no file handle");
    Fh3 fh = decode_fh3(dec);
    skip_post_op_attr(dec);  // obj_attributes
    skip_wcc_data(dec);      // dir_wcc
    return fh;
}

Fh3 mkdir(TcpRpcClient& client, const Fh3& dir, const std::string& name,
           const Sattr3& attrs) {
    const auto args  = encode_mkdir_args(dir, name, attrs);
    const auto reply = client.call(NFS_PROG, NFS_VERS, NFSPROC3_MKDIR, args);
    return decode_mkdir_reply(reply);
}

// ── REMOVE ────────────────────────────────────────────────────────────────────

std::vector<uint8_t> encode_remove_args(const Fh3& dir, const std::string& name) {
    XdrEncoder enc;
    encode_fh3(enc, dir);
    enc.put_string(name);
    return enc.release();
}

void decode_remove_reply(const std::vector<uint8_t>& data) {
    XdrDecoder dec(data);
    const uint32_t status = dec.get_uint32();
    if (status != 0)
        throw NfsError(status, "REMOVE");
    // REMOVE3resok: dir_wcc
    skip_wcc_data(dec);
}

void remove(TcpRpcClient& client, const Fh3& dir, const std::string& name) {
    const auto args  = encode_remove_args(dir, name);
    const auto reply = client.call(NFS_PROG, NFS_VERS, NFSPROC3_REMOVE, args);
    decode_remove_reply(reply);
}

// ── RMDIR ─────────────────────────────────────────────────────────────────────

std::vector<uint8_t> encode_rmdir_args(const Fh3& dir, const std::string& name) {
    XdrEncoder enc;
    encode_fh3(enc, dir);
    enc.put_string(name);
    return enc.release();
}

void decode_rmdir_reply(const std::vector<uint8_t>& data) {
    XdrDecoder dec(data);
    const uint32_t status = dec.get_uint32();
    if (status != 0)
        throw NfsError(status, "RMDIR");
    // RMDIR3resok: dir_wcc
    skip_wcc_data(dec);
}

void rmdir(TcpRpcClient& client, const Fh3& dir, const std::string& name) {
    const auto args  = encode_rmdir_args(dir, name);
    const auto reply = client.call(NFS_PROG, NFS_VERS, NFSPROC3_RMDIR, args);
    decode_rmdir_reply(reply);
}

}  // namespace nfs3
