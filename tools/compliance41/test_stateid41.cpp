#include "runner41.hpp"
#include "test_helpers41.hpp"
#include "nfs4/nfs4_attr.hpp"
#include "nfs4/open.hpp"

#include <string>

// ── Open/close stateid lifecycle (NFSv4.1 variant) ───────────────────────────
//
// NFSv4.1 removes OPEN_CONFIRM: the server MUST NOT set OPEN4_RESULT_CONFIRM
// in the OPEN result flags.  Our Nfs41Client verifies this and throws on
// protocol violation (which would appear as a test FAIL, not SKIP).

namespace {

void test_open_read_close_persists(compliance41::Nfs41TestCtx& ctx) {
    const std::string payload = "persistence check payload";

    Nfs4File wf = ctx.client.open_write(ctx.workdir_fh, "s41_persist.txt");
    ctx.client.write(wf, 0, Stable4::FILE_SYNC,
                     reinterpret_cast<const uint8_t*>(payload.data()),
                     static_cast<uint32_t>(payload.size()));
    ctx.client.close(wf);

    Nfs4File rf = ctx.client.open_read(ctx.workdir_fh, "s41_persist.txt");
    auto data = ctx.client.read(rf, 0, static_cast<uint32_t>(payload.size()));
    ctx.client.close(rf);

    CHECK41(std::string(data.begin(), data.end()) == payload);
    ctx.client.remove(ctx.workdir_fh, "s41_persist.txt");
}

void test_write_multiple_chunks(compliance41::Nfs41TestCtx& ctx) {
    const std::string part1 = "AAAA";
    const std::string part2 = "BBBB";

    Nfs4File wf = ctx.client.open_write(ctx.workdir_fh, "s41_chunks.txt");
    ctx.client.write(wf, 0, Stable4::FILE_SYNC,
                     reinterpret_cast<const uint8_t*>(part1.data()),
                     static_cast<uint32_t>(part1.size()));
    ctx.client.write(wf, static_cast<uint64_t>(part1.size()), Stable4::FILE_SYNC,
                     reinterpret_cast<const uint8_t*>(part2.data()),
                     static_cast<uint32_t>(part2.size()));
    ctx.client.close(wf);

    Nfs4File rf = ctx.client.open_read(ctx.workdir_fh, "s41_chunks.txt");
    auto data = ctx.client.read(rf, 0, 8);
    ctx.client.close(rf);

    CHECK41(data.size() == 8);
    CHECK41(std::string(data.begin(), data.begin() + 4) == part1);
    CHECK41(std::string(data.begin() + 4, data.end()) == part2);
    ctx.client.remove(ctx.workdir_fh, "s41_chunks.txt");
}

void test_commit(compliance41::Nfs41TestCtx& ctx) {
    const std::string payload = "commit test data";

    Nfs4File f = ctx.client.open_write(ctx.workdir_fh, "s41_commit.txt");
    ctx.client.write(f, 0, Stable4::UNSTABLE,
                     reinterpret_cast<const uint8_t*>(payload.data()),
                     static_cast<uint32_t>(payload.size()));
    ctx.client.commit(f, 0, 0);
    ctx.client.close(f);
    ctx.client.remove(ctx.workdir_fh, "s41_commit.txt");
}

void test_setattr_mode(compliance41::Nfs41TestCtx& ctx) {
    Nfs4File f = ctx.client.open_write(ctx.workdir_fh, "s41_mode.txt");
    ctx.client.close(f);

    Nfs4Fh fh = ctx.client.lookup(ctx.workdir_fh, "s41_mode.txt");

    nfs4::Sattr4 attrs;
    attrs.mode = 0644u;
    ctx.client.setattr(fh, attrs);

    Fattr4 got = ctx.client.getattr(fh);
    CHECK41(got.mode.has_value() && *got.mode == 0644u);

    ctx.client.remove(ctx.workdir_fh, "s41_mode.txt");
}

void test_access_check(compliance41::Nfs41TestCtx& ctx) {
    Nfs4File f = ctx.client.open_write(ctx.workdir_fh, "s41_access.txt");
    ctx.client.close(f);

    Nfs4Fh fh = ctx.client.lookup(ctx.workdir_fh, "s41_access.txt");

    // ACCESS4_READ=0x1 | ACCESS4_MODIFY=0x2 | ACCESS4_EXTEND=0x4
    uint32_t granted = ctx.client.access(fh, 0x7);
    // Running as uid=0 (root), at least read should be granted.
    CHECK41(granted & 0x1u);

    ctx.client.remove(ctx.workdir_fh, "s41_access.txt");
}

void test_no_open_confirm(compliance41::Nfs41TestCtx& ctx) {
    // Nfs41Client::do_open() asserts OPEN4_RESULT_CONFIRM is NOT set.
    // If we can open/close successfully, the server correctly omitted it.
    Nfs4File f = ctx.client.open_write(ctx.workdir_fh, "s41_noconfirm.txt");
    ctx.client.close(f);
    ctx.client.remove(ctx.workdir_fh, "s41_noconfirm.txt");
}

}  // anonymous namespace

void register_stateid41_tests(compliance41::TestRunner41& r) {
    using compliance41::ComplianceTest41;
    const std::string sec = "RFC 8881 §18.16";

    r.add({"Stateid41.OpenReadClosePersists",  sec, test_open_read_close_persists});
    r.add({"Stateid41.WriteMultipleChunks",    sec, test_write_multiple_chunks});
    r.add({"Stateid41.Commit",                 sec, test_commit});
    r.add({"Stateid41.SetattrMode",            sec, test_setattr_mode});
    r.add({"Stateid41.AccessCheck",            sec, test_access_check});
    r.add({"Stateid41.NoOpenConfirm",          sec, test_no_open_confirm});
}
