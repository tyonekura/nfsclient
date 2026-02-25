#include "read.hpp"
#include "compound.hpp"

namespace nfs4 {

void encode_read(XdrEncoder& enc, const Stateid4& stateid,
                 uint64_t offset, uint32_t count) {
    enc.put_uint32(OP_READ);
    encode_stateid4(enc, stateid);
    enc.put_uint64(offset);
    enc.put_uint32(count);
}

std::vector<uint8_t> decode_read_result(XdrDecoder& dec) {
    uint32_t resop  = dec.get_uint32();
    uint32_t status = dec.get_uint32();
    (void)resop;
    if (status != 0) throw Nfs4Error(status, "READ");

    /* eof */ dec.get_uint32();
    return dec.get_opaque();
}

}  // namespace nfs4
