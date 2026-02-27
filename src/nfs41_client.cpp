#include "nfs41_client.hpp"
#include "nfs4/compound.hpp"
#include "nfs4/session41.hpp"
#include "nfs4/fh_ops.hpp"
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

// ── encode_fh helper (same logic as v4.0) ────────────────────────────────────

static void encode_fh(XdrEncoder& ops, const Nfs4Fh& fh) {
    if (fh.data.empty())
        nfs4::encode_putrootfh(ops);
    else
        nfs4::encode_putfh(ops, fh);
}

// ── Nfs41Client::compound41 ───────────────────────────────────────────────────

std::vector<uint8_t> Nfs41Client::compound41(const std::string& tag,
                                               const std::vector<uint8_t>& ops_bytes,
                                               uint32_t num_ops) {
    XdrEncoder seq;
    nfs4::encode_sequence41(seq, sessionid_, slot_seqid_++);
    auto seq_bytes = seq.release();

    std::vector<uint8_t> all_ops;
    all_ops.reserve(seq_bytes.size() + ops_bytes.size());
    all_ops.insert(all_ops.end(), seq_bytes.begin(), seq_bytes.end());
    all_ops.insert(all_ops.end(), ops_bytes.begin(), ops_bytes.end());

    return nfs4::call_compound(*rpc_, tag, all_ops, num_ops + 1, /*minorversion=*/1);
}

// ── Constructors ──────────────────────────────────────────────────────────────

static void do_bootstrap(TcpRpcClient& rpc,
                          uint64_t& clientid_out,
                          SessionId41& sessionid_out) {
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    std::array<uint8_t, 8> verifier{};
    for (int i = 7; i >= 0; --i) {
        verifier[static_cast<size_t>(i)] = static_cast<uint8_t>(now & 0xFF);
        now >>= 8;
    }

    // EXCHANGE_ID — no SEQUENCE prefix, outside any session
    XdrEncoder ops1;
    nfs4::encode_exchange_id(ops1, verifier, "nfsclient-v41");
    auto reply1 = nfs4::call_compound(rpc, "init", ops1.release(), 1, /*minorversion=*/1);
    XdrDecoder dec1(reply1);
    nfs4::check_compound_status(dec1);
    auto exid = nfs4::decode_exchange_id_result(dec1);

    // CREATE_SESSION — no SEQUENCE prefix, outside any session
    XdrEncoder ops2;
    nfs4::encode_create_session(ops2, exid.clientid, exid.sequenceid);
    auto reply2 = nfs4::call_compound(rpc, "init", ops2.release(), 1, /*minorversion=*/1);
    XdrDecoder dec2(reply2);
    nfs4::check_compound_status(dec2);
    auto sid = nfs4::decode_create_session_result(dec2);

    clientid_out  = exid.clientid;
    sessionid_out = sid;
}

Nfs41Client::Nfs41Client(const std::string& host) : host_(host) {
    const uint16_t port = nfs3::getport(host_, NFS4_PROG, NFS4_VERS);
    rpc_ = std::make_unique<TcpRpcClient>(host_, port);
    do_bootstrap(*rpc_, clientid_, sessionid_);
    slot_seqid_ = 1;

    // RECLAIM_COMPLETE — first COMPOUND inside the session (with SEQUENCE)
    XdrEncoder ops_rc;
    nfs4::encode_reclaim_complete(ops_rc);
    auto reply_rc = compound41("init", ops_rc.release(), 1);
    XdrDecoder dec_rc(reply_rc);
    nfs4::check_compound_status(dec_rc);
    nfs4::decode_sequence41_result(dec_rc);
    nfs4::decode_reclaim_complete_result(dec_rc);

    // Verify root is reachable; root_fh_ stays empty (PUTROOTFH sentinel)
    XdrEncoder ops_root;
    nfs4::encode_putrootfh(ops_root);
    nfs4::encode_getfh(ops_root);
    auto reply_root = compound41("", ops_root.release(), 2);
    XdrDecoder dec_root(reply_root);
    nfs4::check_compound_status(dec_root);
    nfs4::decode_sequence41_result(dec_root);
    nfs4::decode_putrootfh_result(dec_root);
    nfs4::decode_getfh_result(dec_root);  // discard; use PUTROOTFH for root ops
    root_fh_ = Nfs4Fh{};
}

