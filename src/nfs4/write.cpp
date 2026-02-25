#include "write.hpp"
#include "compound.hpp"

namespace nfs4 {

void encode_write(XdrEncoder& enc, const Stateid4& stateid,
                  uint64_t offset, Stable4 stable,
                  const uint8_t* data, uint32_t len) {
    enc.put_uint32(OP_WRITE);
    encode_stateid4(enc, stateid);
    enc.put_uint64(offset);
    enc.put_uint32(static_cast<uint32_t>(stable));
    enc.put_opaque(data, len);
}

Nfs4WriteResult decode_write_result(XdrDecoder& dec) {
    uint32_t resop  = dec.get_uint32();
    uint32_t status = dec.get_uint32();
    (void)resop;
    if (status != 0) throw Nfs4Error(status, "WRITE");

    Nfs4WriteResult r;
    r.count     = dec.get_uint32();
    r.committed = static_cast<Stable4>(dec.get_uint32());
    auto v      = dec.get_fixed_opaque(8);
    std::copy(v.begin(), v.end(), r.verf.begin());
    return r;
}

}  // namespace nfs4
