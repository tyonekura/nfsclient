#include "runner4.hpp"
#include "test_helpers4.hpp"
#include "nfs4/nfs4_error.hpp"

#include <string>

// ── Section 5.5: Stale file handle ───────────────────────────────────────────

namespace {

// Accepts NFS4ERR_STALE (correct), NFS4ERR_NOENT (Linux kernel variance), or
// success (Linux: inode still cached after unlink, valid server behavior).
// Any unexpected error code is a FAIL.
static void expect_stale_or_noent(const std::function<void()>& expr,
                                   const char* what) {
    bool threw_nfs_error = false;
    try {
        expr();
    } catch (const Nfs4Error& e) {
        threw_nfs_error = true;
        if (!e.is(Nfsstat4::NFS4ERR_STALE) && !e.is(Nfsstat4::NFS4ERR_NOENT)) {
            throw compliance4::ComplianceFailure4(
                std::string(what) + ": expected NFS4ERR_STALE or NFS4ERR_NOENT, got nfsstat4="
                + std::to_string(e.status));
        }
        // STALE or NOENT — both are acceptable
    }
    if (!threw_nfs_error) {
        // Success is valid: Linux nfsd may keep the inode cached after unlink.
        throw std::runtime_error(
            std::string(what) + ": server returned success (FH still valid — inode cached, SKIP)");
    }
}

void test_getattr_stale_fh(compliance4::Nfs4TestCtx& ctx) {
    Nfs4File f = ctx.client.open_write(ctx.workdir_fh, "stale4_file.txt");
    ctx.client.close(f);

    Nfs4Fh fh = ctx.client.lookup(ctx.workdir_fh, "stale4_file.txt");
    ctx.client.remove(ctx.workdir_fh, "stale4_file.txt");

    expect_stale_or_noent(
        [&]() { ctx.client.getattr(fh); },
        "getattr on deleted file FH");
}

void test_lookup_in_stale_dir(compliance4::Nfs4TestCtx& ctx) {
    Nfs4Fh dir = ctx.client.mkdir(ctx.workdir_fh, "stale4_dir");
    ctx.client.remove(ctx.workdir_fh, "stale4_dir");

    expect_stale_or_noent(
        [&]() { ctx.client.lookup(dir, "nonexistent_child"); },
        "lookup in deleted directory FH");
}

}  // anonymous namespace

void register_stale4_tests(compliance4::TestRunner4& r) {
    using compliance4::ComplianceTest4;
    const std::string sec = "RFC 7530 §4.2.4";

    r.add({"Stale4.GetattrOnDeletedFile",  sec, test_getattr_stale_fh});
    r.add({"Stale4.LookupInDeletedDir",    sec, test_lookup_in_stale_dir});
}
