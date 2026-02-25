#include "dirop.hpp"
#include "compound.hpp"

namespace nfs4 {

void encode_remove(XdrEncoder& enc, const std::string& name) {
    enc.put_uint32(OP_REMOVE);
    enc.put_string(name);
}

void decode_remove_result(XdrDecoder& dec) {
    uint32_t resop  = dec.get_uint32();
    uint32_t status = dec.get_uint32();
    (void)resop;
    if (status != 0) throw Nfs4Error(status, "REMOVE");
    skip_change_info4(dec);  // cinfo
}

void encode_rename(XdrEncoder& enc,
                   const std::string& oldname, const std::string& newname) {
    enc.put_uint32(OP_RENAME);
    enc.put_string(oldname);
    enc.put_string(newname);
}

void decode_rename_result(XdrDecoder& dec) {
    uint32_t resop  = dec.get_uint32();
    uint32_t status = dec.get_uint32();
    (void)resop;
    if (status != 0) throw Nfs4Error(status, "RENAME");
    skip_change_info4(dec);  // source_cinfo
    skip_change_info4(dec);  // target_cinfo
}

}  // namespace nfs4
