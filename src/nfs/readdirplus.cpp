#include "readdirplus.hpp"
#include "nfs_error.hpp"
#include "../xdr/xdr.hpp"

namespace nfs3 {

static constexpr uint32_t NFS_PROG             = 100003;
static constexpr uint32_t NFS_VERS             = 3;
static constexpr uint32_t NFSPROC3_READDIRPLUS = 17;
static constexpr size_t   COOKIEVERF_SIZE      = 8;

std::vector<uint8_t> encode_readdirplus_args(const Fh3& dir,
                                              uint64_t cookie,
                                              const std::array<uint8_t, 8>& cookieverf,
                                              uint32_t dircount,
                                              uint32_t maxcount) {
    XdrEncoder enc;
    encode_fh3(enc, dir);
    enc.put_uint64(cookie);
    enc.put_fixed_opaque(cookieverf.data(), COOKIEVERF_SIZE);
    enc.put_uint32(dircount);
    enc.put_uint32(maxcount);
    return enc.release();
}

ReaddirplusPage decode_readdirplus_reply(const std::vector<uint8_t>& data) {
    XdrDecoder dec(data);
    const uint32_t status = dec.get_uint32();
    // dir_attributes present in both OK and fail.
    skip_post_op_attr(dec);
    if (status != 0)
        throw NfsError(status, "READDIRPLUS");

    // cookieverf3: fixed 8-byte opaque
    ReaddirplusPage page{};
    const auto cv = dec.get_fixed_opaque(COOKIEVERF_SIZE);
    std::copy(cv.begin(), cv.end(), page.cookieverf.begin());

    // dirlistplus3: XDR linked list of entryplus3
    while (dec.get_uint32() != 0) {
        DirEntryPlus3 entry{};
        entry.fileid = dec.get_uint64();
        entry.name   = dec.get_string();
        entry.cookie = dec.get_uint64();

        // name_attributes: post_op_attr (bool + optional fattr3)
        entry.has_attrs = (dec.get_uint32() != 0);
        if (entry.has_attrs)
            entry.attrs = decode_fattr3(dec);

        // name_handle: post_op_fh3 (bool + optional fh3)
        entry.has_fh = (dec.get_uint32() != 0);
        if (entry.has_fh)
            entry.fh = decode_fh3(dec);

        page.entries.push_back(std::move(entry));
    }

    page.eof = (dec.get_uint32() != 0);
    return page;
}

ReaddirplusPage readdirplus_page(TcpRpcClient& client, const Fh3& dir,
                                  uint64_t cookie,
                                  const std::array<uint8_t, 8>& cookieverf,
                                  uint32_t dircount, uint32_t maxcount) {
    const auto args  = encode_readdirplus_args(dir, cookie, cookieverf,
                                               dircount, maxcount);
    const auto reply = client.call(NFS_PROG, NFS_VERS, NFSPROC3_READDIRPLUS, args);
    return decode_readdirplus_reply(reply);
}

std::vector<DirEntryPlus3> readdirplus(TcpRpcClient& client, const Fh3& dir,
                                        uint32_t dircount, uint32_t maxcount) {
    std::vector<DirEntryPlus3> all;
    uint64_t cookie = 0;
    std::array<uint8_t, 8> cookieverf{};

    for (;;) {
        auto page = readdirplus_page(client, dir, cookie, cookieverf,
                                     dircount, maxcount);
        for (auto& e : page.entries) {
            cookie = e.cookie;
            all.push_back(std::move(e));
        }
        cookieverf = page.cookieverf;
        if (page.eof) break;
    }
    return all;
}

}  // namespace nfs3
