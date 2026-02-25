#include "create.hpp"
#include "compound.hpp"
#include "nfs4_attr.hpp"

namespace nfs4 {

void encode_create_dir(XdrEncoder& enc, const std::string& name, const Sattr4& attrs) {
    enc.put_uint32(OP_CREATE);
    enc.put_uint32(static_cast<uint32_t>(Ftype4::NF4DIR));  // createtype4
    // no extra data for NF4DIR
    enc.put_string(name);
    encode_fattr4(enc, attrs);
}

void encode_create_symlink(XdrEncoder& enc,
                            const std::string& name,
                            const std::string& target,
                            const Sattr4& attrs) {
    enc.put_uint32(OP_CREATE);
    enc.put_uint32(static_cast<uint32_t>(Ftype4::NF4LNK));
    enc.put_string(target);  // linktext4 linkdata (in the createtype4 union)
    enc.put_string(name);
    encode_fattr4(enc, attrs);
}

void decode_create_result(XdrDecoder& dec) {
    uint32_t resop  = dec.get_uint32();
    uint32_t status = dec.get_uint32();
    (void)resop;
    if (status != 0) throw Nfs4Error(status, "CREATE");
    skip_change_info4(dec);  // cinfo
    // attrset bitmap — discard
    uint32_t bm_size = dec.get_uint32();
    for (uint32_t i = 0; i < bm_size; ++i) dec.get_uint32();
}

}  // namespace nfs4
