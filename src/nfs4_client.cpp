#include "nfs4_client.hpp"
#include "nfs4/compound.hpp"
#include "nfs4/fh_ops.hpp"
#include "nfs4/setclientid.hpp"
#include "nfs4/lookup.hpp"
#include "nfs4/getattr.hpp"
#include "nfs4/access.hpp"
#include "nfs4/open.hpp"
#include "nfs4/read.hpp"
#include "nfs4/write.hpp"
#include "nfs4/commit.hpp"
#include "nfs4/dirop.hpp"
#include "nfs4/setattr.hpp"
#include "nfs4/create.hpp"
#include "nfs4/readdir.hpp"
#include "nfs4/readlink.hpp"
#include "nfs/portmap.hpp"

#include <chrono>
#include <cstdint>
#include <unistd.h>

static constexpr uint32_t NFS4_PROG = 100003;
static constexpr uint32_t NFS4_VERS = 4;

// ── Constructor helpers (file-local) ──────────────────────────────────────────

static uint64_t do_setclientid_confirm(TcpRpcClient& rpc) {
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    std::array<uint8_t, 8> verifier{};
    for (int i = 7; i >= 0; --i) {
        verifier[static_cast<size_t>(i)] = static_cast<uint8_t>(now & 0xFF);
        now >>= 8;
    }

    XdrEncoder ops;
    nfs4::encode_setclientid(ops, verifier, "nfsclient-v4");
    auto reply = nfs4::call_compound(rpc, "init", ops.release(), 1);
    XdrDecoder dec(reply);
    nfs4::check_compound_status(dec);
    auto r = nfs4::decode_setclientid_result(dec);

    XdrEncoder ops2;
    nfs4::encode_setclientid_confirm(ops2, r.clientid, r.confirm_verifier);
    auto reply2 = nfs4::call_compound(rpc, "init", ops2.release(), 1);
    XdrDecoder dec2(reply2);
    nfs4::check_compound_status(dec2);
    nfs4::decode_setclientid_confirm_result(dec2);

    return r.clientid;
}

// Verify root is reachable and return the root sentinel (empty FH).
// All Nfs4Client methods treat an empty Nfs4Fh as "use PUTROOTFH" to
// avoid PUTFH on the root FH, which Linux nfsd rejects with NFS4ERR_PERM
// via fh_verify() while PUTROOTFH (exp_pseudoroot) bypasses that check.
static Nfs4Fh do_get_root_fh(TcpRpcClient& rpc) {
    XdrEncoder ops;
    nfs4::encode_putrootfh(ops);
    nfs4::encode_getfh(ops);
    auto reply = nfs4::call_compound(rpc, "", ops.release(), 2);
    XdrDecoder dec(reply);
    nfs4::check_compound_status(dec);
    nfs4::decode_putrootfh_result(dec);
    nfs4::decode_getfh_result(dec);   // discard FH; we use PUTROOTFH for root ops
    return Nfs4Fh{};                  // empty = root sentinel
}

// Encode PUTROOTFH for the root sentinel or PUTFH(fh) for any other FH.
// decode_putfh_result() works for both because it ignores the resop value.
static void encode_fh(XdrEncoder& ops, const Nfs4Fh& fh) {
    if (fh.data.empty())
        nfs4::encode_putrootfh(ops);
    else
        nfs4::encode_putfh(ops, fh);
}

// ── Constructors ──────────────────────────────────────────────────────────────

Nfs4Client::Nfs4Client(const std::string& host) : host_(host) {
    const uint16_t port = nfs3::getport(host_, NFS4_PROG, NFS4_VERS);
    rpc_      = std::make_unique<TcpRpcClient>(host_, port);
    clientid_ = do_setclientid_confirm(*rpc_);
    root_fh_  = do_get_root_fh(*rpc_);
}

Nfs4Client::Nfs4Client(const std::string& host, const AuthSys& auth) : host_(host) {
    const uint16_t port = nfs3::getport(host_, NFS4_PROG, NFS4_VERS);
    rpc_      = std::make_unique<TcpRpcClient>(host_, port);
    rpc_->set_auth_sys(auth);    // switch to AUTH_SYS before SETCLIENTID and PUTROOTFH
    clientid_ = do_setclientid_confirm(*rpc_);
    root_fh_  = do_get_root_fh(*rpc_);
}

