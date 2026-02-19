#include "portmap.hpp"
#include "../rpc/rpc_client.hpp"
#include "../xdr/xdr.hpp"

#include <stdexcept>

namespace nfs3 {

static constexpr uint32_t PMAP_PROG        = 100000;
static constexpr uint32_t PMAP_VERS        = 2;
static constexpr uint32_t PMAPPROC_GETPORT = 3;
static constexpr uint16_t PMAP_PORT        = 111;
static constexpr uint32_t IPPROTO_TCP_XDR  = 6;

uint16_t getport(const std::string& host, uint32_t prog, uint32_t vers) {
    TcpRpcClient client(host, PMAP_PORT);

    XdrEncoder args;
    args.put_uint32(prog);
    args.put_uint32(vers);
    args.put_uint32(IPPROTO_TCP_XDR);
    args.put_uint32(0);  // port field is ignored in a GETPORT request

    const auto reply = client.call(PMAP_PROG, PMAP_VERS, PMAPPROC_GETPORT, args.bytes());

    XdrDecoder dec(reply);
    const uint32_t port = dec.get_uint32();
    if (port == 0)
        throw std::runtime_error("portmap: program " + std::to_string(prog) +
                                 " version " + std::to_string(vers) +
                                 " is not registered");
    return static_cast<uint16_t>(port);
}

}  // namespace nfs3
