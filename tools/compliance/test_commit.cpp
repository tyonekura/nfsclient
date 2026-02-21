#include "runner.hpp"
#include "test_helpers.hpp"
#include "nfs/commit.hpp"
#include "nfs/fsinfo.hpp"
#include "nfs/create.hpp"

#include <cstdint>
#include <string>

// ── Section 2.3: COMMIT / write verifier — RFC 1813 §3.3.21 ──────────────────
// ── Section 2.4: EXCLUSIVE CREATE verifier — RFC 1813 §3.3.8 ─────────────────

namespace {

// 2.3 — COMMIT and write verifier consistency

void test_unstable_write_commit(compliance::TestCtx& ctx) {
    Fh3 fh = ctx.client.create(ctx.workdir_fh, "c_unstable.txt");
    const std::string payload = "unstable data";
    ctx.client.write(fh, 0, Stable3::UNSTABLE,
                     reinterpret_cast<const uint8_t*>(payload.data()), payload.size());

    // COMMIT must succeed without throwing
    nfs3::CommitVerf3 verf = ctx.client.commit(fh);
    (void)verf;  // verifier value is checked in subsequent tests

    ctx.client.remove(ctx.workdir_fh, "c_unstable.txt");
}

void test_commit_verifier_consistency(compliance::TestCtx& ctx) {
    Fh3 fh = ctx.client.create(ctx.workdir_fh, "c_verf_consistency.txt");
    const std::string payload = "verifier test";
    ctx.client.write(fh, 0, Stable3::UNSTABLE,
                     reinterpret_cast<const uint8_t*>(payload.data()), payload.size());

    nfs3::CommitVerf3 v1 = ctx.client.commit(fh);
    nfs3::CommitVerf3 v2 = ctx.client.commit(fh);

    // Without a server restart the verifier must be stable
    CHECK(v1 == v2);

    ctx.client.remove(ctx.workdir_fh, "c_verf_consistency.txt");
}

void test_filesync_committed(compliance::TestCtx& ctx) {
    // FILE_SYNC must report committed >= FILE_SYNC (i.e. committed == FILE_SYNC)
    Fh3 fh = ctx.client.create(ctx.workdir_fh, "c_filesync.txt");
    const std::string payload = "filesync committed";
    WriteResult r = ctx.client.write(fh, 0, Stable3::FILE_SYNC,
                                     reinterpret_cast<const uint8_t*>(payload.data()),
                                     payload.size());
    // committed must be FILE_SYNC (2) when requested stability is FILE_SYNC
    CHECK(static_cast<uint32_t>(r.committed) >=
          static_cast<uint32_t>(Stable3::FILE_SYNC));
    ctx.client.remove(ctx.workdir_fh, "c_filesync.txt");
}

// 2.4 — EXCLUSIVE CREATE verifier

void test_exclusive_create_same_verifier(compliance::TestCtx& ctx) {
    nfs3::CreateVerf3 verf = {0xAB, 0xCD, 0xEF, 0x01, 0x23, 0x45, 0x67, 0x89};

    Fh3 fh1 = ctx.client.create_exclusive(ctx.workdir_fh, "c_excl_same.txt", verf);
    // Same verifier → idempotent; must return a handle to the same file
    Fh3 fh2 = ctx.client.create_exclusive(ctx.workdir_fh, "c_excl_same.txt", verf);

    Fattr3 a1 = ctx.client.getattr(fh1);
    Fattr3 a2 = ctx.client.getattr(fh2);
    CHECK(a1.fileid == a2.fileid);

    ctx.client.remove(ctx.workdir_fh, "c_excl_same.txt");
}

void test_exclusive_create_diff_verifier(compliance::TestCtx& ctx) {
    nfs3::CreateVerf3 verf1 = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    nfs3::CreateVerf3 verf2 = {0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x00};

    ctx.client.create_exclusive(ctx.workdir_fh, "c_excl_diff.txt", verf1);

    // Different verifier on an existing file → NFS3ERR_EXIST
    EXPECT_NFS_ERR(
        ctx.client.create_exclusive(ctx.workdir_fh, "c_excl_diff.txt", verf2),
        Nfsstat3::NFS3ERR_EXIST);

    ctx.client.remove(ctx.workdir_fh, "c_excl_diff.txt");
}

}  // anonymous namespace

void register_commit_tests(compliance::TestRunner& r) {
    using compliance::ComplianceTest;

    r.add({"Commit.UnstableWriteThenCommit",   "RFC 1813 §3.3.21", test_unstable_write_commit});
    r.add({"Commit.VerifierConsistency",        "RFC 1813 §3.3.21", test_commit_verifier_consistency});
    r.add({"Commit.FileSyncCommitted",          "RFC 1813 §3.3.7",  test_filesync_committed});
    r.add({"ExclCreate.SameVerifierIdempotent", "RFC 1813 §3.3.8",  test_exclusive_create_same_verifier});
    r.add({"ExclCreate.DiffVerifierExist",      "RFC 1813 §3.3.8",  test_exclusive_create_diff_verifier});
}
