#pragma once

#include "nfs/nfs3_types.hpp"
#include "rpc/rpc_client.hpp"

#include <memory>
#include <string>
#include <vector>

// High-level NFSv3 client.
//
// On construction, resolves the NFS port via portmap and establishes a
// persistent TCP connection to the NFS daemon.
//
// mount() opens a separate short-lived connection to mountd each call.
class NFSClient {
public:
    explicit NFSClient(const std::string& host);

    // Obtain the root file handle for an NFS export via the MOUNT protocol.
    Fh3 mount(const std::string& export_path);

    // NFSPROC3_LOOKUP: resolve a name inside a directory.
    Fh3 lookup(const Fh3& dir, const std::string& name);

    // NFSPROC3_READ: read up to `count` bytes from `fh` at `offset`.
    std::vector<uint8_t> read(const Fh3& fh, uint64_t offset, uint32_t count);

    // NFSPROC3_WRITE: write `data_size` bytes to `fh` at `offset`.
    WriteResult write(const Fh3& fh, uint64_t offset, Stable3 stable,
                      const uint8_t* data, size_t data_size);

    WriteResult write(const Fh3& fh, uint64_t offset, Stable3 stable,
                      const std::vector<uint8_t>& data) {
        return write(fh, offset, stable, data.data(), data.size());
    }

private:
    std::string                  host_;
    std::unique_ptr<TcpRpcClient> nfs_conn_;
};
