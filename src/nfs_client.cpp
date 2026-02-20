#include "nfs_client.hpp"
#include "nfs/portmap.hpp"
#include "nfs/mount.hpp"
#include "nfs/getattr.hpp"
#include "nfs/lookup.hpp"
#include "nfs/read.hpp"
#include "nfs/write.hpp"
#include "nfs/create.hpp"
#include "nfs/dirop.hpp"
#include "nfs/setattr.hpp"
#include "nfs/readdir.hpp"
#include "nfs/commit.hpp"
#include "nfs/rename.hpp"
#include "nfs/access.hpp"
#include "nfs/fsinfo.hpp"
#include "nfs/readdirplus.hpp"
#include "nfs/symlink.hpp"
#include "nfs/mknod.hpp"

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

void NFSClient::setattr(const Fh3& fh, const Sattr3& attrs,
                         const nfs3::SattrGuard3& guard) {
    nfs3::setattr(*nfs_conn_, fh, attrs, guard);
}

nfs3::ReaddirPage NFSClient::readdir_page(const Fh3& dir,
                                            uint64_t cookie,
                                            const std::array<uint8_t, 8>& cookieverf,
                                            uint32_t count) {
    return nfs3::readdir_page(*nfs_conn_, dir, cookie, cookieverf, count);
}

std::vector<nfs3::DirEntry3> NFSClient::readdir(const Fh3& dir, uint32_t count) {
    return nfs3::readdir(*nfs_conn_, dir, count);
}

void NFSClient::rename(const Fh3& from_dir, const std::string& from_name,
                        const Fh3& to_dir,   const std::string& to_name) {
    nfs3::rename(*nfs_conn_, from_dir, from_name, to_dir, to_name);
}

nfs3::CommitVerf3 NFSClient::commit(const Fh3& fh, uint64_t offset, uint32_t count) {
    return nfs3::commit(*nfs_conn_, fh, offset, count);
}

uint32_t NFSClient::access(const Fh3& fh, uint32_t access_mask) {
    return nfs3::access(*nfs_conn_, fh, access_mask);
}

nfs3::FsstatResult NFSClient::fsstat(const Fh3& root) {
    return nfs3::fsstat(*nfs_conn_, root);
}

nfs3::FsinfoResult NFSClient::fsinfo(const Fh3& root) {
    return nfs3::fsinfo(*nfs_conn_, root);
}

nfs3::PathconfResult NFSClient::pathconf(const Fh3& fh) {
    return nfs3::pathconf(*nfs_conn_, fh);
}

std::string NFSClient::readlink(const Fh3& symlink_fh) {
    return nfs3::readlink(*nfs_conn_, symlink_fh);
}

Fh3 NFSClient::symlink(const Fh3& dir, const std::string& name,
                        const std::string& target, const Sattr3& attrs) {
    return nfs3::symlink(*nfs_conn_, dir, name, target, attrs);
}

void NFSClient::link(const Fh3& file, const Fh3& link_dir,
                     const std::string& link_name) {
    nfs3::link(*nfs_conn_, file, link_dir, link_name);
}

Fh3 NFSClient::mknod_fifo(const Fh3& dir, const std::string& name,
                            const Sattr3& attrs) {
    return nfs3::mknod_fifo(*nfs_conn_, dir, name, attrs);
}

Fh3 NFSClient::mknod_socket(const Fh3& dir, const std::string& name,
                              const Sattr3& attrs) {
    return nfs3::mknod_socket(*nfs_conn_, dir, name, attrs);
}

Fh3 NFSClient::mknod_chr(const Fh3& dir, const std::string& name,
                           const Sattr3& attrs, const nfs3::DeviceSpec3& spec) {
    return nfs3::mknod_chr(*nfs_conn_, dir, name, attrs, spec);
}

Fh3 NFSClient::mknod_blk(const Fh3& dir, const std::string& name,
                           const Sattr3& attrs, const nfs3::DeviceSpec3& spec) {
    return nfs3::mknod_blk(*nfs_conn_, dir, name, attrs, spec);
}

nfs3::ReaddirplusPage NFSClient::readdirplus_page(
        const Fh3& dir, uint64_t cookie,
        const std::array<uint8_t, 8>& cookieverf,
        uint32_t dircount, uint32_t maxcount) {
    return nfs3::readdirplus_page(*nfs_conn_, dir, cookie, cookieverf,
                                   dircount, maxcount);
}

std::vector<nfs3::DirEntryPlus3> NFSClient::readdirplus(
        const Fh3& dir, uint32_t dircount, uint32_t maxcount) {
    return nfs3::readdirplus(*nfs_conn_, dir, dircount, maxcount);
}

void NFSClient::umnt(const std::string& export_path) {
    nfs3::umnt(host_, export_path);
}

std::vector<nfs3::ExportEntry> NFSClient::export_list() {
    return nfs3::export_list(host_);
}
