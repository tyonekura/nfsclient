#pragma once

#include "nfs4_types.hpp"
#include "nfs4_attr.hpp"
#include "nfs4_error.hpp"
#include "../xdr/xdr.hpp"

namespace nfs4 {

// stateid4 all-zeros = anonymous stateid for SETATTR without an open state
void encode_setattr(XdrEncoder& enc, const Stateid4& stateid, const Sattr4& attrs);
void decode_setattr_result(XdrDecoder& dec);

}  // namespace nfs4