void Nfs4Client::set_auth_sys(const AuthSys& auth) { rpc_->set_auth_sys(auth); }
void Nfs4Client::clear_auth()                       { rpc_->clear_auth(); }

// ── File handle operations ────────────────────────────────────────────────────

Nfs4Fh Nfs4Client::lookup(const Nfs4Fh& dir, const std::string& name) {
    XdrEncoder ops;
    encode_fh(ops, dir);
    nfs4::encode_lookup(ops, name);
    nfs4::encode_getfh(ops);
    auto reply = nfs4::call_compound(*rpc_, "", ops.release(), 3);
    XdrDecoder dec(reply);
    nfs4::check_compound_status(dec);
    nfs4::decode_putfh_result(dec);
    nfs4::decode_lookup_result(dec);
    return nfs4::decode_getfh_result(dec);
}

Fattr4 Nfs4Client::getattr(const Nfs4Fh& fh) {
    XdrEncoder ops;
    encode_fh(ops, fh);
    nfs4::encode_getattr(ops, {
        nfs4::attr::TYPE, nfs4::attr::CHANGE, nfs4::attr::SIZE,
        nfs4::attr::FILEID, nfs4::attr::MODE, nfs4::attr::NUMLINKS,
        nfs4::attr::OWNER, nfs4::attr::OWNER_GROUP,
        nfs4::attr::TIME_ACCESS, nfs4::attr::TIME_METADATA, nfs4::attr::TIME_MODIFY
    });
    auto reply = nfs4::call_compound(*rpc_, "", ops.release(), 2);
    XdrDecoder dec(reply);
    nfs4::check_compound_status(dec);
    nfs4::decode_putfh_result(dec);
    return nfs4::decode_getattr_result(dec);
}

uint32_t Nfs4Client::access(const Nfs4Fh& fh, uint32_t mask) {
    XdrEncoder ops;
    encode_fh(ops, fh);
    nfs4::encode_access(ops, mask);
    auto reply = nfs4::call_compound(*rpc_, "", ops.release(), 2);
    XdrDecoder dec(reply);
    nfs4::check_compound_status(dec);
    nfs4::decode_putfh_result(dec);
    return nfs4::decode_access_result(dec).access;
}

// ── Open / close ─────────────────────────────────────────────────────────────

Nfs4File Nfs4Client::do_open(const Nfs4Fh& dir, const std::string& name,
                              uint32_t share_access, bool create) {
    static constexpr uint32_t NFS4ERR_GRACE = 10013;

    uint32_t seqid = ++open_seqid_;

    // Retry loop: RFC 7530 §8.6 requires clients to retry on NFS4ERR_GRACE
    // (server in grace period after restart) using the same seqid.
    std::vector<uint8_t> reply;
    while (true) {
        XdrEncoder ops;
        encode_fh(ops, dir);
        if (create) {
            nfs4::encode_open_create(ops, seqid, share_access,
                                     clientid_, "nfsclient-v4", name);
        } else {
            nfs4::encode_open_nocreate(ops, seqid, share_access,
                                       clientid_, "nfsclient-v4", name);
        }
        nfs4::encode_getfh(ops);
        reply = nfs4::call_compound(*rpc_, "", ops.release(), 3);
        XdrDecoder dec(reply);
        try {
            nfs4::check_compound_status(dec);
        } catch (const Nfs4Error& e) {
            if (e.status == NFS4ERR_GRACE) {
                ::sleep(5);
                continue;  // retry with same seqid
            }
            throw;
        }
        // Success — re-create decoder at the start of the reply for decoding below
        break;
    }
    XdrDecoder dec(reply);
    nfs4::check_compound_status(dec);
    nfs4::decode_putfh_result(dec);
    auto open_res = nfs4::decode_open_result(dec);
    Nfs4Fh fh = nfs4::decode_getfh_result(dec);

    Nfs4File f;
    f.fh      = fh;
    f.stateid = open_res.stateid;
    f.seqid   = seqid;

    // OPEN_CONFIRM required when rflags & OPEN4_RESULT_CONFIRM
    if (open_res.rflags & nfs4::OPEN4_RESULT_CONFIRM) {
        uint32_t confirm_seqid = ++open_seqid_;
        XdrEncoder ops2;
        encode_fh(ops2, fh);
        nfs4::encode_open_confirm(ops2, f.stateid, confirm_seqid);
        auto reply2 = nfs4::call_compound(*rpc_, "", ops2.release(), 2);
        XdrDecoder dec2(reply2);
        nfs4::check_compound_status(dec2);
        nfs4::decode_putfh_result(dec2);
        f.stateid = nfs4::decode_open_confirm_result(dec2);
        f.seqid   = confirm_seqid;
    }

    return f;
}

