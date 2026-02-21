#include "runner.hpp"
#include "test_helpers.hpp"

#include <cstdint>
#include <string>

// ── Section 2.5: Attribute accuracy — RFC 1813 §2.5 ──────────────────────────

namespace {

// Helper: combine seconds+nseconds into a single comparable value
static uint64_t nfstime_ns(const Nfstime3& t) {
    return static_cast<uint64_t>(t.seconds) * 1'000'000'000ULL + t.nseconds;
}

void test_mtime_after_write(compliance::TestCtx& ctx) {
    Fh3 fh = ctx.client.create(ctx.workdir_fh, "a_mtime.txt");
    Fattr3 before = ctx.client.getattr(fh);

    const std::string payload = "mtime test data";
    ctx.client.write(fh, 0, Stable3::FILE_SYNC,
                     reinterpret_cast<const uint8_t*>(payload.data()), payload.size());

    Fattr3 after = ctx.client.getattr(fh);

    // mtime must not go backward after a write
    CHECK(nfstime_ns(after.mtime) >= nfstime_ns(before.mtime));

    ctx.client.remove(ctx.workdir_fh, "a_mtime.txt");
}

void test_ctime_after_setattr(compliance::TestCtx& ctx) {
    Fh3 fh = ctx.client.create(ctx.workdir_fh, "a_ctime.txt");
    Fattr3 before = ctx.client.getattr(fh);

    // Any metadata change must advance ctime
    Sattr3 s;
    s.set_mode = true;
    s.mode = 0600;
    ctx.client.setattr(fh, s);

    Fattr3 after = ctx.client.getattr(fh);
    CHECK(nfstime_ns(after.ctime) >= nfstime_ns(before.ctime));

    ctx.client.remove(ctx.workdir_fh, "a_ctime.txt");
}

void test_nlink_after_link(compliance::TestCtx& ctx) {
    Fh3 fh = ctx.client.create(ctx.workdir_fh, "a_nlink_src.txt");
    Fattr3 before = ctx.client.getattr(fh);

    ctx.client.link(fh, ctx.workdir_fh, "a_nlink_hardlink.txt");

    Fattr3 after = ctx.client.getattr(fh);
    CHECK(after.nlink == before.nlink + 1);

    ctx.client.remove(ctx.workdir_fh, "a_nlink_hardlink.txt");
    ctx.client.remove(ctx.workdir_fh, "a_nlink_src.txt");
}

void test_nlink_after_remove(compliance::TestCtx& ctx) {
    Fh3 fh = ctx.client.create(ctx.workdir_fh, "a_nlink_rem.txt");
    ctx.client.link(fh, ctx.workdir_fh, "a_nlink_rem2.txt");

    Fattr3 before = ctx.client.getattr(fh);
    CHECK(before.nlink >= 2u);

    ctx.client.remove(ctx.workdir_fh, "a_nlink_rem2.txt");
    Fattr3 after = ctx.client.getattr(fh);
    CHECK(after.nlink == before.nlink - 1);

    ctx.client.remove(ctx.workdir_fh, "a_nlink_rem.txt");
}

void test_size_after_truncate(compliance::TestCtx& ctx) {
    Fh3 fh = ctx.client.create(ctx.workdir_fh, "a_truncate.txt");
    const std::string payload = "data to be truncated";
    ctx.client.write(fh, 0, Stable3::FILE_SYNC,
                     reinterpret_cast<const uint8_t*>(payload.data()), payload.size());

    Sattr3 s;
    s.set_size = true;
    s.size = 0;
    ctx.client.setattr(fh, s);

    Fattr3 attrs = ctx.client.getattr(fh);
    CHECK(attrs.size == 0u);

    ctx.client.remove(ctx.workdir_fh, "a_truncate.txt");
}

}  // anonymous namespace

void register_attribute_tests(compliance::TestRunner& r) {
    using compliance::ComplianceTest;
    const std::string sec = "RFC 1813 §2.5";

    r.add({"Attributes.MtimeAfterWrite",   sec, test_mtime_after_write});
    r.add({"Attributes.CtimeAfterSetattr", sec, test_ctime_after_setattr});
    r.add({"Attributes.NlinkAfterLink",    sec, test_nlink_after_link});
    r.add({"Attributes.NlinkAfterRemove",  sec, test_nlink_after_remove});
    r.add({"Attributes.SizeAfterTruncate", sec, test_size_after_truncate});
}