Nfs41Client::Nfs41Client(const std::string& host, const AuthSys& auth) : host_(host) {
    const uint16_t port = nfs3::getport(host_, NFS4_PROG, NFS4_VERS);
    rpc_ = std::make_unique<TcpRpcClient>(host_, port);
    rpc_->set_auth_sys(auth);
    do_bootstrap(*rpc_, clientid_, sessionid_);
    slot_seqid_ = 1;

    XdrEncoder ops_rc;
    nfs4::encode_reclaim_complete(ops_rc);
    auto reply_rc = compound41("init", ops_rc.release(), 1);
    XdrDecoder dec_rc(reply_rc);
    nfs4::check_compound_status(dec_rc);
    nfs4::decode_sequence41_result(dec_rc);
    nfs4::decode_reclaim_complete_result(dec_rc);

    XdrEncoder ops_root;
    nfs4::encode_putrootfh(ops_root);
    nfs4::encode_getfh(ops_root);
    auto reply_root = compound41("", ops_root.release(), 2);
    XdrDecoder dec_root(reply_root);
    nfs4::check_compound_status(dec_root);
    nfs4::decode_sequence41_result(dec_root);
    nfs4::decode_putrootfh_result(dec_root);
    nfs4::decode_getfh_result(dec_root);
    root_fh_ = Nfs4Fh{};
}

Nfs41Client::~Nfs41Client() {
    // Best-effort DESTROY_SESSION on shutdown
    try {
        XdrEncoder ops;
        nfs4::encode_destroy_session(ops, sessionid_);
        nfs4::call_compound(*rpc_, "destroy", ops.release(), 1, /*minorversion=*/1);
    } catch (...) {}
}

void Nfs41Client::set_auth_sys(const AuthSys& auth) { rpc_->set_auth_sys(auth); }
void Nfs41Client::clear_auth()                       { rpc_->clear_auth(); }

// ── File handle operations ────────────────────────────────────────────────────

Nfs4Fh Nfs41Client::lookup(const Nfs4Fh& dir, const std::string& name) {
    XdrEncoder ops;
    encode_fh(ops, dir);
    nfs4::encode_lookup(ops, name);
    nfs4::encode_getfh(ops);
    auto reply = compound41("", ops.release(), 3);
    XdrDecoder dec(reply);
    nfs4::check_compound_status(dec);
    nfs4::decode_sequence41_result(dec);
    nfs4::decode_putfh_result(dec);
    nfs4::decode_lookup_result(dec);
    return nfs4::decode_getfh_result(dec);
}

Fattr4 Nfs41Client::getattr(const Nfs4Fh& fh) {
    XdrEncoder ops;
    encode_fh(ops, fh);
    nfs4::encode_getattr(ops, {
        nfs4::attr::TYPE, nfs4::attr::CHANGE, nfs4::attr::SIZE,
        nfs4::attr::FILEID, nfs4::attr::MODE, nfs4::attr::NUMLINKS,
        nfs4::attr::OWNER, nfs4::attr::OWNER_GROUP,
        nfs4::attr::TIME_ACCESS, nfs4::attr::TIME_METADATA, nfs4::attr::TIME_MODIFY
    });
    auto reply = compound41("", ops.release(), 2);
    XdrDecoder dec(reply);
    nfs4::check_compound_status(dec);
    nfs4::decode_sequence41_result(dec);
    nfs4::decode_putfh_result(dec);
    return nfs4::decode_getattr_result(dec);
}

uint32_t Nfs41Client::access(const Nfs4Fh& fh, uint32_t mask) {
    XdrEncoder ops;
    encode_fh(ops, fh);
    nfs4::encode_access(ops, mask);
    auto reply = compound41("", ops.release(), 2);
    XdrDecoder dec(reply);
    nfs4::check_compound_status(dec);
    nfs4::decode_sequence41_result(dec);
    nfs4::decode_putfh_result(dec);
    return nfs4::decode_access_result(dec).access;
}

// ── Open / close ─────────────────────────────────────────────────────────────

