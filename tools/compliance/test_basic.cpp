#include "runner.hpp"
#include "test_helpers.hpp"
#include "nfs/nfs_error.hpp"
#include "nfs/create.hpp"

#include <cstdint>
#include <string>
#include <vector>

// ── Section 2.1: Basic operations ────────────────────────────────────────────

namespace {

void test_lookup_existing(compliance::TestCtx& ctx) {
    ctx.client.create(ctx.workdir_fh, "b_lookup_exist.txt");
    Fh3 fh = ctx.client.lookup(ctx.workdir_fh, "b_lookup_exist.txt");
    Fattr3 attrs = ctx.client.getattr(fh);
    CHECK(attrs.type == Ftype3::NF3REG);
    ctx.client.remove(ctx.workdir_fh, "b_lookup_exist.txt");
}

void test_lookup_noent(compliance::TestCtx& ctx) {
    EXPECT_NFS_ERR(
        ctx.client.lookup(ctx.workdir_fh, "b_no_such_file_xyz"),
        Nfsstat3::NFS3ERR_NOENT);
}

void test_lookup_notdir(compliance::TestCtx& ctx) {
    ctx.client.create(ctx.workdir_fh, "b_notdir.txt");
    Fh3 file_fh = ctx.client.lookup(ctx.workdir_fh, "b_notdir.txt");
    EXPECT_NFS_ERR(
        ctx.client.lookup(file_fh, "child"),
        Nfsstat3::NFS3ERR_NOTDIR);
    ctx.client.remove(ctx.workdir_fh, "b_notdir.txt");
}

void test_read_exact_size(compliance::TestCtx& ctx) {
    Fh3 fh = ctx.client.create(ctx.workdir_fh, "b_read_exact.txt");
    const std::string payload = "Hello, NFS compliance!";
    ctx.client.write(fh, 0, Stable3::FILE_SYNC,
                     reinterpret_cast<const uint8_t*>(payload.data()), payload.size());
    auto data = ctx.client.read(fh, 0, static_cast<uint32_t>(payload.size()));
    CHECK(data.size() == payload.size());
    CHECK(std::string(data.begin(), data.end()) == payload);
    ctx.client.remove(ctx.workdir_fh, "b_read_exact.txt");
}

void test_read_past_eof(compliance::TestCtx& ctx) {
    Fh3 fh = ctx.client.create(ctx.workdir_fh, "b_read_eof.txt");
    const std::string payload = "short";
    ctx.client.write(fh, 0, Stable3::FILE_SYNC,
                     reinterpret_cast<const uint8_t*>(payload.data()), payload.size());
    // Read starting well past the end of the file
    auto data = ctx.client.read(fh, 1000, 512);
    CHECK(data.empty());
    ctx.client.remove(ctx.workdir_fh, "b_read_eof.txt");
}

void test_write_filesync_count(compliance::TestCtx& ctx) {
    Fh3 fh = ctx.client.create(ctx.workdir_fh, "b_write_sync.txt");
    const std::string payload = "write filesync test";
    WriteResult r = ctx.client.write(fh, 0, Stable3::FILE_SYNC,
                                     reinterpret_cast<const uint8_t*>(payload.data()),
                                     payload.size());
    CHECK(r.count == static_cast<uint32_t>(payload.size()));
    ctx.client.remove(ctx.workdir_fh, "b_write_sync.txt");
}

void test_write_then_getattr(compliance::TestCtx& ctx) {
    Fh3 fh = ctx.client.create(ctx.workdir_fh, "b_write_getattr.txt");
    const std::string payload = "size check";
    ctx.client.write(fh, 0, Stable3::FILE_SYNC,
                     reinterpret_cast<const uint8_t*>(payload.data()), payload.size());
    Fattr3 attrs = ctx.client.getattr(fh);
    CHECK(attrs.size == payload.size());
    ctx.client.remove(ctx.workdir_fh, "b_write_getattr.txt");
}

void test_create_guarded_duplicate(compliance::TestCtx& ctx) {
    ctx.client.create(ctx.workdir_fh, "b_guarded.txt",
                      nfs3::CreateMode3::GUARDED);
    EXPECT_NFS_ERR(
        ctx.client.create(ctx.workdir_fh, "b_guarded.txt",
                          nfs3::CreateMode3::GUARDED),
        Nfsstat3::NFS3ERR_EXIST);
    ctx.client.remove(ctx.workdir_fh, "b_guarded.txt");
}

void test_create_exclusive_idempotent(compliance::TestCtx& ctx) {
    nfs3::CreateVerf3 verf = {0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03, 0x04};
    Fh3 fh1 = ctx.client.create_exclusive(ctx.workdir_fh, "b_exclusive.txt", verf);
    // Second call with the same verifier must not fail (idempotent re-creation)
    Fh3 fh2 = ctx.client.create_exclusive(ctx.workdir_fh, "b_exclusive.txt", verf);
    // Both FHs should resolve to the same file
    Fattr3 a1 = ctx.client.getattr(fh1);
    Fattr3 a2 = ctx.client.getattr(fh2);
    CHECK(a1.fileid == a2.fileid);
    ctx.client.remove(ctx.workdir_fh, "b_exclusive.txt");
}

void test_remove_noent(compliance::TestCtx& ctx) {
    EXPECT_NFS_ERR(
        ctx.client.remove(ctx.workdir_fh, "b_no_such_file_to_remove"),
        Nfsstat3::NFS3ERR_NOENT);
}

void test_rename_across_dirs(compliance::TestCtx& ctx) {
    // Create source dir and destination dir
    Fh3 src_dir = ctx.client.mkdir(ctx.workdir_fh, "b_rename_src");
    Fh3 dst_dir = ctx.client.mkdir(ctx.workdir_fh, "b_rename_dst");

    // Create a file in src_dir
    ctx.client.create(src_dir, "file.txt");

    // Rename it to dst_dir
    ctx.client.rename(src_dir, "file.txt", dst_dir, "file_moved.txt");

    // Source should be gone
    EXPECT_NFS_ERR(
        ctx.client.lookup(src_dir, "file.txt"),
        Nfsstat3::NFS3ERR_NOENT);

    // Destination should exist
    Fh3 moved = ctx.client.lookup(dst_dir, "file_moved.txt");
    Fattr3 attrs = ctx.client.getattr(moved);
    CHECK(attrs.type == Ftype3::NF3REG);

    // Cleanup
    ctx.client.remove(dst_dir, "file_moved.txt");
    ctx.client.rmdir(ctx.workdir_fh, "b_rename_src");
    ctx.client.rmdir(ctx.workdir_fh, "b_rename_dst");
}

}  // anonymous namespace

