#pragma once

#include "nfs4_types.hpp"
#include "nfs4_attr.hpp"
#include "nfs4_error.hpp"
#include "../xdr/xdr.hpp"

#include <initializer_list>

namespace nfs4 {

// Encode GETATTR op with the given attribute IDs.
void encode_getattr(XdrEncoder& enc, std::initializer_list<uint32_t> attr_ids);

// Decode GETATTR per-op result.
Fattr4 decode_getattr_result(XdrDecoder& dec);

}  // namespace nfs4
