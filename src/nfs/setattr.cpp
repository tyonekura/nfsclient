#include "setattr.hpp"
#include "nfs_error.hpp"
#include "../xdr/xdr.hpp"

namespace nfs3 {

static constexpr uint32_t NFS_PROG         = 100003;
static constexpr uint32_t NFS_VERS         = 3;
static constexpr uint32_t NFSPROC3_SETATTR = 2;

std::vector<uint8_t> encode_setattr_args(const Fh3& fh, const Sattr3& attrs,
                                          const SattrGuard3& guard) {
    XdrEncoder enc;
    encode_fh3(enc, fh);
    encode_sattr3(enc, attrs);
    // sattrguard3: bool + optional nfstime3
    enc.put_uint32(guard.check ? 1u : 0u);
    if (guard.check) {
        enc.put_uint32(guard.ctime_sec);
        enc.put_uint32(guard.ctime_nsec);
    }
    return enc.release();
}

void decode_setattr_reply(const std::vector<uint8_t>& data) {
    XdrDecoder dec(data);
    const uint32_t status = dec.get_uint32();
    // SETATTR3res always carries obj_wcc (wcc_data) in both OK and fail.
    skip_wcc_data(dec);
    if (status != 0)
        throw NfsError(status, "SETATTR");
}

void setattr(TcpRpcClient& client, const Fh3& fh, const Sattr3& attrs,
             const SattrGuard3& guard) {
    const auto args  = encode_setattr_args(fh, attrs, guard);
    const auto reply = client.call(NFS_PROG, NFS_VERS, NFSPROC3_SETATTR, args);
    decode_setattr_reply(reply);
}

}  // namespace nfs3
