#include "session41.hpp"
#include "compound.hpp"

namespace nfs4 {

// ── EXCHANGE_ID ───────────────────────────────────────────────────────────────

void encode_exchange_id(XdrEncoder& enc,
                        const std::array<uint8_t, 8>& verifier,
                        const std::string& client_id) {
    enc.put_uint32(OP_EXCHANGE_ID);

    // eia_clientowner: co_verifier(8 fixed) + co_ownerid(opaque<>)
    enc.put_fixed_opaque(verifier.data(), 8);
    enc.put_opaque(reinterpret_cast<const uint8_t*>(client_id.data()), client_id.size());

    // eia_flags: EXCHGID4_FLAG_USE_NON_PNFS = 0x00020000
    enc.put_uint32(0x00020000);

    // eia_state_protect: SP4_NONE = discriminant 0, no body
    enc.put_uint32(0);

    // eia_client_impl_id: empty array (count = 0)
    enc.put_uint32(0);
}

ExchangeIdResult decode_exchange_id_result(XdrDecoder& dec) {
    uint32_t resop  = dec.get_uint32();
    uint32_t status = dec.get_uint32();
    (void)resop;
    if (status != 0) throw Nfs4Error(status, "EXCHANGE_ID");

    ExchangeIdResult r;
    r.clientid   = dec.get_uint64();
    r.sequenceid = dec.get_uint32();

    // eir_flags
    dec.get_uint32();

    // eir_state_protect: SP4_NONE discriminant, no body
    uint32_t sprotect = dec.get_uint32();
    (void)sprotect;

    // eir_server_owner: so_minor_id(u64) + so_major_id(opaque<>)
    dec.get_uint64();
    dec.get_opaque();

    // eir_server_scope: opaque<>
    dec.get_opaque();

    // eir_server_impl_id: array<nfs_impl_id4> — skip count then each element
    uint32_t impl_count = dec.get_uint32();
    for (uint32_t i = 0; i < impl_count; ++i) {
        dec.get_opaque();  // nii_domain
        dec.get_opaque();  // nii_name
        dec.get_uint64();  // nii_date.seconds
        dec.get_uint32();  // nii_date.nseconds
    }

    return r;
}

// ── CREATE_SESSION ────────────────────────────────────────────────────────────

static void encode_channel_attrs(XdrEncoder& enc,
                                  uint32_t maxrqst,
                                  uint32_t maxresp,
                                  uint32_t maxresp_cached) {
    enc.put_uint32(0);             // ca_headerpadsize
    enc.put_uint32(maxrqst);       // ca_maxrequestsize
    enc.put_uint32(maxresp);       // ca_maxresponsesize
    enc.put_uint32(maxresp_cached);// ca_maxresponsesize_cached
    enc.put_uint32(16);            // ca_maxoperations
    enc.put_uint32(1);             // ca_maxrequests
    enc.put_uint32(0);             // ca_rdma_ird: empty array (count=0)
}

void encode_create_session(XdrEncoder& enc,
                           uint64_t clientid,
                           uint32_t sequenceid) {
    enc.put_uint32(OP_CREATE_SESSION);

    enc.put_uint64(clientid);
    enc.put_uint32(sequenceid);
    enc.put_uint32(0);  // csa_flags

    // csa_fore_chan_attrs
    encode_channel_attrs(enc, 65536, 65536, 1024);
    // csa_back_chan_attrs (minimal)
    encode_channel_attrs(enc, 4096, 4096, 256);

    enc.put_uint32(0);  // csa_cb_program

    // csa_sec_parms: array of 1 element, cb_secflavor = AUTH_NONE(0)
    enc.put_uint32(1);
    enc.put_uint32(0);  // AUTH_NONE
}

SessionId41 decode_create_session_result(XdrDecoder& dec) {
    uint32_t resop  = dec.get_uint32();
    uint32_t status = dec.get_uint32();
    (void)resop;
    if (status != 0) throw Nfs4Error(status, "CREATE_SESSION");

    SessionId41 sid{};
    auto raw = dec.get_fixed_opaque(16);
    std::copy(raw.begin(), raw.end(), sid.begin());

    // csr_sequence, csr_flags — skip
    dec.get_uint32();
    dec.get_uint32();

    // csr_fore_chan_attrs: 7 uint32s
    for (int i = 0; i < 7; ++i) dec.get_uint32();
    // csr_back_chan_attrs: 7 uint32s
    for (int i = 0; i < 7; ++i) dec.get_uint32();

    return sid;
}

// ── SEQUENCE ──────────────────────────────────────────────────────────────────

void encode_sequence41(XdrEncoder& enc,
                       const SessionId41& sessionid,
                       uint32_t sequenceid,
                       uint32_t slotid,
                       uint32_t highest_slotid,
                       bool cachethis) {
    enc.put_uint32(OP_SEQUENCE);
    enc.put_fixed_opaque(sessionid.data(), 16);
    enc.put_uint32(sequenceid);
    enc.put_uint32(slotid);
    enc.put_uint32(highest_slotid);
    enc.put_uint32(cachethis ? 1 : 0);
}

void decode_sequence41_result(XdrDecoder& dec) {
    uint32_t resop  = dec.get_uint32();
    uint32_t status = dec.get_uint32();
    (void)resop;
    if (status != 0) throw Nfs4Error(status, "SEQUENCE");

    // sr_sessionid(16) + sr_sequenceid(u32) + sr_slotid(u32) +
    // sr_highest_slotid(u32) + sr_target_highest_slotid(u32) + sr_status_flags(u32)
    dec.get_fixed_opaque(16);
    dec.get_uint32();
    dec.get_uint32();
    dec.get_uint32();
    dec.get_uint32();
    dec.get_uint32();
}

// ── RECLAIM_COMPLETE ──────────────────────────────────────────────────────────

void encode_reclaim_complete(XdrEncoder& enc, bool one_fs) {
    enc.put_uint32(OP_RECLAIM_COMPLETE);
    enc.put_uint32(one_fs ? 1 : 0);
}

void decode_reclaim_complete_result(XdrDecoder& dec) {
    uint32_t resop  = dec.get_uint32();
    uint32_t status = dec.get_uint32();
    (void)resop;
    if (status != 0) throw Nfs4Error(status, "RECLAIM_COMPLETE");
}

// ── DESTROY_SESSION ───────────────────────────────────────────────────────────

void encode_destroy_session(XdrEncoder& enc, const SessionId41& sessionid) {
    enc.put_uint32(OP_DESTROY_SESSION);
    enc.put_fixed_opaque(sessionid.data(), 16);
}

void decode_destroy_session_result(XdrDecoder& dec) {
    uint32_t resop  = dec.get_uint32();
    uint32_t status = dec.get_uint32();
    (void)resop;
    if (status != 0) throw Nfs4Error(status, "DESTROY_SESSION");
}

}  // namespace nfs4
