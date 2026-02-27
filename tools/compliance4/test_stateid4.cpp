#include "runner4.hpp"
#include "test_helpers4.hpp"
#include "nfs4/nfs4_attr.hpp"

#include <string>

// ── Section 5.3: Open/close stateid lifecycle ─────────────────────────────────

namespace {

void test_open_read_close_persists(compliance4::Nfs4TestCtx& ctx) {
    const std::string payload = "persistence check payload";

    Nfs4File wf = ctx.client.open_write(ctx.workdir_fh, "s4_persist.txt");
    ctx.client.write(wf, 0, Stable4::FILE_SYNC,
                     reinterpret_cast<const uint8_t*>(payload.data()),
                     static_cast<uint32_t>(payload.size()));
    ctx.client.close(wf);

    Nfs4File rf = ctx.client.open_read(ctx.workdir_fh, "s4_persist.txt");
    auto data = ctx.client.read(rf, 0, static_cast<uint32_t>(payload.size()));
    ctx.client.close(rf);

    CHECK4(std::string(data.begin(), data.end()) == payload);
    ctx.client.remove(ctx.workdir_fh, "s4_persist.txt");
}

void test_write_multiple_chunks(compliance4::Nfs4TestCtx& ctx) {
    const std::string part1 = "AAAA";
    const std::string part2 = "BBBB";

    Nfs4File wf = ctx.client.open_write(ctx.workdir_fh, "s4_chunks.txt");
    ctx.client.write(wf, 0, Stable4::FILE_SYNC,
                     reinterpret_cast<const uint8_t*>(part1.data()),
                     static_cast<uint32_t>(part1.size()));
    ctx.client.write(wf, static_cast<uint64_t>(part1.size()), Stable4::FILE_SYNC,
                     reinterpret_cast<const uint8_t*>(part2.data()),
                     static_cast<uint32_t>(part2.size()));
    ctx.client.close(wf);

    Nfs4File rf = ctx.client.open_read(ctx.workdir_fh, "s4_chunks.txt");
    auto data = ctx.client.read(rf, 0, 8);
    ctx.client.close(rf);

    CHECK4(data.size() == 8);
    CHECK4(std::string(data.begin(), data.begin() + 4) == part1);
    CHECK4(std::string(data.begin() + 4, data.end()) == part2);
    ctx.client.remove(ctx.workdir_fh, "s4_chunks.txt");
}

void test_commit(compliance4::Nfs4TestCtx& ctx) {
    const std::string payload = "commit test data";

    Nfs4File f = ctx.client.open_write(ctx.workdir_fh, "s4_commit.txt");
    ctx.client.write(f, 0, Stable4::UNSTABLE,
                     reinterpret_cast<const uint8_t*>(payload.data()),
                     static_cast<uint32_t>(payload.size()));
    // commit() must not throw on a valid open file
    ctx.client.commit(f, 0, 0);
    ctx.client.close(f);
    ctx.client.remove(ctx.workdir_fh, "s4_commit.txt");
}

void test_setattr_mode(compliance4::Nfs4TestCtx& ctx) {
    Nfs4File f = ctx.client.open_write(ctx.workdir_fh, "s4_mode.txt");
    ctx.client.close(f);

    Nfs4Fh fh = ctx.client.lookup(ctx.workdir_fh, "s4_mode.txt");

    nfs4::Sattr4 attrs;
    attrs.mode = 0644u;
    ctx.client.setattr(fh, attrs);

    Fattr4 got = ctx.client.getattr(fh);
    CHECK4(got.mode.has_value() && *got.mode == 0644u);

    ctx.client.remove(ctx.workdir_fh, "s4_mode.txt");
}

void test_access_check(compliance4::Nfs4TestCtx& ctx) {
    Nfs4File f = ctx.client.open_write(ctx.workdir_fh, "s4_access.txt");
    ctx.client.close(f);

    Nfs4Fh fh = ctx.client.lookup(ctx.workdir_fh, "s4_access.txt");

    // ACCESS4_READ=0x1 | ACCESS4_MODIFY=0x2 | ACCESS4_EXTEND=0x4
    uint32_t granted = ctx.client.access(fh, 0x7);
    // Running as uid=0 (root), at least read should be granted.
    CHECK4(granted & 0x1u);

    ctx.client.remove(ctx.workdir_fh, "s4_access.txt");
}

}  // anonymous namespace

void register_stateid4_tests(compliance4::TestRunner4& r) {
    using compliance4::ComplianceTest4;
    const std::string sec = "RFC 7530 §16.16";

    r.add({"Stateid4.OpenReadClosePersists",  sec, test_open_read_close_persists});
    r.add({"Stateid4.WriteMultipleChunks",    sec, test_write_multiple_chunks});
    r.add({"Stateid4.Commit",                 sec, test_commit});
    r.add({"Stateid4.SetattrMode",            sec, test_setattr_mode});
    r.add({"Stateid4.AccessCheck",            sec, test_access_check});
}
