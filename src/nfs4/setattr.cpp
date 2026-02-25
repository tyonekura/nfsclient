#include "setattr.hpp"
#include "compound.hpp"
#include "nfs4_attr.hpp"

namespace nfs4 {

void encode_setattr(XdrEncoder& enc, const Stateid4& stateid, const Sattr4& attrs) {
    enc.put_uint32(OP_SETATTR);
    encode_stateid4(enc, stateid);
    encode_fattr4(enc, attrs);
}

void decode_setattr_result(XdrDecoder& dec) {
    uint32_t resop  = dec.get_uint32();
    uint32_t status = dec.get_uint32();
    (void)resop;
    if (status != 0) throw Nfs4Error(status, "SETATTR");
    // attrsset bitmap — discard
    uint32_t bm_size = dec.get_uint32();
    for (uint32_t i = 0; i < bm_size; ++i) dec.get_uint32();
}

}  // namespace nfs4
