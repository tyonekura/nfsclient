#include "fh_ops.hpp"
#include "compound.hpp"

namespace nfs4 {

// ── Encode ────────────────────────────────────────────────────────────────────

void encode_putrootfh(XdrEncoder& enc) {
    enc.put_uint32(OP_PUTROOTFH);
}

void encode_putfh(XdrEncoder& enc, const Nfs4Fh& fh) {
    enc.put_uint32(OP_PUTFH);
    encode_nfs4fh(enc, fh);
}

void encode_getfh(XdrEncoder& enc) {
    enc.put_uint32(OP_GETFH);
}

void encode_savefh(XdrEncoder& enc) {
    enc.put_uint32(OP_SAVEFH);
}

void encode_restorefh(XdrEncoder& enc) {
    enc.put_uint32(OP_RESTOREFH);
}

void encode_lookupp(XdrEncoder& enc) {
    enc.put_uint32(OP_LOOKUPP);
}

// ── Decode ────────────────────────────────────────────────────────────────────

static void check_op_status(XdrDecoder& dec, uint32_t expected_op, const char* name) {
    uint32_t resop  = dec.get_uint32();
    uint32_t status = dec.get_uint32();
    (void)resop;
    (void)expected_op;
    if (status != 0) throw Nfs4Error(status, name);
}

void decode_putrootfh_result(XdrDecoder& dec) {
    check_op_status(dec, OP_PUTROOTFH, "PUTROOTFH");
}

void decode_putfh_result(XdrDecoder& dec) {
    check_op_status(dec, OP_PUTFH, "PUTFH");
}

Nfs4Fh decode_getfh_result(XdrDecoder& dec) {
    check_op_status(dec, OP_GETFH, "GETFH");
    return decode_nfs4fh(dec);
}

void decode_savefh_result(XdrDecoder& dec) {
    check_op_status(dec, OP_SAVEFH, "SAVEFH");
}

void decode_restorefh_result(XdrDecoder& dec) {
    check_op_status(dec, OP_RESTOREFH, "RESTOREFH");
}

void decode_lookupp_result(XdrDecoder& dec) {
    check_op_status(dec, OP_LOOKUPP, "LOOKUPP");
}

}  // namespace nfs4
