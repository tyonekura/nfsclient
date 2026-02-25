#pragma once

#include "nfs4_error.hpp"
#include "../xdr/xdr.hpp"

#include <cstdint>

namespace nfs4 {

// ACCESS4 access flags (RFC 7530 §18.1)
constexpr uint32_t ACCESS4_READ    = 0x0001;
constexpr uint32_t ACCESS4_LOOKUP  = 0x0002;
constexpr uint32_t ACCESS4_MODIFY  = 0x0004;
constexpr uint32_t ACCESS4_EXTEND  = 0x0008;
constexpr uint32_t ACCESS4_DELETE  = 0x0010;
constexpr uint32_t ACCESS4_EXECUTE = 0x0020;

// Result of ACCESS4: supported + access bitmasks.
struct Access4Result {
    uint32_t supported{};
    uint32_t access{};
};

void encode_access(XdrEncoder& enc, uint32_t access_mask);
Access4Result decode_access_result(XdrDecoder& dec);

}  // namespace nfs4
