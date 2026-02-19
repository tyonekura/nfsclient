#include "write.hpp"
#include "../xdr/xdr.hpp"

#include <stdexcept>

namespace nfs3 {

static constexpr uint32_t NFS_PROG       = 100003;
static constexpr uint32_t NFS_VERS       = 3;
static constexpr uint32_t NFSPROC3_WRITE = 7;
static constexpr size_t   WRITE_VERF_SIZE = 8;

std::vector<uint8_t> encode_write_args(const Fh3& fh, uint64_t offset,
                                        Stable3 stable,
                                        const uint8_t* data, size_t data_size) {
    XdrEncoder enc;
    encode_fh3(enc, fh);
    enc.put_uint64(offset);
    enc.put_uint32(static_cast<uint32_t>(data_size));  // count
    enc.put_uint32(static_cast<uint32_t>(stable));
    enc.put_opaque(data, data_size);
    return enc.release();
}

WriteResult decode_write_reply(const std::vector<uint8_t>& data) {
    XdrDecoder dec(data);
    const uint32_t status = dec.get_uint32();
    // WRITE3res always carries file_wcc (wcc_data) in both OK and fail.
    skip_wcc_data(dec);
    if (status != 0)
        throw std::runtime_error("WRITE failed, nfsstat3=" + std::to_string(status));
    // WRITE3resok: count(uint32), committed(uint32), verf(writeverf3 = 8 bytes fixed opaque)
    WriteResult result{};
    result.count     = dec.get_uint32();
    result.committed = static_cast<Stable3>(dec.get_uint32());
    const auto verf  = dec.get_fixed_opaque(WRITE_VERF_SIZE);
    std::copy(verf.begin(), verf.end(), result.verf.begin());
    return result;
}

WriteResult write(TcpRpcClient& client, const Fh3& fh, uint64_t offset,
                  Stable3 stable, const uint8_t* data, size_t data_size) {
    const auto args  = encode_write_args(fh, offset, stable, data, data_size);
    const auto reply = client.call(NFS_PROG, NFS_VERS, NFSPROC3_WRITE, args);
    return decode_write_reply(reply);
}

}  // namespace nfs3