void register_basic_tests(compliance::TestRunner& r) {
    using compliance::ComplianceTest;
    const std::string sec = "RFC 1813";

    r.add({"Basic.LookupExistingFile",      sec + " §3.3.14", test_lookup_existing});
    r.add({"Basic.LookupNonExistent",        sec + " §3.3.14", test_lookup_noent});
    r.add({"Basic.LookupOnNonDirectory",     sec + " §3.3.14", test_lookup_notdir});
    r.add({"Basic.ReadExactSize",            sec + " §3.3.6",  test_read_exact_size});
    r.add({"Basic.ReadPastEof",              sec + " §3.3.6",  test_read_past_eof});
    r.add({"Basic.WriteFileSyncCount",       sec + " §3.3.7",  test_write_filesync_count});
    r.add({"Basic.WriteThenGetattr",         sec + " §3.3.7",  test_write_then_getattr});
    r.add({"Basic.CreateGuardedDuplicate",   sec + " §3.3.8",  test_create_guarded_duplicate});
    r.add({"Basic.CreateExclusiveIdempotent",sec + " §3.3.8",  test_create_exclusive_idempotent});
    r.add({"Basic.RemoveNonExistent",        sec + " §3.3.12", test_remove_noent});
    r.add({"Basic.RenameAcrossDirectories",  sec + " §3.3.14", test_rename_across_dirs});
}