Nfs4File Nfs41Client::do_open(const Nfs4Fh& dir, const std::string& name,
                               uint32_t share_access, bool create) {
    static constexpr uint32_t NFS4ERR_GRACE = 10013;

    uint32_t seqid = ++open_seqid_;

    std::vector<uint8_t> reply;
    while (true) {
        XdrEncoder ops;
        encode_fh(ops, dir);
        if (create) {
            nfs4::encode_open_create(ops, seqid, share_access,
                                     clientid_, "nfsclient-v41", name);
        } else {
            nfs4::encode_open_nocreate(ops, seqid, share_access,
                                       clientid_, "nfsclient-v41", name);
        }
        nfs4::encode_getfh(ops);
        reply = compound41("", ops.release(), 3);
        XdrDecoder dec(reply);
        try {
            nfs4::check_compound_status(dec);
        } catch (const Nfs4Error& e) {
            if (e.status == NFS4ERR_GRACE) {
                ::sleep(5);
                continue;
            }
            throw;
        }
        break;
    }

    XdrDecoder dec(reply);
    nfs4::check_compound_status(dec);
    nfs4::decode_sequence41_result(dec);
    nfs4::decode_putfh_result(dec);
    auto open_res = nfs4::decode_open_result(dec);
    Nfs4Fh fh = nfs4::decode_getfh_result(dec);

    // In NFSv4.1, OPEN_CONFIRM must NOT be requested (RFC 8881 §18.16.3)
    if (open_res.rflags & nfs4::OPEN4_RESULT_CONFIRM)
        throw std::runtime_error("NFSv4.1 server set OPEN4_RESULT_CONFIRM — protocol error");

    Nfs4File f;
    f.fh      = fh;
    f.stateid = open_res.stateid;
    f.seqid   = seqid;
    return f;
}

Nfs4File Nfs41Client::open_read(const Nfs4Fh& dir, const std::string& name) {
    return do_open(dir, name, nfs4::OPEN4_SHARE_ACCESS_READ, false);
}

Nfs4File Nfs41Client::open_write(const Nfs4Fh& dir, const std::string& name, bool create) {
    return do_open(dir, name, nfs4::OPEN4_SHARE_ACCESS_WRITE, create);
}

void Nfs41Client::close(const Nfs4File& f) {
    XdrEncoder ops;
    encode_fh(ops, f.fh);
    nfs4::encode_close(ops, f.seqid, f.stateid);
    auto reply = compound41("", ops.release(), 2);
    XdrDecoder dec(reply);
    nfs4::check_compound_status(dec);
    nfs4::decode_sequence41_result(dec);
    nfs4::decode_putfh_result(dec);
    nfs4::decode_close_result(dec);
}

// ── Data operations ───────────────────────────────────────────────────────────

std::vector<uint8_t> Nfs41Client::read(const Nfs4File& f,
                                        uint64_t offset, uint32_t count) {
    XdrEncoder ops;
    encode_fh(ops, f.fh);
    nfs4::encode_read(ops, f.stateid, offset, count);
    auto reply = compound41("", ops.release(), 2);
    XdrDecoder dec(reply);
    nfs4::check_compound_status(dec);
    nfs4::decode_sequence41_result(dec);
    nfs4::decode_putfh_result(dec);
    return nfs4::decode_read_result(dec);
}

uint32_t Nfs41Client::write(const Nfs4File& f, uint64_t offset, Stable4 stable,
                             const uint8_t* data, uint32_t len) {
    XdrEncoder ops;
    encode_fh(ops, f.fh);
    nfs4::encode_write(ops, f.stateid, offset, stable, data, len);
    auto reply = compound41("", ops.release(), 2);
    XdrDecoder dec(reply);
    nfs4::check_compound_status(dec);
    nfs4::decode_sequence41_result(dec);
    nfs4::decode_putfh_result(dec);
    return nfs4::decode_write_result(dec).count;
}

std::array<uint8_t, 8> Nfs41Client::commit(const Nfs4File& f,
                                             uint64_t offset, uint32_t count) {
    XdrEncoder ops;
    encode_fh(ops, f.fh);
    nfs4::encode_commit(ops, offset, count);
    auto reply = compound41("", ops.release(), 2);
    XdrDecoder dec(reply);
    nfs4::check_compound_status(dec);
    nfs4::decode_sequence41_result(dec);
    nfs4::decode_putfh_result(dec);
    return nfs4::decode_commit_result(dec);
}

