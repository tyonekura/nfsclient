#include "runner41.hpp"
#include "test_helpers41.hpp"
#include "nfs4/nfs4_error.hpp"

#include <algorithm>
#include <string>
#include <vector>

// ── Basic NFSv4.1 file operations ─────────────────────────────────────────────

namespace {

void test_lookup_existing(compliance41::Nfs41TestCtx& ctx) {
    Nfs4File f = ctx.client.open_write(ctx.workdir_fh, "b41_lookup.txt");
    ctx.client.close(f);

    Nfs4Fh fh = ctx.client.lookup(ctx.workdir_fh, "b41_lookup.txt");
    Fattr4 attrs = ctx.client.getattr(fh);
    CHECK41(attrs.type.has_value() && *attrs.type == Ftype4::NF4REG);

    ctx.client.remove(ctx.workdir_fh, "b41_lookup.txt");
}

void test_lookup_noent(compliance41::Nfs41TestCtx& ctx) {
    EXPECT_NFS41_ERR(
        ctx.client.lookup(ctx.workdir_fh, "b41_no_such_file_xyz"),
        Nfsstat4::NFS4ERR_NOENT);
}

void test_read_write_roundtrip(compliance41::Nfs41TestCtx& ctx) {
    const std::string payload = "Hello, NFSv4.1 compliance!";

    Nfs4File wf = ctx.client.open_write(ctx.workdir_fh, "b41_rw.txt");
    ctx.client.write(wf, 0, Stable4::FILE_SYNC,
                     reinterpret_cast<const uint8_t*>(payload.data()),
                     static_cast<uint32_t>(payload.size()));
    ctx.client.close(wf);

    Nfs4File rf = ctx.client.open_read(ctx.workdir_fh, "b41_rw.txt");
    auto data = ctx.client.read(rf, 0, static_cast<uint32_t>(payload.size()));
    ctx.client.close(rf);

    CHECK41(std::string(data.begin(), data.end()) == payload);
    ctx.client.remove(ctx.workdir_fh, "b41_rw.txt");
}

void test_read_past_eof(compliance41::Nfs41TestCtx& ctx) {
    const std::string payload = "short";

    Nfs4File wf = ctx.client.open_write(ctx.workdir_fh, "b41_eof.txt");
    ctx.client.write(wf, 0, Stable4::FILE_SYNC,
                     reinterpret_cast<const uint8_t*>(payload.data()),
                     static_cast<uint32_t>(payload.size()));
    ctx.client.close(wf);

    Nfs4File rf = ctx.client.open_read(ctx.workdir_fh, "b41_eof.txt");
    auto data = ctx.client.read(rf, 1000, 512);
    ctx.client.close(rf);

    CHECK41(data.empty());
    ctx.client.remove(ctx.workdir_fh, "b41_eof.txt");
}

void test_mkdir_and_remove(compliance41::Nfs41TestCtx& ctx) {
    Nfs4Fh dir = ctx.client.mkdir(ctx.workdir_fh, "b41_dir");
    Fattr4 attrs = ctx.client.getattr(dir);
    CHECK41(attrs.type.has_value() && *attrs.type == Ftype4::NF4DIR);
    ctx.client.remove(ctx.workdir_fh, "b41_dir");
}

void test_readdir(compliance41::Nfs41TestCtx& ctx) {
    Nfs4Fh dir = ctx.client.mkdir(ctx.workdir_fh, "b41_readdir_dir");

    Nfs4File f1 = ctx.client.open_write(dir, "file1.txt");
    ctx.client.close(f1);
    Nfs4File f2 = ctx.client.open_write(dir, "file2.txt");
    ctx.client.close(f2);

    auto entries = ctx.client.readdir(dir);

    bool found1 = false, found2 = false;
    for (const auto& e : entries) {
        if (e.name == "file1.txt") found1 = true;
        if (e.name == "file2.txt") found2 = true;
    }
    CHECK41(found1);
    CHECK41(found2);

    ctx.client.remove(dir, "file1.txt");
    ctx.client.remove(dir, "file2.txt");
    ctx.client.remove(ctx.workdir_fh, "b41_readdir_dir");
}

}  // anonymous namespace

void register_basic41_tests(compliance41::TestRunner41& r) {
    using compliance41::ComplianceTest41;
    const std::string sec = "RFC 8881";

    r.add({"Basic41.LookupExistingFile",  sec + " §18.15", test_lookup_existing});
    r.add({"Basic41.LookupNonExistent",   sec + " §18.15", test_lookup_noent});
    r.add({"Basic41.ReadWriteRoundtrip",  sec + " §18.22", test_read_write_roundtrip});
    r.add({"Basic41.ReadPastEof",         sec + " §18.22", test_read_past_eof});
    r.add({"Basic41.MkdirAndRemove",      sec + " §18.6",  test_mkdir_and_remove});
    r.add({"Basic41.Readdir",             sec + " §18.23", test_readdir});
}
