#include "getattr.hpp"
#include "compound.hpp"
#include "nfs4_attr.hpp"

namespace nfs4 {

void encode_getattr(XdrEncoder& enc, std::initializer_list<uint32_t> attr_ids) {
    enc.put_uint32(OP_GETATTR);
    encode_attr_request(enc, attr_ids);
}

Fattr4 decode_getattr_result(XdrDecoder& dec) {
    uint32_t resop  = dec.get_uint32();
    uint32_t status = dec.get_uint32();
    (void)resop;
    if (status != 0) throw Nfs4Error(status, "GETATTR");
    return decode_fattr4(dec);
}

}  // namespace nfs4
