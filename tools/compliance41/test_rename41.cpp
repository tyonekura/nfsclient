#include "runner41.hpp"
#include "test_helpers41.hpp"
#include "nfs4/nfs4_error.hpp"

#include <string>

// ── RENAME atomicity (RFC 8881 §18.26) ───────────────────────────────────────

namespace {

void test_source_gone_after_rename(compliance41::Nfs41TestCtx& ctx) {
    Nfs4Fh src_dir = ctx.client.mkdir(ctx.workdir_fh, "r41_src");
    Nfs4Fh dst_dir = ctx.client.mkdir(ctx.workdir_fh, "r41_dst");

    Nfs4File f = ctx.client.open_write(src_dir, "r41_file.txt");
    ctx.client.close(f);

    ctx.client.rename(src_dir, "r41_file.txt", dst_dir, "r41_moved.txt");

    EXPECT_NFS41_ERR(
        ctx.client.lookup(src_dir, "r41_file.txt"),
        Nfsstat4::NFS4ERR_NOENT);

    ctx.client.remove(dst_dir, "r41_moved.txt");
    ctx.client.remove(ctx.workdir_fh, "r41_src");
    ctx.client.remove(ctx.workdir_fh, "r41_dst");
}

void test_destination_present_after_rename(compliance41::Nfs41TestCtx& ctx) {
    Nfs4Fh src_dir = ctx.client.mkdir(ctx.workdir_fh, "r41p_src");
    Nfs4Fh dst_dir = ctx.client.mkdir(ctx.workdir_fh, "r41p_dst");

    Nfs4File f = ctx.client.open_write(src_dir, "r41p_file.txt");
    ctx.client.close(f);

    ctx.client.rename(src_dir, "r41p_file.txt", dst_dir, "r41p_moved.txt");

    Nfs4Fh moved = ctx.client.lookup(dst_dir, "r41p_moved.txt");
    Fattr4 attrs = ctx.client.getattr(moved);
    CHECK41(attrs.type.has_value() && *attrs.type == Ftype4::NF4REG);

    ctx.client.remove(dst_dir, "r41p_moved.txt");
    ctx.client.remove(ctx.workdir_fh, "r41p_src");
    ctx.client.remove(ctx.workdir_fh, "r41p_dst");
}

void test_change_advances_on_both_dirs(compliance41::Nfs41TestCtx& ctx) {
    Nfs4Fh src_dir = ctx.client.mkdir(ctx.workdir_fh, "rc41_src");
    Nfs4Fh dst_dir = ctx.client.mkdir(ctx.workdir_fh, "rc41_dst");

    Nfs4File f = ctx.client.open_write(src_dir, "rc41_file.txt");
    ctx.client.close(f);

    Fattr4 src_before = ctx.client.getattr(src_dir);
    Fattr4 dst_before = ctx.client.getattr(dst_dir);
    CHECK41(src_before.change.has_value() && dst_before.change.has_value());

    ctx.client.rename(src_dir, "rc41_file.txt", dst_dir, "rc41_moved.txt");

    Fattr4 src_after = ctx.client.getattr(src_dir);
    Fattr4 dst_after = ctx.client.getattr(dst_dir);
    CHECK41(src_after.change.has_value() && *src_after.change > *src_before.change);
    CHECK41(dst_after.change.has_value() && *dst_after.change > *dst_before.change);

    ctx.client.remove(dst_dir, "rc41_moved.txt");
    ctx.client.remove(ctx.workdir_fh, "rc41_src");
    ctx.client.remove(ctx.workdir_fh, "rc41_dst");
}

}  // anonymous namespace

void register_rename41_tests(compliance41::TestRunner41& r) {
    using compliance41::ComplianceTest41;
    const std::string sec = "RFC 8881 §18.26";

    r.add({"Rename41.SourceGoneAfterRename",         sec, test_source_gone_after_rename});
    r.add({"Rename41.DestinationPresentAfterRename", sec, test_destination_present_after_rename});
    r.add({"Rename41.ChangeAdvancesOnBothDirs",      sec, test_change_advances_on_both_dirs});
}
