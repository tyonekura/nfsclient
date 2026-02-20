#include "mknod.hpp"
#include "nfs_error.hpp"
#include "../xdr/xdr.hpp"

#include <stdexcept>

namespace nfs3 {

static constexpr uint32_t NFS_PROG       = 100003;
static constexpr uint32_t NFS_VERS       = 3;
static constexpr uint32_t NFSPROC3_MKNOD = 11;

std::vector<uint8_t> encode_mknod_args(const Fh3& dir, const std::string& name,
                                        Ftype3 type, const Sattr3& attrs) {
    XdrEncoder enc;
    encode_fh3(enc, dir);
    enc.put_string(name);
    enc.put_uint32(static_cast<uint32_t>(type));
    encode_sattr3(enc, attrs);
    return enc.release();
}

std::vector<uint8_t> encode_mknod_device_args(const Fh3& dir, const std::string& name,
                                               Ftype3 type, const Sattr3& attrs,
                                               const DeviceSpec3& spec) {
    XdrEncoder enc;
    encode_fh3(enc, dir);
    enc.put_string(name);
    enc.put_uint32(static_cast<uint32_t>(type));
    encode_sattr3(enc, attrs);
    enc.put_uint32(spec.major_num);
    enc.put_uint32(spec.minor_num);
    return enc.release();
}

Fh3 decode_mknod_reply(const std::vector<uint8_t>& data) {
    XdrDecoder dec(data);
    const uint32_t status = dec.get_uint32();
    if (status != 0) {
        // MKNOD3resfail: dir_wcc follows
        throw NfsError(status, "MKNOD");
    }
    // MKNOD3resok: obj (post_op_fh3), obj_attributes (post_op_attr), dir_wcc
    const uint32_t fh_present = dec.get_uint32();
    if (!fh_present)
        throw std::runtime_error("MKNOD: server returned no file handle");
    Fh3 fh = decode_fh3(dec);
    skip_post_op_attr(dec);  // obj_attributes
    skip_wcc_data(dec);      // dir_wcc
    return fh;
}

static Fh3 mknod_simple(TcpRpcClient& client, const Fh3& dir,
                          const std::string& name, Ftype3 type, const Sattr3& attrs) {
    const auto args  = encode_mknod_args(dir, name, type, attrs);
    const auto reply = client.call(NFS_PROG, NFS_VERS, NFSPROC3_MKNOD, args);
    return decode_mknod_reply(reply);
}

static Fh3 mknod_device(TcpRpcClient& client, const Fh3& dir,
                          const std::string& name, Ftype3 type,
                          const Sattr3& attrs, const DeviceSpec3& spec) {
    const auto args  = encode_mknod_device_args(dir, name, type, attrs, spec);
    const auto reply = client.call(NFS_PROG, NFS_VERS, NFSPROC3_MKNOD, args);
    return decode_mknod_reply(reply);
}

Fh3 mknod_fifo(TcpRpcClient& client, const Fh3& dir, const std::string& name,
                const Sattr3& attrs) {
    return mknod_simple(client, dir, name, Ftype3::NF3FIFO, attrs);
}

Fh3 mknod_socket(TcpRpcClient& client, const Fh3& dir, const std::string& name,
                  const Sattr3& attrs) {
    return mknod_simple(client, dir, name, Ftype3::NF3SOCK, attrs);
}

Fh3 mknod_chr(TcpRpcClient& client, const Fh3& dir, const std::string& name,
               const Sattr3& attrs, const DeviceSpec3& spec) {
    return mknod_device(client, dir, name, Ftype3::NF3CHR, attrs, spec);
}

Fh3 mknod_blk(TcpRpcClient& client, const Fh3& dir, const std::string& name,
               const Sattr3& attrs, const DeviceSpec3& spec) {
    return mknod_device(client, dir, name, Ftype3::NF3BLK, attrs, spec);
}

}  // namespace nfs3
