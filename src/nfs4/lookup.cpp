#include "lookup.hpp"
#include "compound.hpp"

namespace nfs4 {

void encode_lookup(XdrEncoder& enc, const std::string& name) {
    enc.put_uint32(OP_LOOKUP);
    enc.put_string(name);
}

void decode_lookup_result(XdrDecoder& dec) {
    uint32_t resop  = dec.get_uint32();
    uint32_t status = dec.get_uint32();
    (void)resop;
    if (status != 0) throw Nfs4Error(status, "LOOKUP");
}

}  // namespace nfs4
