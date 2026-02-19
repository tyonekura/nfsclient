#pragma once

#include "nfs3_types.hpp"
#include "../rpc/rpc_client.hpp"

#include <vector>

namespace nfs3 {

// Encode / decode helpers (used directly by unit tests).
std::vector<uint8_t> encode_write_args(const Fh3& fh, uint64_t offset,
                                        Stable3 stable,
                                        const uint8_t* data, size_t data_size);
WriteResult decode_write_reply(const std::vector<uint8_t>& data);

// Send NFSPROC3_WRITE and return the result.
WriteResult write(TcpRpcClient& client, const Fh3& fh, uint64_t offset,
                  Stable3 stable, const uint8_t* data, size_t data_size);

}  // namespace nfs3
