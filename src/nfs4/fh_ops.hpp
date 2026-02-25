#pragma once

#include "nfs4_types.hpp"
#include "nfs4_error.hpp"
#include "../xdr/xdr.hpp"

namespace nfs4 {

// ── Encode ops (append to a shared XdrEncoder) ───────────────────────────────

void encode_putrootfh(XdrEncoder& enc);
void encode_putfh(XdrEncoder& enc, const Nfs4Fh& fh);
void encode_getfh(XdrEncoder& enc);
void encode_savefh(XdrEncoder& enc);
void encode_restorefh(XdrEncoder& enc);
void encode_lookupp(XdrEncoder& enc);

// ── Decode per-op results ─────────────────────────────────────────────────────
// Each function reads: [resop:u32] [status:u32] [result data if OK]
// Throws Nfs4Error on non-zero status.

void   decode_putrootfh_result(XdrDecoder& dec);
void   decode_putfh_result(XdrDecoder& dec);
Nfs4Fh decode_getfh_result(XdrDecoder& dec);
void   decode_savefh_result(XdrDecoder& dec);
void   decode_restorefh_result(XdrDecoder& dec);
void   decode_lookupp_result(XdrDecoder& dec);

}  // namespace nfs4
