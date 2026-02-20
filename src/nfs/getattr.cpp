#include "getattr.hpp"
#include "nfs_error.hpp"
#include "../xdr/xdr.hpp"

namespace nfs3 {

static constexpr uint32_t NFS_PROG        = 100003;
static constexpr uint32_t NFS_VERS        = 3;
static constexpr uint32_t NFSPROC3_GETATTR = 1;

std::vector<uint8_t> encode_getattr_args(const Fh3& fh) {
    XdrEncoder enc;
    encode_fh3(enc, fh);
    return enc.release();
}

Fattr3 decode_getattr_reply(const std::vector<uint8_t>& data) {
    XdrDecoder dec(data);
    const uint32_t status = dec.get_uint32();
    if (status != 0)
        throw NfsError(status, "GETATTR");
    // GETATTR3resok: obj_attributes (fattr3, always present)
    return decode_fattr3(dec);
}

Fattr3 getattr(TcpRpcClient& client, const Fh3& fh) {
    const auto args  = encode_getattr_args(fh);
    const auto reply = client.call(NFS_PROG, NFS_VERS, NFSPROC3_GETATTR, args);
    return decode_getattr_reply(reply);
}

}  // namespace nfs3
