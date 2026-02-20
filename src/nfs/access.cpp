#include "access.hpp"
#include "nfs_error.hpp"
#include "../xdr/xdr.hpp"

namespace nfs3 {

static constexpr uint32_t NFS_PROG       = 100003;
static constexpr uint32_t NFS_VERS       = 3;
static constexpr uint32_t NFSPROC3_ACCESS = 4;

std::vector<uint8_t> encode_access_args(const Fh3& fh, uint32_t access_mask) {
    XdrEncoder enc;
    encode_fh3(enc, fh);
    enc.put_uint32(access_mask);
    return enc.release();
}

uint32_t decode_access_reply(const std::vector<uint8_t>& data) {
    XdrDecoder dec(data);
    const uint32_t status = dec.get_uint32();
    // ACCESS3res always carries obj_attributes (post_op_attr) in both OK and fail.
    skip_post_op_attr(dec);
    if (status != 0)
        throw NfsError(status, "ACCESS");
    // ACCESS3resok: access (uint32)
    return dec.get_uint32();
}

uint32_t access(TcpRpcClient& client, const Fh3& fh, uint32_t access_mask) {
    const auto args  = encode_access_args(fh, access_mask);
    const auto reply = client.call(NFS_PROG, NFS_VERS, NFSPROC3_ACCESS, args);
    return decode_access_reply(reply);
}

}  // namespace nfs3
