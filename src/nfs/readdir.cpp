#include "readdir.hpp"
#include "nfs_error.hpp"
#include "../xdr/xdr.hpp"

namespace nfs3 {

static constexpr uint32_t NFS_PROG          = 100003;
static constexpr uint32_t NFS_VERS          = 3;
static constexpr uint32_t NFSPROC3_READDIR  = 16;
static constexpr size_t   COOKIEVERF_SIZE   = 8;

std::vector<uint8_t> encode_readdir_args(const Fh3& dir,
                                          uint64_t cookie,
                                          const std::array<uint8_t, 8>& cookieverf,
                                          uint32_t count) {
    XdrEncoder enc;
    encode_fh3(enc, dir);
    enc.put_uint64(cookie);
    enc.put_fixed_opaque(cookieverf.data(), COOKIEVERF_SIZE);
    enc.put_uint32(count);
    return enc.release();
}

ReaddirPage decode_readdir_reply(const std::vector<uint8_t>& data) {
    XdrDecoder dec(data);
    const uint32_t status = dec.get_uint32();
    // READDIR3resfail: dir_attributes (post_op_attr)
    // READDIR3resok:   dir_attributes (post_op_attr), cookieverf, dirlist3
    // dir_attributes is present in both branches.
    skip_post_op_attr(dec);
    if (status != 0)
        throw NfsError(status, "READDIR");

    // cookieverf3: fixed 8-byte opaque
    ReaddirPage page{};
    const auto cv = dec.get_fixed_opaque(COOKIEVERF_SIZE);
    std::copy(cv.begin(), cv.end(), page.cookieverf.begin());

    // dirlist3: XDR linked list of entry3
    // Each entry is preceded by a value_follows bool.
    while (dec.get_uint32() != 0) {
        DirEntry3 entry{};
        entry.fileid = dec.get_uint64();
        entry.name   = dec.get_string();
        entry.cookie = dec.get_uint64();
        page.entries.push_back(std::move(entry));
    }

    // eof bool
    page.eof = (dec.get_uint32() != 0);
    return page;
}

ReaddirPage readdir_page(TcpRpcClient& client, const Fh3& dir,
                          uint64_t cookie,
                          const std::array<uint8_t, 8>& cookieverf,
                          uint32_t count) {
    const auto args  = encode_readdir_args(dir, cookie, cookieverf, count);
    const auto reply = client.call(NFS_PROG, NFS_VERS, NFSPROC3_READDIR, args);
    return decode_readdir_reply(reply);
}

std::vector<DirEntry3> readdir(TcpRpcClient& client, const Fh3& dir, uint32_t count) {
    std::vector<DirEntry3> all;
    uint64_t cookie = 0;
    std::array<uint8_t, 8> cookieverf{};

    for (;;) {
        auto page = readdir_page(client, dir, cookie, cookieverf, count);
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
