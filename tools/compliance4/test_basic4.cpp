#include "runner4.hpp"
#include "test_helpers4.hpp"
#include "nfs4/nfs4_error.hpp"

#include <algorithm>
#include <string>
#include <vector>

// ── Section 5.1 / 5.3: Basic operations ──────────────────────────────────────

namespace {

void test_lookup_existing(compliance4::Nfs4TestCtx& ctx) {
    Nfs4File f = ctx.client.open_write(ctx.workdir_fh, "b4_lookup.txt");
    ctx.client.close(f);

    Nfs4Fh fh = ctx.client.lookup(ctx.workdir_fh, "b4_lookup.txt");
    Fattr4 attrs = ctx.client.getattr(fh);
    CHECK4(attrs.type.has_value() && *attrs.type == Ftype4::NF4REG);

    ctx.client.remove(ctx.workdir_fh, "b4_lookup.txt");
}

void test_lookup_noent(compliance4::Nfs4TestCtx& ctx) {
    EXPECT_NFS4_ERR(
        ctx.client.lookup(ctx.workdir_fh, "b4_no_such_file_xyz"),
        Nfsstat4::NFS4ERR_NOENT);
}

void test_read_write_roundtrip(compliance4::Nfs4TestCtx& ctx) {
    const std::string payload = "Hello, NFSv4 compliance!";

    Nfs4File wf = ctx.client.open_write(ctx.workdir_fh, "b4_rw.txt");
    ctx.client.write(wf, 0, Stable4::FILE_SYNC,
                     reinterpret_cast<const uint8_t*>(payload.data()),
                     static_cast<uint32_t>(payload.size()));
    ctx.client.close(wf);

    Nfs4File rf = ctx.client.open_read(ctx.workdir_fh, "b4_rw.txt");
    auto data = ctx.client.read(rf, 0, static_cast<uint32_t>(payload.size()));
    ctx.client.close(rf);

    CHECK4(std::string(data.begin(), data.end()) == payload);
    ctx.client.remove(ctx.workdir_fh, "b4_rw.txt");
}

void test_read_past_eof(compliance4::Nfs4TestCtx& ctx) {
    const std::string payload = "short";

    Nfs4File wf = ctx.client.open_write(ctx.workdir_fh, "b4_eof.txt");
    ctx.client.write(wf, 0, Stable4::FILE_SYNC,
                     reinterpret_cast<const uint8_t*>(payload.data()),
                     static_cast<uint32_t>(payload.size()));
    ctx.client.close(wf);

    Nfs4File rf = ctx.client.open_read(ctx.workdir_fh, "b4_eof.txt");
    auto data = ctx.client.read(rf, 1000, 512);
    ctx.client.close(rf);

    CHECK4(data.empty());
    ctx.client.remove(ctx.workdir_fh, "b4_eof.txt");
}

void test_mkdir_and_remove(compliance4::Nfs4TestCtx& ctx) {
    Nfs4Fh dir = ctx.client.mkdir(ctx.workdir_fh, "b4_dir");
    Fattr4 attrs = ctx.client.getattr(dir);
    CHECK4(attrs.type.has_value() && *attrs.type == Ftype4::NF4DIR);
    ctx.client.remove(ctx.workdir_fh, "b4_dir");
}

void test_readdir(compliance4::Nfs4TestCtx& ctx) {
    Nfs4Fh dir = ctx.client.mkdir(ctx.workdir_fh, "b4_readdir_dir");

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
    CHECK4(found1);
    CHECK4(found2);

    ctx.client.remove(dir, "file1.txt");
    ctx.client.remove(dir, "file2.txt");
    ctx.client.remove(ctx.workdir_fh, "b4_readdir_dir");
}

}  // anonymous namespace

void register_basic4_tests(compliance4::TestRunner4& r) {
    using compliance4::ComplianceTest4;
    const std::string sec = "RFC 7530";

    r.add({"Basic4.LookupExistingFile",  sec + " §16.15", test_lookup_existing});
    r.add({"Basic4.LookupNonExistent",   sec + " §16.15", test_lookup_noent});
    r.add({"Basic4.ReadWriteRoundtrip",  sec + " §18.22", test_read_write_roundtrip});
    r.add({"Basic4.ReadPastEof",         sec + " §18.22", test_read_past_eof});
    r.add({"Basic4.MkdirAndRemove",      sec + " §18.6",  test_mkdir_and_remove});
    r.add({"Basic4.Readdir",             sec + " §18.23", test_readdir});
}
