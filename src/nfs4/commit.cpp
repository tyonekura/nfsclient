#include "commit.hpp"
#include "compound.hpp"

namespace nfs4 {

void encode_commit(XdrEncoder& enc, uint64_t offset, uint32_t count) {
    enc.put_uint32(OP_COMMIT);
    enc.put_uint64(offset);
    enc.put_uint32(count);
}

std::array<uint8_t, 8> decode_commit_result(XdrDecoder& dec) {
    uint32_t resop  = dec.get_uint32();
    uint32_t status = dec.get_uint32();
    (void)resop;
    if (status != 0) throw Nfs4Error(status, "COMMIT");

    std::array<uint8_t, 8> verf{};
    auto v = dec.get_fixed_opaque(8);
    std::copy(v.begin(), v.end(), verf.begin());
    return verf;
}

}  // namespace nfs4
