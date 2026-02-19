#include "read.hpp"
#include "../xdr/xdr.hpp"

#include <stdexcept>

namespace nfs3 {

static constexpr uint32_t NFS_PROG      = 100003;
static constexpr uint32_t NFS_VERS      = 3;
static constexpr uint32_t NFSPROC3_READ = 6;

std::vector<uint8_t> encode_read_args(const Fh3& fh, uint64_t offset, uint32_t count) {
    XdrEncoder enc;
    encode_fh3(enc, fh);
    enc.put_uint64(offset);
    enc.put_uint32(count);
    return enc.release();
}

std::vector<uint8_t> decode_read_reply(const std::vector<uint8_t>& data) {
    XdrDecoder dec(data);
    const uint32_t status = dec.get_uint32();
    // READ3res always carries file_attributes (post_op_attr) in both OK and fail.
    skip_post_op_attr(dec);
    if (status != 0)
        throw std::runtime_error("READ failed, nfsstat3=" + std::to_string(status));
    // READ3resok: count(uint32), eof(bool/uint32), data(opaque)
    /* count */ dec.get_uint32();
    /* eof   */ dec.get_uint32();
    return dec.get_opaque();
}

std::vector<uint8_t> read(TcpRpcClient& client, const Fh3& fh,
                           uint64_t offset, uint32_t count) {
    const auto args  = encode_read_args(fh, offset, count);
    const auto reply = client.call(NFS_PROG, NFS_VERS, NFSPROC3_READ, args);
    return decode_read_reply(reply);
}

}  // namespace nfs3
