#include "readdir.hpp"
#include "compound.hpp"
#include "nfs4_attr.hpp"

namespace nfs4 {

void encode_readdir(XdrEncoder& enc,
                    uint64_t cookie,
                    const std::array<uint8_t, 8>& cookieverf,
                    uint32_t dircount,
                    uint32_t maxcount,
                    std::initializer_list<uint32_t> attr_ids) {
    enc.put_uint32(OP_READDIR);
    enc.put_uint64(cookie);
    enc.put_fixed_opaque(cookieverf.data(), 8);
    enc.put_uint32(dircount);
    enc.put_uint32(maxcount);
    encode_attr_request(enc, attr_ids);
}

ReaddirPage4 decode_readdir_result(XdrDecoder& dec) {
    uint32_t resop  = dec.get_uint32();
    uint32_t status = dec.get_uint32();
    (void)resop;
    if (status != 0) throw Nfs4Error(status, "READDIR");

    ReaddirPage4 page;

    auto cv = dec.get_fixed_opaque(8);
    std::copy(cv.begin(), cv.end(), page.cookieverf.begin());

    // dirlist4: value_follows + entries + eof
    while (dec.get_uint32() != 0) {  // value_follows
        Nfs4DirEntry e;
        e.cookie = dec.get_uint64();
        e.name   = dec.get_string();
        e.attrs  = decode_fattr4(dec);
        page.entries.push_back(std::move(e));
    }
    page.eof = (dec.get_uint32() != 0);
    return page;
}

}  // namespace nfs4