// ── Namespace operations ──────────────────────────────────────────────────────

Nfs4Fh Nfs41Client::mkdir(const Nfs4Fh& dir, const std::string& name,
                           const nfs4::Sattr4& attrs) {
    XdrEncoder ops;
    encode_fh(ops, dir);
    nfs4::encode_create_dir(ops, name, attrs);
    nfs4::encode_getfh(ops);
    auto reply = compound41("", ops.release(), 3);
    XdrDecoder dec(reply);
    nfs4::check_compound_status(dec);
    nfs4::decode_sequence41_result(dec);
    nfs4::decode_putfh_result(dec);
    nfs4::decode_create_result(dec);
    return nfs4::decode_getfh_result(dec);
}

void Nfs41Client::remove(const Nfs4Fh& dir, const std::string& name) {
    XdrEncoder ops;
    encode_fh(ops, dir);
    nfs4::encode_remove(ops, name);
    auto reply = compound41("", ops.release(), 2);
    XdrDecoder dec(reply);
    nfs4::check_compound_status(dec);
    nfs4::decode_sequence41_result(dec);
    nfs4::decode_putfh_result(dec);
    nfs4::decode_remove_result(dec);
}

void Nfs41Client::rename(const Nfs4Fh& src_dir, const std::string& src_name,
                         const Nfs4Fh& dst_dir, const std::string& dst_name) {
    XdrEncoder ops;
    encode_fh(ops, src_dir);
    nfs4::encode_savefh(ops);
    encode_fh(ops, dst_dir);
    nfs4::encode_rename(ops, src_name, dst_name);
    auto reply = compound41("", ops.release(), 4);
    XdrDecoder dec(reply);
    nfs4::check_compound_status(dec);
    nfs4::decode_sequence41_result(dec);
    nfs4::decode_putfh_result(dec);
    nfs4::decode_savefh_result(dec);
    nfs4::decode_putfh_result(dec);
    nfs4::decode_rename_result(dec);
}

Nfs4Fh Nfs41Client::symlink(const Nfs4Fh& dir, const std::string& name,
                              const std::string& target, const nfs4::Sattr4& attrs) {
    XdrEncoder ops;
    encode_fh(ops, dir);
    nfs4::encode_create_symlink(ops, name, target, attrs);
    nfs4::encode_getfh(ops);
    auto reply = compound41("", ops.release(), 3);
    XdrDecoder dec(reply);
    nfs4::check_compound_status(dec);
    nfs4::decode_sequence41_result(dec);
    nfs4::decode_putfh_result(dec);
    nfs4::decode_create_result(dec);
    return nfs4::decode_getfh_result(dec);
}

std::string Nfs41Client::readlink(const Nfs4Fh& fh) {
    XdrEncoder ops;
    encode_fh(ops, fh);
    nfs4::encode_readlink(ops);
    auto reply = compound41("", ops.release(), 2);
    XdrDecoder dec(reply);
    nfs4::check_compound_status(dec);
    nfs4::decode_sequence41_result(dec);
    nfs4::decode_putfh_result(dec);
    return nfs4::decode_readlink_result(dec);
}

void Nfs41Client::setattr(const Nfs4Fh& fh, const nfs4::Sattr4& attrs) {
    Stateid4 anon{};
    XdrEncoder ops;
    encode_fh(ops, fh);
    nfs4::encode_setattr(ops, anon, attrs);
    auto reply = compound41("", ops.release(), 2);
    XdrDecoder dec(reply);
    nfs4::check_compound_status(dec);
    nfs4::decode_sequence41_result(dec);
    nfs4::decode_putfh_result(dec);
    nfs4::decode_setattr_result(dec);
}

// ── Directory listing ─────────────────────────────────────────────────────────

std::vector<Nfs4DirEntry> Nfs41Client::readdir(const Nfs4Fh& dir) {
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
        auto reply = compound41("", ops.release(), 2);
        XdrDecoder dec(reply);
        nfs4::check_compound_status(dec);
        nfs4::decode_sequence41_result(dec);
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
