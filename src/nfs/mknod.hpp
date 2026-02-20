#pragma once

#include "nfs3_types.hpp"
#include "../rpc/rpc_client.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace nfs3 {

// Device major/minor numbers for NF3CHR and NF3BLK special files.
struct DeviceSpec3 {
    uint32_t major_num = 0;
    uint32_t minor_num = 0;
};

// Encode/decode helpers (pure, no network)

// NF3FIFO or NF3SOCK: encode takes just the sattr3.
std::vector<uint8_t> encode_mknod_args(const Fh3& dir, const std::string& name,
                                        Ftype3 type, const Sattr3& attrs = {});

// NF3CHR or NF3BLK: encode takes sattr3 + device specdata.
std::vector<uint8_t> encode_mknod_device_args(const Fh3& dir, const std::string& name,
                                               Ftype3 type, const Sattr3& attrs,
                                               const DeviceSpec3& spec);

Fh3 decode_mknod_reply(const std::vector<uint8_t>& data);

// NFSPROC3_MKNOD (proc 11) — create a special file.

// Create a named pipe (NF3FIFO) or Unix socket (NF3SOCK).
Fh3 mknod_fifo(TcpRpcClient& client, const Fh3& dir, const std::string& name,
                const Sattr3& attrs = {});
Fh3 mknod_socket(TcpRpcClient& client, const Fh3& dir, const std::string& name,
                  const Sattr3& attrs = {});

// Create a character (NF3CHR) or block (NF3BLK) device file.
Fh3 mknod_chr(TcpRpcClient& client, const Fh3& dir, const std::string& name,
               const Sattr3& attrs, const DeviceSpec3& spec);
Fh3 mknod_blk(TcpRpcClient& client, const Fh3& dir, const std::string& name,
               const Sattr3& attrs, const DeviceSpec3& spec);

}  // namespace nfs3
