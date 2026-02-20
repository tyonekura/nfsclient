#include "commit.hpp"
#include "nfs_error.hpp"
#include "../xdr/xdr.hpp"

namespace nfs3 {

static constexpr uint32_t NFS_PROG        = 100003;
static constexpr uint32_t NFS_VERS        = 3;
static constexpr uint32_t NFSPROC3_COMMIT = 21;
static constexpr size_t   VERF_SIZE       = 8;

std::vector<uint8_t> encode_commit_args(const Fh3& fh,
                                         uint64_t offset,
                                         uint32_t count) {
    XdrEncoder enc;
    encode_fh3(enc, fh);
    enc.put_uint64(offset);
    enc.put_uint32(count);
    return enc.release();
}

CommitVerf3 decode_commit_reply(const std::vector<uint8_t>& data) {
    XdrDecoder dec(data);
    const uint32_t status = dec.get_uint32();
    // COMMIT3res always carries file_wcc in both OK and fail.
    skip_wcc_data(dec);
    if (status != 0)
        throw NfsError(status, "COMMIT");
    // COMMIT3resok: writeverf3 (8-byte fixed opaque)
    CommitVerf3 verf{};
    const auto v = dec.get_fixed_opaque(VERF_SIZE);
    std::copy(v.begin(), v.end(), verf.begin());
    return verf;
}

CommitVerf3 commit(TcpRpcClient& client, const Fh3& fh,
                   uint64_t offset, uint32_t count) {
    const auto args  = encode_commit_args(fh, offset, count);
    const auto reply = client.call(NFS_PROG, NFS_VERS, NFSPROC3_COMMIT, args);
    return decode_commit_reply(reply);
}

}  // namespace nfs3
