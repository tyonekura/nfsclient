#pragma once

#include "nfs4_types.hpp"
#include "nfs4_attr.hpp"
#include "nfs4_error.hpp"
#include "../xdr/xdr.hpp"

#include <array>
#include <cstdint>
#include <initializer_list>
#include <vector>

namespace nfs4 {

// Result of a single READDIR page.
struct ReaddirPage4 {
    std::array<uint8_t, 8> cookieverf{};
    std::vector<Nfs4DirEntry> entries;
    bool eof{};
};

void encode_readdir(XdrEncoder& enc,
                    uint64_t cookie,
                    const std::array<uint8_t, 8>& cookieverf,
                    uint32_t dircount,
                    uint32_t maxcount,
                    std::initializer_list<uint32_t> attr_ids);

ReaddirPage4 decode_readdir_result(XdrDecoder& dec);

}  // namespace nfs4
