#include "symlink.hpp"
#include "nfs_error.hpp"
#include "../xdr/xdr.hpp"

#include <stdexcept>

namespace nfs3 {

static constexpr uint32_t NFS_PROG         = 100003;
static constexpr uint32_t NFS_VERS         = 3;
static constexpr uint32_t NFSPROC3_READLINK = 5;
static constexpr uint32_t NFSPROC3_SYMLINK  = 10;
static constexpr uint32_t NFSPROC3_LINK     = 15;

// ── READLINK ─────────────────────────────────────────────────────────────────

std::vector<uint8_t> encode_readlink_args(const Fh3& symlink_fh) {
    XdrEncoder enc;
    encode_fh3(enc, symlink_fh);
    return enc.release();
}

std::string decode_readlink_reply(const std::vector<uint8_t>& data) {
    XdrDecoder dec(data);
    const uint32_t status = dec.get_uint32();
    // READLINK3res always carries symlink_attributes (post_op_attr).
    skip_post_op_attr(dec);
    if (status != 0)
        throw NfsError(status, "READLINK");
    // READLINK3resok: data (nfspath3 = string)
    return dec.get_string();
}

std::string readlink(TcpRpcClient& client, const Fh3& symlink_fh) {
    const auto args  = encode_readlink_args(symlink_fh);
    const auto reply = client.call(NFS_PROG, NFS_VERS, NFSPROC3_READLINK, args);
    return decode_readlink_reply(reply);
}

// ── SYMLINK ──────────────────────────────────────────────────────────────────

std::vector<uint8_t> encode_symlink_args(const Fh3& dir, const std::string& name,
                                          const std::string& target,
                                          const Sattr3& attrs) {
    XdrEncoder enc;
    encode_fh3(enc, dir);
    enc.put_string(name);
    // symlinkdata3: sattr3 + nfspath3
    encode_sattr3(enc, attrs);
    enc.put_string(target);
    return enc.release();
}

Fh3 decode_symlink_reply(const std::vector<uint8_t>& data) {
    XdrDecoder dec(data);
    const uint32_t status = dec.get_uint32();
    if (status != 0) {
        // SYMLINK3resfail: dir_wcc follows
        throw NfsError(status, "SYMLINK");
    }
    // SYMLINK3resok: obj (post_op_fh3), obj_attributes (post_op_attr), dir_wcc
    const uint32_t fh_present = dec.get_uint32();
    if (!fh_present)
        throw std::runtime_error("SYMLINK: server returned no file handle");
    Fh3 fh = decode_fh3(dec);
    skip_post_op_attr(dec);  // obj_attributes
    skip_wcc_data(dec);      // dir_wcc
    return fh;
}

Fh3 symlink(TcpRpcClient& client, const Fh3& dir, const std::string& name,
             const std::string& target, const Sattr3& attrs) {
    const auto args  = encode_symlink_args(dir, name, target, attrs);
    const auto reply = client.call(NFS_PROG, NFS_VERS, NFSPROC3_SYMLINK, args);
    return decode_symlink_reply(reply);
}

// ── LINK ─────────────────────────────────────────────────────────────────────

std::vector<uint8_t> encode_link_args(const Fh3& file,
                                       const Fh3& link_dir,
                                       const std::string& link_name) {
    XdrEncoder enc;
    encode_fh3(enc, file);
    encode_fh3(enc, link_dir);
    enc.put_string(link_name);
    return enc.release();
}

void decode_link_reply(const std::vector<uint8_t>& data) {
    XdrDecoder dec(data);
    const uint32_t status = dec.get_uint32();
    // LINK3res always carries file_attributes (post_op_attr) and linkdir_wcc.
    skip_post_op_attr(dec);  // file_attributes
    skip_wcc_data(dec);      // linkdir_wcc
    if (status != 0)
        throw NfsError(status, "LINK");
}

void link(TcpRpcClient& client, const Fh3& file,
           const Fh3& link_dir, const std::string& link_name) {
    const auto args  = encode_link_args(file, link_dir, link_name);
    const auto reply = client.call(NFS_PROG, NFS_VERS, NFSPROC3_LINK, args);
    decode_link_reply(reply);
}

}  // namespace nfs3
