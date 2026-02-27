#include "runner4.hpp"
#include "test_helpers4.hpp"
#include "nfs4/nfs4_error.hpp"

#include <string>

// ── Section 5.7: RENAME two-directory atomicity ───────────────────────────────

namespace {

void test_source_gone_after_rename(compliance4::Nfs4TestCtx& ctx) {
    Nfs4Fh src_dir = ctx.client.mkdir(ctx.workdir_fh, "r4_src");
    Nfs4Fh dst_dir = ctx.client.mkdir(ctx.workdir_fh, "r4_dst");

    Nfs4File f = ctx.client.open_write(src_dir, "r4_file.txt");
    ctx.client.close(f);

    ctx.client.rename(src_dir, "r4_file.txt", dst_dir, "r4_moved.txt");

    EXPECT_NFS4_ERR(
        ctx.client.lookup(src_dir, "r4_file.txt"),
        Nfsstat4::NFS4ERR_NOENT);

    ctx.client.remove(dst_dir, "r4_moved.txt");
    ctx.client.remove(ctx.workdir_fh, "r4_src");
    ctx.client.remove(ctx.workdir_fh, "r4_dst");
}

void test_destination_present_after_rename(compliance4::Nfs4TestCtx& ctx) {
    Nfs4Fh src_dir = ctx.client.mkdir(ctx.workdir_fh, "r4p_src");
    Nfs4Fh dst_dir = ctx.client.mkdir(ctx.workdir_fh, "r4p_dst");

    Nfs4File f = ctx.client.open_write(src_dir, "r4p_file.txt");
    ctx.client.close(f);

    ctx.client.rename(src_dir, "r4p_file.txt", dst_dir, "r4p_moved.txt");

    Nfs4Fh moved = ctx.client.lookup(dst_dir, "r4p_moved.txt");
    Fattr4 attrs = ctx.client.getattr(moved);
    CHECK4(attrs.type.has_value() && *attrs.type == Ftype4::NF4REG);

    ctx.client.remove(dst_dir, "r4p_moved.txt");
    ctx.client.remove(ctx.workdir_fh, "r4p_src");
    ctx.client.remove(ctx.workdir_fh, "r4p_dst");
}

void test_change_advances_on_both_dirs(compliance4::Nfs4TestCtx& ctx) {
    Nfs4Fh src_dir = ctx.client.mkdir(ctx.workdir_fh, "rc4_src");
    Nfs4Fh dst_dir = ctx.client.mkdir(ctx.workdir_fh, "rc4_dst");

    Nfs4File f = ctx.client.open_write(src_dir, "rc4_file.txt");
    ctx.client.close(f);

    Fattr4 src_before = ctx.client.getattr(src_dir);
    Fattr4 dst_before = ctx.client.getattr(dst_dir);
    CHECK4(src_before.change.has_value() && dst_before.change.has_value());

    ctx.client.rename(src_dir, "rc4_file.txt", dst_dir, "rc4_moved.txt");

    Fattr4 src_after = ctx.client.getattr(src_dir);
    Fattr4 dst_after = ctx.client.getattr(dst_dir);
    CHECK4(src_after.change.has_value() && *src_after.change > *src_before.change);
    CHECK4(dst_after.change.has_value() && *dst_after.change > *dst_before.change);

    ctx.client.remove(dst_dir, "rc4_moved.txt");
    ctx.client.remove(ctx.workdir_fh, "rc4_src");
    ctx.client.remove(ctx.workdir_fh, "rc4_dst");
}

}  // anonymous namespace

void register_rename4_tests(compliance4::TestRunner4& r) {
    using compliance4::ComplianceTest4;
    const std::string sec = "RFC 7530 §16.24";

    r.add({"Rename4.SourceGoneAfterRename",      sec, test_source_gone_after_rename});
    r.add({"Rename4.DestinationPresentAfterRename", sec, test_destination_present_after_rename});
    r.add({"Rename4.ChangeAdvancesOnBothDirs",   sec, test_change_advances_on_both_dirs});
}
