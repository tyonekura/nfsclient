#include "access.hpp"
#include "compound.hpp"

namespace nfs4 {

void encode_access(XdrEncoder& enc, uint32_t access_mask) {
    enc.put_uint32(OP_ACCESS);
    enc.put_uint32(access_mask);
}

Access4Result decode_access_result(XdrDecoder& dec) {
    uint32_t resop  = dec.get_uint32();
    uint32_t status = dec.get_uint32();
    (void)resop;
    if (status != 0) throw Nfs4Error(status, "ACCESS");

    Access4Result r;
    r.supported = dec.get_uint32();
    r.access    = dec.get_uint32();
    return r;
}

}  // namespace nfs4
