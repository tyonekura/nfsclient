#pragma once

#include "nfs4/nfs4_types.hpp"
#include "nfs4/nfs4_error.hpp"
#include "nfs4/nfs4_attr.hpp"
#include "nfs4/readdir.hpp"
#include "rpc/rpc_client.hpp"
#include "rpc/rpc_types.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// High-level NFSv4.0 client.
//
// On construction, resolves the NFS port via portmap, establishes a persistent
// TCP connection, and performs the SETCLIENTID / SETCLIENTID_CONFIRM handshake
// to obtain a clientid4 from the server.
//
// All data operations (read, write) require an Nfs4File obtained via open_read()
// or open_write() and must be paired with close().
class Nfs4Client {
public:
    // Connect to `host`, resolve its NFSv4 port, and register this client.
    // export_path is used only to obtain the root file handle via PUTROOTFH.
    explicit Nfs4Client(const std::string& host);

    // Switch to AUTH_SYS credentials for all subsequent calls.
    void set_auth_sys(const AuthSys& auth);
    void clear_auth();

    // ── File handle operations ────────────────────────────────────────────────

    // Returns the root file handle (established in constructor via PUTROOTFH+GETFH).
    Nfs4Fh root_fh() const { return root_fh_; }

    // Resolve a name inside a directory (COMPOUND: PUTFH + LOOKUP + GETFH).
    Nfs4Fh lookup(const Nfs4Fh& dir, const std::string& name);

    // Get file attributes (COMPOUND: PUTFH + GETATTR).
    Fattr4 getattr(const Nfs4Fh& fh);

    // Check access permissions (COMPOUND: PUTFH + ACCESS).
    uint32_t access(const Nfs4Fh& fh, uint32_t mask);

    // ── Open / close ─────────────────────────────────────────────────────────

    // Open an existing file for reading (COMPOUND: PUTFH + OPEN(NOCREATE) + GETFH).
    Nfs4File open_read(const Nfs4Fh& dir, const std::string& name);

    // Open (or create) a file for writing (COMPOUND: PUTFH + OPEN(CREATE,UNCHECKED) + GETFH).
    Nfs4File open_write(const Nfs4Fh& dir, const std::string& name, bool create = true);

    // Close an open file (COMPOUND: PUTFH + CLOSE).
    void close(const Nfs4File& f);

    // ── Data operations ───────────────────────────────────────────────────────

    // Read up to `count` bytes from `f` at `offset`.
    std::vector<uint8_t> read(const Nfs4File& f, uint64_t offset, uint32_t count);

    // Write `len` bytes to `f` at `offset`. Returns number of bytes written.
    uint32_t write(const Nfs4File& f, uint64_t offset, Stable4 stable,
                   const uint8_t* data, uint32_t len);

    // Flush unstable writes to stable storage (COMPOUND: PUTFH + COMMIT).
    std::array<uint8_t, 8> commit(const Nfs4File& f,
                                   uint64_t offset = 0, uint32_t count = 0);

    // ── Namespace operations ──────────────────────────────────────────────────

    // Create a directory (COMPOUND: PUTFH + CREATE(NF4DIR) + GETFH).
    Nfs4Fh mkdir(const Nfs4Fh& dir, const std::string& name,
                 const nfs4::Sattr4& attrs = {});

    // Delete a file or empty directory (COMPOUND: PUTFH + REMOVE).
    void remove(const Nfs4Fh& dir, const std::string& name);

    // Rename / move (COMPOUND: PUTFH(src) + SAVEFH + PUTFH(dst) + RENAME).
    void rename(const Nfs4Fh& src_dir, const std::string& src_name,
                const Nfs4Fh& dst_dir, const std::string& dst_name);

    // Create a symbolic link (COMPOUND: PUTFH + CREATE(NF4LNK) + GETFH).
    Nfs4Fh symlink(const Nfs4Fh& dir, const std::string& name,
                   const std::string& target, const nfs4::Sattr4& attrs = {});

    // Read a symbolic link target (COMPOUND: PUTFH + READLINK).
    std::string readlink(const Nfs4Fh& fh);

    // Set file attributes (COMPOUND: PUTFH + SETATTR).
    void setattr(const Nfs4Fh& fh, const nfs4::Sattr4& attrs);

    // ── Directory listing ─────────────────────────────────────────────────────

    // List all entries in `dir` (auto-paginated READDIR).
    std::vector<Nfs4DirEntry> readdir(const Nfs4Fh& dir);

    // ── Lease renewal ─────────────────────────────────────────────────────────

    void renew();

private:
    // Perform OPEN and optional OPEN_CONFIRM; return the opened Nfs4File.
    Nfs4File do_open(const Nfs4Fh& dir, const std::string& name,
                     uint32_t share_access, bool create);

    std::string                    host_;
    std::unique_ptr<TcpRpcClient>  rpc_;
    Nfs4Fh                         root_fh_;
    uint64_t                       clientid_{};
    uint32_t                       open_seqid_{0};
};
