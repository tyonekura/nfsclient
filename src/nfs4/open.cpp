#include "open.hpp"
#include "compound.hpp"
#include "nfs4_attr.hpp"

namespace nfs4 {

// Common OPEN args prefix: seqid, share_access, share_deny, open_owner4
static void encode_open_prefix(XdrEncoder& enc,
                                uint32_t seqid,
                                uint32_t share_access,
                                uint64_t clientid,
                                const std::string& owner) {
    enc.put_uint32(OP_OPEN);
    enc.put_uint32(seqid);
    enc.put_uint32(share_access);
    enc.put_uint32(OPEN4_SHARE_DENY_NONE);
    // open_owner4: clientid(u64) + opaque owner<>
    enc.put_uint64(clientid);
    enc.put_opaque(reinterpret_cast<const uint8_t*>(owner.data()), owner.size());
}

void encode_open_nocreate(XdrEncoder& enc,
                          uint32_t seqid,
                          uint32_t share_access,
                          uint64_t clientid,
                          const std::string& owner,
                          const std::string& name) {
    encode_open_prefix(enc, seqid, share_access, clientid, owner);
    // openflag4: OPEN4_NOCREATE (no additional data)
    enc.put_uint32(OPEN4_NOCREATE);
    // open_claim4: CLAIM_NULL + filename
    enc.put_uint32(CLAIM_NULL);
    enc.put_string(name);
}

void encode_open_create(XdrEncoder& enc,
                        uint32_t seqid,
                        uint32_t share_access,
                        uint64_t clientid,
                        const std::string& owner,
                        const std::string& name,
                        const Sattr4& attrs) {
    encode_open_prefix(enc, seqid, share_access, clientid, owner);
    // openflag4: OPEN4_CREATE + createhow4(UNCHECKED4) + fattr4
    enc.put_uint32(OPEN4_CREATE);
    enc.put_uint32(UNCHECKED4);
    encode_fattr4(enc, attrs);
    // open_claim4: CLAIM_NULL + filename
    enc.put_uint32(CLAIM_NULL);
    enc.put_string(name);
}

Open4Result decode_open_result(XdrDecoder& dec) {
    uint32_t resop  = dec.get_uint32();
    uint32_t status = dec.get_uint32();
    (void)resop;
    if (status != 0) throw Nfs4Error(status, "OPEN");

    Open4Result r;
    r.stateid = decode_stateid4(dec);
    skip_change_info4(dec);           // cinfo
    r.rflags  = dec.get_uint32();

    // attrset bitmap (for EXCLUSIVE4, otherwise empty — must still be read)
    auto bm_size = dec.get_uint32();
    for (uint32_t i = 0; i < bm_size; ++i) dec.get_uint32();

    // open_delegation4: delegation_type (always read, even if NONE=0)
    uint32_t deleg_type = dec.get_uint32();
    if (deleg_type == 1) {
        // OPEN_DELEGATE_READ: stateid4 + recall(bool) + nfsace4
        decode_stateid4(dec);
        dec.get_uint32();  // recall bool
        // nfsace4: type(u32) + flag(u32) + access_mask(u32) + who(string)
        dec.get_uint32(); dec.get_uint32(); dec.get_uint32();
        dec.get_string();
    } else if (deleg_type == 2) {
        // OPEN_DELEGATE_WRITE: stateid4 + recall(bool) + space_limit + nfsace4
        decode_stateid4(dec);
        dec.get_uint32();   // recall bool
        dec.get_uint32();   // limitby (nfs_limit_by4)
        dec.get_uint32();   // num_blocks or filesize (u32)
        dec.get_uint32();   // bytes_per_block or padding (u32)
        // nfsace4
        dec.get_uint32(); dec.get_uint32(); dec.get_uint32();
        dec.get_string();
    }
    // OPEN_DELEGATE_NONE (0): nothing to read

    return r;
}

void encode_open_confirm(XdrEncoder& enc, const Stateid4& stateid, uint32_t seqid) {
    enc.put_uint32(OP_OPEN_CONFIRM);
    encode_stateid4(enc, stateid);
    enc.put_uint32(seqid);
}

Stateid4 decode_open_confirm_result(XdrDecoder& dec) {
    uint32_t resop  = dec.get_uint32();
    uint32_t status = dec.get_uint32();
    (void)resop;
    if (status != 0) throw Nfs4Error(status, "OPEN_CONFIRM");
    return decode_stateid4(dec);
}

void encode_close(XdrEncoder& enc, uint32_t seqid, const Stateid4& stateid) {
    enc.put_uint32(OP_CLOSE);
    enc.put_uint32(seqid);
    encode_stateid4(enc, stateid);
}

void decode_close_result(XdrDecoder& dec) {
    uint32_t resop  = dec.get_uint32();
    uint32_t status = dec.get_uint32();
    (void)resop;
    if (status != 0) throw Nfs4Error(status, "CLOSE");
    // CLOSE4resok: stateid4 (all-zeros after close) — discard
    decode_stateid4(dec);
}

void encode_renew(XdrEncoder& enc, uint64_t clientid) {
    enc.put_uint32(OP_RENEW);
    enc.put_uint64(clientid);
}

void decode_renew_result(XdrDecoder& dec) {
    uint32_t resop  = dec.get_uint32();
    uint32_t status = dec.get_uint32();
    (void)resop;
    if (status != 0) throw Nfs4Error(status, "RENEW");
}

}  // namespace nfs4
