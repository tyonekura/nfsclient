#include "lookup.hpp"
#include "nfs_error.hpp"
#include "../xdr/xdr.hpp"

#include <stdexcept>

namespace nfs3 {

static constexpr uint32_t NFS_PROG        = 100003;
static constexpr uint32_t NFS_VERS        = 3;
static constexpr uint32_t NFSPROC3_LOOKUP = 3;

std::vector<uint8_t> encode_lookup_args(const Fh3& dir, const std::string& name) {
    XdrEncoder enc;
    encode_fh3(enc, dir);
    enc.put_string(name);
    return enc.release();
}

Fh3 decode_lookup_reply(const std::vector<uint8_t>& data) {
    XdrDecoder dec(data);
    const uint32_t status = dec.get_uint32();
    if (status != 0) {
        // LOOKUP3resfail: dir_attributes (post_op_attr) follows, but we just throw.
        throw NfsError(status, "LOOKUP");
    }
    // LOOKUP3resok: object fh3, obj_attributes, dir_attributes
    Fh3 fh = decode_fh3(dec);
    skip_post_op_attr(dec);  // obj_attributes
    skip_post_op_attr(dec);  // dir_attributes
    return fh;
}

Fh3 lookup(TcpRpcClient& client, const Fh3& dir, const std::string& name) {
    const auto args  = encode_lookup_args(dir, name);
    const auto reply = client.call(NFS_PROG, NFS_VERS, NFSPROC3_LOOKUP, args);
    return decode_lookup_reply(reply);
}

}  // namespace nfs3
