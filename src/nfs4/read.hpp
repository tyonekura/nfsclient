#pragma once

#include "nfs4_types.hpp"
#include "nfs4_error.hpp"
#include "../xdr/xdr.hpp"

#include <cstdint>
#include <vector>

namespace nfs4 {

void encode_read(XdrEncoder& enc, const Stateid4& stateid,
                 uint64_t offset, uint32_t count);

// Returns the data bytes; eof is discarded (callers check data.size() < count).
std::vector<uint8_t> decode_read_result(XdrDecoder& dec);

}  // namespace nfs4
