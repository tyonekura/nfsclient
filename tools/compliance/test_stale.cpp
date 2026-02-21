#include "runner.hpp"
#include "test_helpers.hpp"

#include <string>

// ── Section 2.7: Stale file handle — RFC 1813 §2.5 ───────────────────────────
//
// Obtain a file handle, delete the underlying object via a different path, then
// attempt to use the stale FH.  The server must return NFS3ERR_STALE.

namespace {

void test_read_on_deleted_file(compliance::TestCtx& ctx) {
    Fh3 fh = ctx.client.create(ctx.workdir_fh, "s_stale_file.txt");
    const std::string payload = "will be deleted";
    ctx.client.write(fh, 0, Stable3::FILE_SYNC,
                     reinterpret_cast<const uint8_t*>(payload.data()), payload.size());

    // Delete the file via the directory — fh is now stale.
    ctx.client.remove(ctx.workdir_fh, "s_stale_file.txt");

    // Reading the stale FH should return NFS3ERR_STALE.
    EXPECT_NFS_ERR(
        ctx.client.read(fh, 0, 512),
        Nfsstat3::NFS3ERR_STALE);
}

void test_lookup_in_deleted_dir(compliance::TestCtx& ctx) {
    Fh3 dir_fh = ctx.client.mkdir(ctx.workdir_fh, "s_stale_dir");

    // Create a file inside the directory so lookup has something to find.
    ctx.client.create(dir_fh, "inside.txt");

    // Remove the directory entry from the parent (requires removing the file
    // first, then the directory itself).
    ctx.client.remove(dir_fh, "inside.txt");
    ctx.client.rmdir(ctx.workdir_fh, "s_stale_dir");

    // dir_fh is now stale — lookup inside it should return NFS3ERR_STALE.
    EXPECT_NFS_ERR(
        ctx.client.lookup(dir_fh, "anything"),
        Nfsstat3::NFS3ERR_STALE);
}

}  // anonymous namespace

void register_stale_tests(compliance::TestRunner& r) {
    using compliance::ComplianceTest;
    const std::string sec = "RFC 1813 §2.5";

    r.add({"Stale.ReadOnDeletedFile",    sec, test_read_on_deleted_file});
    r.add({"Stale.LookupInDeletedDir",   sec, test_lookup_in_deleted_dir});
}
