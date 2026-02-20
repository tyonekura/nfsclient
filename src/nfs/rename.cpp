#include "rename.hpp"
#include "nfs_error.hpp"
#include "../xdr/xdr.hpp"

namespace nfs3 {

static constexpr uint32_t NFS_PROG        = 100003;
static constexpr uint32_t NFS_VERS        = 3;
static constexpr uint32_t NFSPROC3_RENAME = 14;

std::vector<uint8_t> encode_rename_args(const Fh3& from_dir, const std::string& from_name,
                                         const Fh3& to_dir,   const std::string& to_name) {
    XdrEncoder enc;
    encode_fh3(enc, from_dir);
    enc.put_string(from_name);
    encode_fh3(enc, to_dir);
    enc.put_string(to_name);
    return enc.release();
}

void decode_rename_reply(const std::vector<uint8_t>& data) {
    XdrDecoder dec(data);
    const uint32_t status = dec.get_uint32();
    // RENAME3res always carries fromdir_wcc and todir_wcc in both OK and fail.
    skip_wcc_data(dec);  // fromdir_wcc
    skip_wcc_data(dec);  // todir_wcc
    if (status != 0)
        throw NfsError(status, "RENAME");
}

void rename(TcpRpcClient& client,
            const Fh3& from_dir, const std::string& from_name,
            const Fh3& to_dir,   const std::string& to_name) {
    const auto args  = encode_rename_args(from_dir, from_name, to_dir, to_name);
    const auto reply = client.call(NFS_PROG, NFS_VERS, NFSPROC3_RENAME, args);
    decode_rename_reply(reply);
}

}  // namespace nfs3
