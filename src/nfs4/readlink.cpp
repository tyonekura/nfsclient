#include "readlink.hpp"
#include "compound.hpp"

namespace nfs4 {

void encode_readlink(XdrEncoder& enc) {
    enc.put_uint32(OP_READLINK);
}

std::string decode_readlink_result(XdrDecoder& dec) {
    uint32_t resop  = dec.get_uint32();
    uint32_t status = dec.get_uint32();
    (void)resop;
    if (status != 0) throw Nfs4Error(status, "READLINK");
    return dec.get_string();
}

}  // namespace nfs4
