#pragma once

#include "nfs4_error.hpp"
#include "../xdr/xdr.hpp"

#include <string>

namespace nfs4 {

void encode_readlink(XdrEncoder& enc);
std::string decode_readlink_result(XdrDecoder& dec);

}  // namespace nfs4
