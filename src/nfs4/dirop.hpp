#pragma once

#include "nfs4_types.hpp"
#include "nfs4_error.hpp"
#include "../xdr/xdr.hpp"

#include <string>

namespace nfs4 {

void encode_remove(XdrEncoder& enc, const std::string& name);
void decode_remove_result(XdrDecoder& dec);

void encode_rename(XdrEncoder& enc,
                   const std::string& oldname, const std::string& newname);
void decode_rename_result(XdrDecoder& dec);

}  // namespace nfs4
