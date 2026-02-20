#pragma once

#include "nfs/nfs3_types.hpp"
#include "nfs/nfs_error.hpp"
#include "nfs/access.hpp"
#include "nfs/commit.hpp"
#include "nfs/create.hpp"
#include "nfs/fsinfo.hpp"
#include "nfs/readdir.hpp"
#include "nfs/rename.hpp"
#include "nfs/setattr.hpp"
#include "rpc/rpc_client.hpp"
#include "rpc/rpc_types.hpp"

#include <array>
#include <cstdint>
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

    // Switch to AUTH_SYS credentials for all subsequent NFS calls.
    void set_auth_sys(const AuthSys& auth);

    // Revert to AUTH_NONE (the default).
    void clear_auth();

    // ── MOUNT protocol ───────────────────────────────────────────────────────

    // Obtain the root file handle for an NFS export via the MOUNT protocol.
    Fh3 mount(const std::string& export_path);

    // ── File operations ──────────────────────────────────────────────────────

    // NFSPROC3_GETATTR (proc 1): return file attributes.
    Fattr3 getattr(const Fh3& fh);

    // NFSPROC3_LOOKUP (proc 3): resolve a name inside a directory.
    Fh3 lookup(const Fh3& dir, const std::string& name);

    // NFSPROC3_READ (proc 6): read up to `count` bytes from `fh` at `offset`.
    std::vector<uint8_t> read(const Fh3& fh, uint64_t offset, uint32_t count);

    // NFSPROC3_WRITE (proc 7): write `data_size` bytes to `fh` at `offset`.
    WriteResult write(const Fh3& fh, uint64_t offset, Stable3 stable,
                      const uint8_t* data, size_t data_size);

    WriteResult write(const Fh3& fh, uint64_t offset, Stable3 stable,
                      const std::vector<uint8_t>& data) {
        return write(fh, offset, stable, data.data(), data.size());
    }

    // NFSPROC3_CREATE (proc 8): create a file. Returns the new file's handle.
    Fh3 create(const Fh3& dir, const std::string& name,
                nfs3::CreateMode3 mode = nfs3::CreateMode3::UNCHECKED,
                const Sattr3& attrs = {});

    Fh3 create_exclusive(const Fh3& dir, const std::string& name,
                         const nfs3::CreateVerf3& verf);

    // ── Directory operations ─────────────────────────────────────────────────

    // NFSPROC3_MKDIR (proc 9): create a directory.
    Fh3 mkdir(const Fh3& dir, const std::string& name, const Sattr3& attrs = {});

    // NFSPROC3_REMOVE (proc 12): delete a file.
    void remove(const Fh3& dir, const std::string& name);

    // NFSPROC3_RMDIR (proc 13): remove an empty directory.
    void rmdir(const Fh3& dir, const std::string& name);

    // NFSPROC3_SETATTR (proc 2): set attributes on fh.
    void setattr(const Fh3& fh, const Sattr3& attrs,
                 const nfs3::SattrGuard3& guard = {});

    // NFSPROC3_READDIR (proc 16) — single page.
    nfs3::ReaddirPage readdir_page(const Fh3& dir,
                                   uint64_t cookie = 0,
                                   const std::array<uint8_t, 8>& cookieverf = {},
                                   uint32_t count = 4096);

    // NFSPROC3_READDIR — all entries (auto-paginated).
    std::vector<nfs3::DirEntry3> readdir(const Fh3& dir, uint32_t count = 4096);

    // NFSPROC3_RENAME (proc 14): rename from_dir/from_name to to_dir/to_name.
    void rename(const Fh3& from_dir, const std::string& from_name,
                const Fh3& to_dir,   const std::string& to_name);

    // NFSPROC3_COMMIT (proc 21): flush unstable writes to stable storage.
    // offset=0, count=0 means "commit everything". Returns the write verifier.
    nfs3::CommitVerf3 commit(const Fh3& fh, uint64_t offset = 0, uint32_t count = 0);

    // NFSPROC3_ACCESS (proc 4): check access permissions.
    // Returns the granted access bitmask (ACCESS3_READ, ACCESS3_MODIFY, etc.).
    uint32_t access(const Fh3& fh, uint32_t access_mask);

    // NFSPROC3_FSSTAT (proc 18): filesystem capacity and usage.
    nfs3::FsstatResult fsstat(const Fh3& root);

    // NFSPROC3_FSINFO (proc 19): server capabilities and preferred I/O sizes.
    nfs3::FsinfoResult fsinfo(const Fh3& root);

    // NFSPROC3_PATHCONF (proc 20): POSIX pathconf values.
    nfs3::PathconfResult pathconf(const Fh3& fh);

private:
    std::string                  host_;
    std::unique_ptr<TcpRpcClient> nfs_conn_;
};
