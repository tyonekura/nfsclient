#include "nfs4_attr.hpp"
#include "nfs4_types.hpp"
#include "../xdr/xdr.hpp"

namespace nfs4 {

void bitmap4_set(std::vector<uint32_t>& bm, uint32_t id) {
    uint32_t word = id / 32;
    uint32_t bit  = 1u << (31 - (id % 32));
    if (bm.size() <= word) bm.resize(word + 1, 0);
    bm[word] |= bit;
}

bool bitmap4_test(const std::vector<uint32_t>& bm, uint32_t id) {
    uint32_t word = id / 32;
    uint32_t bit  = 1u << (31 - (id % 32));
    if (bm.size() <= word) return false;
    return (bm[word] & bit) != 0;
}

void encode_bitmap4(XdrEncoder& enc, const std::vector<uint32_t>& bm) {
    enc.put_uint32(static_cast<uint32_t>(bm.size()));
    for (uint32_t w : bm) enc.put_uint32(w);
}

std::vector<uint32_t> decode_bitmap4(XdrDecoder& dec) {
    uint32_t count = dec.get_uint32();
    std::vector<uint32_t> bm(count);
    for (uint32_t& w : bm) w = dec.get_uint32();
    return bm;
}

std::vector<uint32_t> make_bitmap4(std::initializer_list<uint32_t> ids) {
    std::vector<uint32_t> bm;
    for (uint32_t id : ids) bitmap4_set(bm, id);
    return bm;
}

void encode_attr_request(XdrEncoder& enc, std::initializer_list<uint32_t> ids) {
    encode_bitmap4(enc, make_bitmap4(ids));
}

Fattr4 decode_fattr4(XdrDecoder& dec) {
    auto bm       = decode_bitmap4(dec);
    auto attrlist = dec.get_opaque();
    XdrDecoder ad(attrlist);

    Fattr4 a;

    // Attributes are decoded in ascending ID order matching the bitmap.
    if (bitmap4_test(bm, attr::TYPE)) {
        a.type = static_cast<Ftype4>(ad.get_uint32());
    }
    if (bitmap4_test(bm, attr::CHANGE)) {
        a.change = ad.get_uint64();
    }
    if (bitmap4_test(bm, attr::SIZE)) {
        a.size = ad.get_uint64();
    }
    if (bitmap4_test(bm, attr::FSID)) {
        // fsid4: major(uint64) + minor(uint64) — not exposed in Fattr4
        ad.get_uint64();
        ad.get_uint64();
    }
    if (bitmap4_test(bm, attr::FILEID)) {
        a.fileid = ad.get_uint64();
    }
    if (bitmap4_test(bm, attr::MODE)) {
        a.mode = ad.get_uint32();
    }
    if (bitmap4_test(bm, attr::NUMLINKS)) {
        a.numlinks = ad.get_uint32();
    }
    if (bitmap4_test(bm, attr::OWNER)) {
        a.owner = ad.get_string();
    }
    if (bitmap4_test(bm, attr::OWNER_GROUP)) {
        a.owner_group = ad.get_string();
    }
    if (bitmap4_test(bm, attr::SPACE_USED)) {
        a.space_used = ad.get_uint64();
    }
    if (bitmap4_test(bm, attr::TIME_ACCESS)) {
        a.time_access = decode_nfstime4(ad);
    }
    if (bitmap4_test(bm, attr::TIME_METADATA)) {
        a.time_metadata = decode_nfstime4(ad);
    }
    if (bitmap4_test(bm, attr::TIME_MODIFY)) {
        a.time_modify = decode_nfstime4(ad);
    }
    if (bitmap4_test(bm, attr::MOUNTED_ON_FILEID)) {
        a.mounted_on_fileid = ad.get_uint64();
    }

    return a;
}

void encode_fattr4(XdrEncoder& enc, const Sattr4& attrs) {
    // Build bitmap — attributes encoded in ascending ID order.
    std::vector<uint32_t> bm;
    if (attrs.size)         bitmap4_set(bm, attr::SIZE);
    if (attrs.mode)         bitmap4_set(bm, attr::MODE);
    if (attrs.owner)        bitmap4_set(bm, attr::OWNER);
    if (attrs.owner_group)  bitmap4_set(bm, attr::OWNER_GROUP);
    if (attrs.time_access)  bitmap4_set(bm, attr::TIME_ACCESS_SET);
    if (attrs.time_modify)  bitmap4_set(bm, attr::TIME_MODIFY_SET);

    // Encode attrlist into a temporary buffer (ascending ID order).
    XdrEncoder ae;
    if (attrs.size)  ae.put_uint64(*attrs.size);
    if (attrs.mode)  ae.put_uint32(*attrs.mode);
    if (attrs.owner) ae.put_string(*attrs.owner);
    if (attrs.owner_group) ae.put_string(*attrs.owner_group);
    if (attrs.time_access) {
        // settime4: time_how4=SET_TO_CLIENT_TIME(1) + nfstime4
        ae.put_uint32(1);
        ae.put_uint64(static_cast<uint64_t>(attrs.time_access->seconds));
        ae.put_uint32(attrs.time_access->nseconds);
    }
    if (attrs.time_modify) {
        ae.put_uint32(1);
        ae.put_uint64(static_cast<uint64_t>(attrs.time_modify->seconds));
        ae.put_uint32(attrs.time_modify->nseconds);
    }

    encode_bitmap4(enc, bm);
    enc.put_opaque(ae.bytes());
}

}  // namespace nfs4
