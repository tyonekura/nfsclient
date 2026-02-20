#include "create.hpp"
#include "nfs_error.hpp"
#include "../xdr/xdr.hpp"

#include <stdexcept>

namespace nfs3 {

static constexpr uint32_t NFS_PROG        = 100003;
static constexpr uint32_t NFS_VERS        = 3;
static constexpr uint32_t NFSPROC3_CREATE = 8;
static constexpr size_t   CREATE_VERF_SIZE = 8;

std::vector<uint8_t> encode_create_args(const Fh3& dir, const std::string& name,
                                         CreateMode3 mode, const Sattr3& attrs) {
    XdrEncoder enc;
    encode_fh3(enc, dir);
    enc.put_string(name);
    enc.put_uint32(static_cast<uint32_t>(mode));
    encode_sattr3(enc, attrs);
    return enc.release();
}

std::vector<uint8_t> encode_create_args_exclusive(const Fh3& dir, const std::string& name,
                                                    const CreateVerf3& verf) {
    XdrEncoder enc;
    encode_fh3(enc, dir);
    enc.put_string(name);
    enc.put_uint32(static_cast<uint32_t>(CreateMode3::EXCLUSIVE));
    enc.put_fixed_opaque(verf.data.data(), CREATE_VERF_SIZE);
    return enc.release();
}

Fh3 decode_create_reply(const std::vector<uint8_t>& data) {
    XdrDecoder dec(data);
    const uint32_t status = dec.get_uint32();
    if (status != 0) {
        // CREATE3resfail: obj_wcc follows
        throw NfsError(status, "CREATE");
    }
    // CREATE3resok: post_op_fh3, obj_attributes (post_op_attr), dir_wcc
    // post_op_fh3 = bool + optional fh3
    const uint32_t fh_present = dec.get_uint32();
    if (!fh_present)
        throw std::runtime_error("CREATE: server returned no file handle");
    Fh3 fh = decode_fh3(dec);
    skip_post_op_attr(dec);  // obj_attributes
    skip_wcc_data(dec);      // dir_wcc
    return fh;
}

Fh3 create(TcpRpcClient& client, const Fh3& dir, const std::string& name,
            CreateMode3 mode, const Sattr3& attrs) {
    const auto args  = encode_create_args(dir, name, mode, attrs);
    const auto reply = client.call(NFS_PROG, NFS_VERS, NFSPROC3_CREATE, args);
    return decode_create_reply(reply);
}

Fh3 create_exclusive(TcpRpcClient& client, const Fh3& dir, const std::string& name,
                     const CreateVerf3& verf) {
    const auto args  = encode_create_args_exclusive(dir, name, verf);
    const auto reply = client.call(NFS_PROG, NFS_VERS, NFSPROC3_CREATE, args);
    return decode_create_reply(reply);
}

}  // namespace nfs3
