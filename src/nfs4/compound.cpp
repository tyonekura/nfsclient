#include "compound.hpp"
#include "../xdr/xdr.hpp"

namespace nfs4 {

static constexpr uint32_t NFS4_PROG         = 100003;
static constexpr uint32_t NFS4_VERS         = 4;
static constexpr uint32_t NFS4_PROC_COMPOUND = 1;

std::vector<uint8_t> call_compound(TcpRpcClient& rpc,
                                    const std::string& tag,
                                    const std::vector<uint8_t>& ops_bytes,
                                    uint32_t num_ops) {
    // Encode COMPOUND4args header: tag, minorversion=0, numops
    XdrEncoder hdr;
    hdr.put_string(tag);
    hdr.put_uint32(0);        // minorversion = 0
    hdr.put_uint32(num_ops);
    auto hdr_bytes = hdr.release();

    // Concatenate header + ops
    std::vector<uint8_t> args;
    args.reserve(hdr_bytes.size() + ops_bytes.size());
    args.insert(args.end(), hdr_bytes.begin(), hdr_bytes.end());
    args.insert(args.end(), ops_bytes.begin(), ops_bytes.end());

    return rpc.call(NFS4_PROG, NFS4_VERS, NFS4_PROC_COMPOUND, args);
}

void check_compound_status(XdrDecoder& dec) {
    uint32_t status = dec.get_uint32();
    if (status != 0) throw Nfs4Error(status, "COMPOUND");
    dec.get_string();    // echoed tag
    dec.get_uint32();    // numops in reply
}

}  // namespace nfs4
