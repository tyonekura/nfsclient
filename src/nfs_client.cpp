#include "nfs_client.hpp"
#include "nfs/portmap.hpp"
#include "nfs/mount.hpp"
#include "nfs/lookup.hpp"
#include "nfs/read.hpp"
#include "nfs/write.hpp"

static constexpr uint32_t NFS_PROG = 100003;
static constexpr uint32_t NFS_VERS = 3;

NFSClient::NFSClient(const std::string& host) : host_(host) {
    const uint16_t port = nfs3::getport(host_, NFS_PROG, NFS_VERS);
    nfs_conn_ = std::make_unique<TcpRpcClient>(host_, port);
}

Fh3 NFSClient::mount(const std::string& export_path) {
    return nfs3::mnt(host_, export_path);
}

Fh3 NFSClient::lookup(const Fh3& dir, const std::string& name) {
    return nfs3::lookup(*nfs_conn_, dir, name);
}

std::vector<uint8_t> NFSClient::read(const Fh3& fh, uint64_t offset, uint32_t count) {
    return nfs3::read(*nfs_conn_, fh, offset, count);
}

WriteResult NFSClient::write(const Fh3& fh, uint64_t offset, Stable3 stable,
                              const uint8_t* data, size_t data_size) {
    return nfs3::write(*nfs_conn_, fh, offset, stable, data, data_size);
}
