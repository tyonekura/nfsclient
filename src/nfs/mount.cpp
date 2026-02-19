#include "mount.hpp"
#include "portmap.hpp"
#include "../rpc/rpc_client.hpp"
#include "../xdr/xdr.hpp"

#include <stdexcept>

namespace nfs3 {

static constexpr uint32_t MOUNT_PROG      = 100005;
static constexpr uint32_t MOUNT_VERS      = 3;
static constexpr uint32_t MOUNTPROC3_MNT  = 1;

Fh3 mnt(const std::string& host, const std::string& export_path) {
    const uint16_t port = getport(host, MOUNT_PROG, MOUNT_VERS);
    TcpRpcClient client(host, port);

    XdrEncoder args;
    args.put_string(export_path);

    const auto reply = client.call(MOUNT_PROG, MOUNT_VERS, MOUNTPROC3_MNT, args.bytes());

    XdrDecoder dec(reply);
    const uint32_t status = dec.get_uint32();
    if (status != 0)
        throw std::runtime_error("MOUNT MNT3 failed, mountstat3=" +
                                 std::to_string(status));

    // fhandle3: variable-length opaque (RFC 1813 Appendix I)
    return Fh3{dec.get_opaque()};
    // auth_flavors array follows but we don't need it
}

}  // namespace nfs3