Nfs4File Nfs4Client::open_read(const Nfs4Fh& dir, const std::string& name) {
    return do_open(dir, name, nfs4::OPEN4_SHARE_ACCESS_READ, false);
}

Nfs4File Nfs4Client::open_write(const Nfs4Fh& dir, const std::string& name, bool create) {
    return do_open(dir, name, nfs4::OPEN4_SHARE_ACCESS_WRITE, create);
}

void Nfs4Client::close(const Nfs4File& f) {
    XdrEncoder ops;
    encode_fh(ops, f.fh);
    nfs4::encode_close(ops, f.seqid, f.stateid);
    auto reply = nfs4::call_compound(*rpc_, "", ops.release(), 2);
    XdrDecoder dec(reply);
    nfs4::check_compound_status(dec);
    nfs4::decode_putfh_result(dec);
    nfs4::decode_close_result(dec);
}

// ── Data operations ───────────────────────────────────────────────────────────

std::vector<uint8_t> Nfs4Client::read(const Nfs4File& f,
                                       uint64_t offset, uint32_t count) {
    XdrEncoder ops;
    encode_fh(ops, f.fh);
    nfs4::encode_read(ops, f.stateid, offset, count);
    auto reply = nfs4::call_compound(*rpc_, "", ops.release(), 2);
    XdrDecoder dec(reply);
    nfs4::check_compound_status(dec);
    nfs4::decode_putfh_result(dec);
    return nfs4::decode_read_result(dec);
}

uint32_t Nfs4Client::write(const Nfs4File& f, uint64_t offset, Stable4 stable,
                            const uint8_t* data, uint32_t len) {
    XdrEncoder ops;
    encode_fh(ops, f.fh);
    nfs4::encode_write(ops, f.stateid, offset, stable, data, len);
    auto reply = nfs4::call_compound(*rpc_, "", ops.release(), 2);
    XdrDecoder dec(reply);
    nfs4::check_compound_status(dec);
    nfs4::decode_putfh_result(dec);
    return nfs4::decode_write_result(dec).count;
}

std::array<uint8_t, 8> Nfs4Client::commit(const Nfs4File& f,
                                            uint64_t offset, uint32_t count) {
    XdrEncoder ops;
    encode_fh(ops, f.fh);
    nfs4::encode_commit(ops, offset, count);
    auto reply = nfs4::call_compound(*rpc_, "", ops.release(), 2);
    XdrDecoder dec(reply);
    nfs4::check_compound_status(dec);
    nfs4::decode_putfh_result(dec);
    return nfs4::decode_commit_result(dec);
}

// ── Namespace operations ──────────────────────────────────────────────────────

Nfs4Fh Nfs4Client::mkdir(const Nfs4Fh& dir, const std::string& name,
                          const nfs4::Sattr4& attrs) {
    XdrEncoder ops;
    encode_fh(ops, dir);
    nfs4::encode_create_dir(ops, name, attrs);
    nfs4::encode_getfh(ops);
    auto reply = nfs4::call_compound(*rpc_, "", ops.release(), 3);
    XdrDecoder dec(reply);
    nfs4::check_compound_status(dec);
    nfs4::decode_putfh_result(dec);
    nfs4::decode_create_result(dec);
    return nfs4::decode_getfh_result(dec);
}

void Nfs4Client::remove(const Nfs4Fh& dir, const std::string& name) {
    XdrEncoder ops;
    encode_fh(ops, dir);
    nfs4::encode_remove(ops, name);
    auto reply = nfs4::call_compound(*rpc_, "", ops.release(), 2);
    XdrDecoder dec(reply);
    nfs4::check_compound_status(dec);
    nfs4::decode_putfh_result(dec);
    nfs4::decode_remove_result(dec);
}

