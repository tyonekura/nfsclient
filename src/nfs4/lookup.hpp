#pragma once

#include "nfs4_types.hpp"
#include "nfs4_error.hpp"
#include "../xdr/xdr.hpp"

#include <string>

namespace nfs4 {

void encode_lookup(XdrEncoder& enc, const std::string& name);
void decode_lookup_result(XdrDecoder& dec);

}  // namespace nfs4
