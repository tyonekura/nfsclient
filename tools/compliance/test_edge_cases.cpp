#include "runner.hpp"
#include "test_helpers.hpp"
#include "nfs/fsinfo.hpp"
#include "nfs/readdir.hpp"

#include <array>
#include <cstdint>
#include <string>

// ── Section 2.8: Edge cases ───────────────────────────────────────────────────

namespace {

void test_zero_byte_read(compliance::TestCtx& ctx) {
    Fh3 fh = ctx.client.create(ctx.workdir_fh, "e_zero_read.txt");
    const std::string payload = "some data";
    ctx.client.write(fh, 0, Stable3::FILE_SYNC,
                     reinterpret_cast<const uint8_t*>(payload.data()), payload.size());

    // Requesting 0 bytes must succeed and return an empty buffer.
    auto data = ctx.client.read(fh, 0, 0);
    CHECK(data.empty());

    ctx.client.remove(ctx.workdir_fh, "e_zero_read.txt");
}

void test_zero_byte_write(compliance::TestCtx& ctx) {
    Fh3 fh = ctx.client.create(ctx.workdir_fh, "e_zero_write.txt");

    WriteResult r = ctx.client.write(fh, 0, Stable3::FILE_SYNC, nullptr, 0);
    CHECK(r.count == 0u);

    ctx.client.remove(ctx.workdir_fh, "e_zero_write.txt");
}

void test_max_filename_255(compliance::TestCtx& ctx) {
    // NFS3ERR_NAMETOOLONG applies to names > NAME_MAX (typically 255).
    // A 255-character name must succeed.
    std::string name(255, 'x');
    Fh3 fh = ctx.client.create(ctx.workdir_fh, name);
    Fattr3 attrs = ctx.client.getattr(fh);
    CHECK(attrs.type == Ftype3::NF3REG);
    ctx.client.remove(ctx.workdir_fh, name);
}

void test_filename_256_nametoolong(compliance::TestCtx& ctx) {
    std::string name(256, 'y');
    EXPECT_NFS_ERR(
        ctx.client.create(ctx.workdir_fh, name),
        Nfsstat3::NFS3ERR_NAMETOOLONG);
}

void test_read_count_gt_rtmax(compliance::TestCtx& ctx) {
    // Get server's rtmax (and wtmax for safe chunked writes).
    nfs3::FsinfoResult info = ctx.client.fsinfo(ctx.root_fh);
    uint32_t rtmax = info.rtmax;
    uint32_t chunk = std::min(info.wtmax, 65536u);

    // Create a large file using wtmax-bounded chunks so we stay within the
    // server's per-request write limit.  The file must be > rtmax bytes so
    // the subsequent read truly exercises the over-rtmax path.
    Fh3 fh = ctx.client.create(ctx.workdir_fh, "e_rtmax.txt");
    uint32_t file_size = rtmax + 4096u;
    std::vector<uint8_t> buf(chunk, 0xAB);
    uint64_t written = 0;
    while (written < file_size) {
        uint32_t n = static_cast<uint32_t>(
            std::min<uint64_t>(chunk, file_size - written));
        ctx.client.write(fh, written, Stable3::FILE_SYNC, buf.data(), n);
        written += n;
    }

    // Read more than rtmax — server must either clip or honour the request.
    // Either way it must NOT error; it returns count <= requested.
    auto data = ctx.client.read(fh, 0, file_size);
    CHECK(!data.empty());
    CHECK(data.size() <= file_size);

    ctx.client.remove(ctx.workdir_fh, "e_rtmax.txt");
}

void test_readdir_bad_cookieverf(compliance::TestCtx& ctx) {
    // Create a few entries so the directory is non-trivial.
    ctx.client.create(ctx.workdir_fh, "e_rc_file1.txt");
    ctx.client.create(ctx.workdir_fh, "e_rc_file2.txt");

    // First call: cookie=0, zero cookieverf — get the real cookieverf.
    std::array<uint8_t, 8> zero_cv{};
    nfs3::ReaddirPage page1 =
        ctx.client.readdir_page(ctx.workdir_fh, 0, zero_cv, 512);

    // Second call: non-zero cookie (claim we've already paged) + WRONG cookieverf.
    // The server should return NFS3ERR_BAD_COOKIE.
    std::array<uint8_t, 8> bad_cv = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    EXPECT_NFS_ERR(
        ctx.client.readdir_page(ctx.workdir_fh, 1 /* non-zero cookie */, bad_cv, 512),
        Nfsstat3::NFS3ERR_BAD_COOKIE);

    ctx.client.remove(ctx.workdir_fh, "e_rc_file1.txt");
    ctx.client.remove(ctx.workdir_fh, "e_rc_file2.txt");
}

}  // anonymous namespace

void register_edge_case_tests(compliance::TestRunner& r) {
    using compliance::ComplianceTest;

    r.add({"EdgeCase.ZeroByteRead",           "RFC 1813 §3.3.6",  test_zero_byte_read});
    r.add({"EdgeCase.ZeroByteWrite",          "RFC 1813 §3.3.7",  test_zero_byte_write});
    r.add({"EdgeCase.MaxFilename255",         "RFC 1813 §2.5",    test_max_filename_255});
    r.add({"EdgeCase.Filename256NameTooLong", "RFC 1813 §2.5",    test_filename_256_nametoolong});
    r.add({"EdgeCase.ReadCountGtRtmax",       "RFC 1813 §3.3.19", test_read_count_gt_rtmax});
    r.add({"EdgeCase.ReaddirBadCookieverf",   "RFC 1813 §3.3.16", test_readdir_bad_cookieverf});
}
