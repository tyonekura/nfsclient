#include "setclientid.hpp"
#include "compound.hpp"

namespace nfs4 {

void encode_setclientid(XdrEncoder& enc,
                        const std::array<uint8_t, 8>& verifier,
                        const std::string& client_id,
                        uint32_t cb_program) {
    enc.put_uint32(OP_SETCLIENTID);

    // nfs_client_id4: verifier4 (8 fixed bytes) + opaque id<>
    enc.put_fixed_opaque(verifier.data(), 8);
    enc.put_opaque(reinterpret_cast<const uint8_t*>(client_id.data()), client_id.size());

    // cb_client4: cb_program(u32) + netaddr4{na_r_netid, na_r_addr}
    enc.put_uint32(cb_program);
    enc.put_string("tcp");   // na_r_netid
    enc.put_string("0.0.0.0.0.0");  // na_r_addr (null address — no callbacks)

    // callback_ident
    enc.put_uint32(0);
}

SetclientidResult decode_setclientid_result(XdrDecoder& dec) {
    uint32_t resop  = dec.get_uint32();
    uint32_t status = dec.get_uint32();
    (void)resop;
    if (status != 0) throw Nfs4Error(status, "SETCLIENTID");

    SetclientidResult r;
    r.clientid = dec.get_uint64();
    auto cv    = dec.get_fixed_opaque(8);
    std::copy(cv.begin(), cv.end(), r.confirm_verifier.begin());
    return r;
}

void encode_setclientid_confirm(XdrEncoder& enc,
                                 uint64_t clientid,
                                 const std::array<uint8_t, 8>& confirm_verifier) {
    enc.put_uint32(OP_SETCLIENTID_CONFIRM);
    enc.put_uint64(clientid);
    enc.put_fixed_opaque(confirm_verifier.data(), 8);
}

void decode_setclientid_confirm_result(XdrDecoder& dec) {
    uint32_t resop  = dec.get_uint32();
    uint32_t status = dec.get_uint32();
    (void)resop;
    if (status != 0) throw Nfs4Error(status, "SETCLIENTID_CONFIRM");
}

}  // namespace nfs4
