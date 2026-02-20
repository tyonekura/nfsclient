#include "nfs_client.hpp"
#include "nfs/portmap.hpp"
#include "nfs/mount.hpp"
#include "nfs/getattr.hpp"
#include "nfs/lookup.hpp"
#include "nfs/read.hpp"
#include "nfs/write.hpp"
#include "nfs/create.hpp"
#include "nfs/dirop.hpp"

static constexpr uint32_t NFS_PROG = 100003;
static constexpr uint32_t NFS_VERS = 3;

NFSClient::NFSClient(const std::string& host) : host_(host) {
    const uint16_t port = nfs3::getport(host_, NFS_PROG, NFS_VERS);
    nfs_conn_ = std::make_unique<TcpRpcClient>(host_, port);
}

void NFSClient::set_auth_sys(const AuthSys& auth) {
    nfs_conn_->set_auth_sys(auth);
}

void NFSClient::clear_auth() {
    nfs_conn_->clear_auth();
}

Fh3 NFSClient::mount(const std::string& export_path) {
    return nfs3::mnt(host_, export_path);
}

Fattr3 NFSClient::getattr(const Fh3& fh) {
    return nfs3::getattr(*nfs_conn_, fh);
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

Fh3 NFSClient::create(const Fh3& dir, const std::string& name,
                       nfs3::CreateMode3 mode, const Sattr3& attrs) {
    return nfs3::create(*nfs_conn_, dir, name, mode, attrs);
}

Fh3 NFSClient::create_exclusive(const Fh3& dir, const std::string& name,
                                 const nfs3::CreateVerf3& verf) {
    return nfs3::create_exclusive(*nfs_conn_, dir, name, verf);
}

Fh3 NFSClient::mkdir(const Fh3& dir, const std::string& name, const Sattr3& attrs) {
    return nfs3::mkdir(*nfs_conn_, dir, name, attrs);
}

void NFSClient::remove(const Fh3& dir, const std::string& name) {
    return nfs3::remove(*nfs_conn_, dir, name);
}

void NFSClient::rmdir(const Fh3& dir, const std::string& name) {
    return nfs3::rmdir(*nfs_conn_, dir, name);
}
