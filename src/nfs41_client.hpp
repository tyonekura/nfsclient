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

// High-level NFSv4.1 client.
//
// On construction, resolves the NFS port via portmap, establishes a persistent
// TCP connection, and performs the EXCHANGE_ID / CREATE_SESSION / RECLAIM_COMPLETE
// handshake to set up an NFSv4.1 session.
//
// Public API is identical to Nfs4Client.  All COMPOUNDs after session setup
// automatically prepend a SEQUENCE op (single-slot, slotid=0).
//
// In NFSv4.1 there is no OPEN_CONFIRM; RENEW is replaced by implicit lease
// renewal via SEQUENCE on any COMPOUND.
class Nfs41Client {
public:
    // Connect to `host` with AUTH_NONE and establish an NFSv4.1 session.
    explicit Nfs41Client(const std::string& host);

    // Same as above but switches to AUTH_SYS before session setup.
    Nfs41Client(const std::string& host, const AuthSys& auth);

    ~Nfs41Client();

    void set_auth_sys(const AuthSys& auth);
    void clear_auth();

    // ── File handle operations ────────────────────────────────────────────────

    Nfs4Fh root_fh() const { return root_fh_; }

    Nfs4Fh lookup(const Nfs4Fh& dir, const std::string& name);
    Fattr4 getattr(const Nfs4Fh& fh);
    uint32_t access(const Nfs4Fh& fh, uint32_t mask);

    // ── Open / close ─────────────────────────────────────────────────────────

    Nfs4File open_read(const Nfs4Fh& dir, const std::string& name);
    Nfs4File open_write(const Nfs4Fh& dir, const std::string& name, bool create = true);
    void close(const Nfs4File& f);

    // ── Data operations ───────────────────────────────────────────────────────

    std::vector<uint8_t> read(const Nfs4File& f, uint64_t offset, uint32_t count);
    uint32_t write(const Nfs4File& f, uint64_t offset, Stable4 stable,
                   const uint8_t* data, uint32_t len);
    std::array<uint8_t, 8> commit(const Nfs4File& f,
                                   uint64_t offset = 0, uint32_t count = 0);

    // ── Namespace operations ──────────────────────────────────────────────────

    Nfs4Fh mkdir(const Nfs4Fh& dir, const std::string& name,
                 const nfs4::Sattr4& attrs = {});
    void remove(const Nfs4Fh& dir, const std::string& name);
    void rename(const Nfs4Fh& src_dir, const std::string& src_name,
                const Nfs4Fh& dst_dir, const std::string& dst_name);
    Nfs4Fh symlink(const Nfs4Fh& dir, const std::string& name,
                   const std::string& target, const nfs4::Sattr4& attrs = {});
    std::string readlink(const Nfs4Fh& fh);
    void setattr(const Nfs4Fh& fh, const nfs4::Sattr4& attrs);

    // ── Directory listing ─────────────────────────────────────────────────────

    std::vector<Nfs4DirEntry> readdir(const Nfs4Fh& dir);

    // ── Session ID (for test introspection) ───────────────────────────────────

    const SessionId41& session_id() const { return sessionid_; }
    uint64_t client_id() const { return clientid_; }

private:
    // Send a COMPOUND with SEQUENCE prepended (minorversion=1).
    std::vector<uint8_t> compound41(const std::string& tag,
                                     const std::vector<uint8_t>& ops_bytes,
                                     uint32_t num_ops);

    // Perform OPEN (with NFS4ERR_GRACE retry loop); no OPEN_CONFIRM in v4.1.
    Nfs4File do_open(const Nfs4Fh& dir, const std::string& name,
                     uint32_t share_access, bool create);

    std::string                   host_;
    std::unique_ptr<TcpRpcClient> rpc_;
    Nfs4Fh                        root_fh_;
    uint64_t                      clientid_{};
    SessionId41                   sessionid_{};
    uint32_t                      slot_seqid_{1};  // increments each COMPOUND
    uint32_t                      open_seqid_{0};  // OPEN seqid (ignored by server in v4.1)
};