void Nfs4Client::rename(const Nfs4Fh& src_dir, const std::string& src_name,
                        const Nfs4Fh& dst_dir, const std::string& dst_name) {
    // COMPOUND: PUTFH/PUTROOTFH(src_dir), SAVEFH, PUTFH/PUTROOTFH(dst_dir), RENAME
    XdrEncoder ops;
    encode_fh(ops, src_dir);
    nfs4::encode_savefh(ops);
    encode_fh(ops, dst_dir);
    nfs4::encode_rename(ops, src_name, dst_name);
    auto reply = nfs4::call_compound(*rpc_, "", ops.release(), 4);
    XdrDecoder dec(reply);
    nfs4::check_compound_status(dec);
    nfs4::decode_putfh_result(dec);
    nfs4::decode_savefh_result(dec);
    nfs4::decode_putfh_result(dec);
    nfs4::decode_rename_result(dec);
}

Nfs4Fh Nfs4Client::symlink(const Nfs4Fh& dir, const std::string& name,
                             const std::string& target, const nfs4::Sattr4& attrs) {
    XdrEncoder ops;
    encode_fh(ops, dir);
    nfs4::encode_create_symlink(ops, name, target, attrs);
    nfs4::encode_getfh(ops);
    auto reply = nfs4::call_compound(*rpc_, "", ops.release(), 3);
    XdrDecoder dec(reply);
    nfs4::check_compound_status(dec);
    nfs4::decode_putfh_result(dec);
    nfs4::decode_create_result(dec);
    return nfs4::decode_getfh_result(dec);
}

std::string Nfs4Client::readlink(const Nfs4Fh& fh) {
    XdrEncoder ops;
    encode_fh(ops, fh);
    nfs4::encode_readlink(ops);
    auto reply = nfs4::call_compound(*rpc_, "", ops.release(), 2);
    XdrDecoder dec(reply);
    nfs4::check_compound_status(dec);
    nfs4::decode_putfh_result(dec);
    return nfs4::decode_readlink_result(dec);
}

void Nfs4Client::setattr(const Nfs4Fh& fh, const nfs4::Sattr4& attrs) {
    // Use anonymous stateid (all zeros) for SETATTR without open state
    Stateid4 anon{};
    XdrEncoder ops;
    encode_fh(ops, fh);
    nfs4::encode_setattr(ops, anon, attrs);
    auto reply = nfs4::call_compound(*rpc_, "", ops.release(), 2);
    XdrDecoder dec(reply);
    nfs4::check_compound_status(dec);
    nfs4::decode_putfh_result(dec);
    nfs4::decode_setattr_result(dec);
}

// ── Directory listing ─────────────────────────────────────────────────────────

std::vector<Nfs4DirEntry> Nfs4Client::readdir(const Nfs4Fh& dir) {
    std::vector<Nfs4DirEntry> all;
    std::array<uint8_t, 8> cookieverf{};
    uint64_t cookie = 0;

    while (true) {
        XdrEncoder ops;
        encode_fh(ops, dir);
        nfs4::encode_readdir(ops, cookie, cookieverf,
                             4096, 32768,
                             {nfs4::attr::TYPE, nfs4::attr::SIZE, nfs4::attr::FILEID,
                              nfs4::attr::MODE, nfs4::attr::TIME_MODIFY});
        auto reply = nfs4::call_compound(*rpc_, "", ops.release(), 2);
        XdrDecoder dec(reply);
        nfs4::check_compound_status(dec);
        nfs4::decode_putfh_result(dec);
        auto page = nfs4::decode_readdir_result(dec);

        cookieverf = page.cookieverf;
        for (auto& e : page.entries) {
            cookie = e.cookie;
            all.push_back(std::move(e));
        }

        if (page.eof) break;
    }
    return all;
}

// ── Lease renewal ─────────────────────────────────────────────────────────────

void Nfs4Client::renew() {
    XdrEncoder ops;
    nfs4::encode_renew(ops, clientid_);
    auto reply = nfs4::call_compound(*rpc_, "", ops.release(), 1);
    XdrDecoder dec(reply);
    nfs4::check_compound_status(dec);
    nfs4::decode_renew_result(dec);
}
