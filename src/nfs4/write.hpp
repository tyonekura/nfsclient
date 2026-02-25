#pragma once

#include "nfs4_types.hpp"
#include "nfs4_error.hpp"
#include "../xdr/xdr.hpp"

#include <cstdint>

namespace nfs4 {

void encode_write(XdrEncoder& enc, const Stateid4& stateid,
                  uint64_t offset, Stable4 stable,
                  const uint8_t* data, uint32_t len);

Nfs4WriteResult decode_write_result(XdrDecoder& dec);

}  // namespace nfs4
