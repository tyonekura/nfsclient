#include "runner.hpp"
#include "test_helpers.hpp"

#include <cstdint>
#include <string>

// ── Section 2.2: Write consistency (WCC) — RFC 1813 §2.6 ─────────────────────
//
// These tests verify WCC behaviour by comparing GETATTR results taken before
// and after a mutating operation.  We do NOT parse the wcc_data embedded in
// the RPC replies; GETATTR is the authoritative source of truth.

namespace {

void test_write_pre_op_size(compliance::TestCtx& ctx) {
    // Create a file, note the size before the write.
    Fh3 fh = ctx.client.create(ctx.workdir_fh, "w_pre_size.txt");

    const std::string first = "initial";
    ctx.client.write(fh, 0, Stable3::FILE_SYNC,
                     reinterpret_cast<const uint8_t*>(first.data()), first.size());

    Fattr3 before = ctx.client.getattr(fh);

    const std::string extra = "extra data appended";
    ctx.client.write(fh, before.size, Stable3::FILE_SYNC,
                     reinterpret_cast<const uint8_t*>(extra.data()), extra.size());

    Fattr3 after = ctx.client.getattr(fh);

    // Size should have grown
    CHECK(after.size > before.size);
    CHECK(after.size == before.size + extra.size());

    ctx.client.remove(ctx.workdir_fh, "w_pre_size.txt");
}

void test_write_post_op_size(compliance::TestCtx& ctx) {
    Fh3 fh = ctx.client.create(ctx.workdir_fh, "w_post_size.txt");

    const std::string payload = "hello world";
    ctx.client.write(fh, 0, Stable3::FILE_SYNC,
                     reinterpret_cast<const uint8_t*>(payload.data()), payload.size());

    Fattr3 attrs = ctx.client.getattr(fh);
    CHECK(attrs.size == payload.size());

    ctx.client.remove(ctx.workdir_fh, "w_post_size.txt");
}

void test_create_post_op_dir_mtime(compliance::TestCtx& ctx) {
    // GETATTR on the workdir before and after creating a file.
    Fattr3 before = ctx.client.getattr(ctx.workdir_fh);

    ctx.client.create(ctx.workdir_fh, "w_dir_mtime.txt");

    Fattr3 after = ctx.client.getattr(ctx.workdir_fh);

    // Parent directory mtime must advance (or at least not go backward).
    // A well-behaved server sets mtime to the time of the mutation.
    uint64_t mt_before = static_cast<uint64_t>(before.mtime.seconds) * 1'000'000'000
                       + before.mtime.nseconds;
    uint64_t mt_after  = static_cast<uint64_t>(after.mtime.seconds)  * 1'000'000'000
                       + after.mtime.nseconds;
    CHECK(mt_after >= mt_before);

    ctx.client.remove(ctx.workdir_fh, "w_dir_mtime.txt");
}

void test_remove_post_op_dir_nlink(compliance::TestCtx& ctx) {
    // Create a subdirectory so that we can observe nlink change on workdir_fh.
    Fattr3 before = ctx.client.getattr(ctx.workdir_fh);
    ctx.client.mkdir(ctx.workdir_fh, "w_rmdir_nlink");
    Fattr3 mid = ctx.client.getattr(ctx.workdir_fh);
    CHECK(mid.nlink > before.nlink);  // mkdir added a hard-link (the '..' entry)

    ctx.client.rmdir(ctx.workdir_fh, "w_rmdir_nlink");
    Fattr3 after = ctx.client.getattr(ctx.workdir_fh);
    CHECK(after.nlink == before.nlink);  // nlink must return to original value
}

}  // anonymous namespace

void register_wcc_tests(compliance::TestRunner& r) {
    using compliance::ComplianceTest;
    const std::string sec = "RFC 1813 §2.6";

    r.add({"WCC.WritePreOpSize",         sec, test_write_pre_op_size});
    r.add({"WCC.WritePostOpSize",        sec, test_write_post_op_size});
    r.add({"WCC.CreatePostOpDirMtime",   sec, test_create_post_op_dir_mtime});
    r.add({"WCC.RemovePostOpDirNlink",   sec, test_remove_post_op_dir_nlink});
}
