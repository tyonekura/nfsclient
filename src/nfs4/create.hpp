#pragma once

#include "nfs4_types.hpp"
#include "nfs4_attr.hpp"
#include "nfs4_error.hpp"
#include "../xdr/xdr.hpp"



#include <string>

namespace nfs4 {

// Encode CREATE op for a directory (NF4DIR).
void encode_create_dir(XdrEncoder& enc, const std::string& name, const Sattr4& attrs = {});

// Encode CREATE op for a symlink (NF4LNK).
void encode_create_symlink(XdrEncoder& enc,
                            const std::string& name,
                            const std::string& target,
                            const Sattr4& attrs = {});

// Decode CREATE per-op result.  Returns nothing (FH obtained via GETFH after).
void decode_create_result(XdrDecoder& dec);

}  // namespace nfs4
