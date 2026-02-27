#include "runner4.hpp"
#include "test_helpers4.hpp"

#include <string>

// ── Section 5.4: fattr4 mandatory attributes ──────────────────────────────────

namespace {

void test_type_regular_file(compliance4::Nfs4TestCtx& ctx) {
    Nfs4File f = ctx.client.open_write(ctx.workdir_fh, "a4_type_reg.txt");
    ctx.client.close(f);

    Nfs4Fh fh = ctx.client.lookup(ctx.workdir_fh, "a4_type_reg.txt");
    Fattr4 attrs = ctx.client.getattr(fh);
    CHECK4(attrs.type.has_value() && *attrs.type == Ftype4::NF4REG);

    ctx.client.remove(ctx.workdir_fh, "a4_type_reg.txt");
}

void test_type_directory(compliance4::Nfs4TestCtx& ctx) {
    Nfs4Fh dir = ctx.client.mkdir(ctx.workdir_fh, "a4_type_dir");
    Fattr4 attrs = ctx.client.getattr(dir);
    CHECK4(attrs.type.has_value() && *attrs.type == Ftype4::NF4DIR);
    ctx.client.remove(ctx.workdir_fh, "a4_type_dir");
}

void test_size_after_write(compliance4::Nfs4TestCtx& ctx) {
    const std::string payload = "size test payload string";

    Nfs4File f = ctx.client.open_write(ctx.workdir_fh, "a4_size.txt");
    ctx.client.write(f, 0, Stable4::FILE_SYNC,
                     reinterpret_cast<const uint8_t*>(payload.data()),
                     static_cast<uint32_t>(payload.size()));
    ctx.client.close(f);

    Nfs4Fh fh = ctx.client.lookup(ctx.workdir_fh, "a4_size.txt");
    Fattr4 attrs = ctx.client.getattr(fh);
    CHECK4(attrs.size.has_value() && *attrs.size == payload.size());

    ctx.client.remove(ctx.workdir_fh, "a4_size.txt");
}

void test_change_advances_after_write(compliance4::Nfs4TestCtx& ctx) {
    // Create file and capture initial change attr.
    Nfs4File f1 = ctx.client.open_write(ctx.workdir_fh, "a4_change.txt");
    const std::string payload1 = "initial content";
    ctx.client.write(f1, 0, Stable4::FILE_SYNC,
                     reinterpret_cast<const uint8_t*>(payload1.data()),
                     static_cast<uint32_t>(payload1.size()));
    ctx.client.close(f1);

    Nfs4Fh fh = ctx.client.lookup(ctx.workdir_fh, "a4_change.txt");
    Fattr4 before = ctx.client.getattr(fh);
    CHECK4(before.change.has_value());

    // Overwrite the file (UNCHECKED4 truncates+rewrites → advances change).
    Nfs4File f2 = ctx.client.open_write(ctx.workdir_fh, "a4_change.txt");
    const std::string payload2 = "updated content that is longer than before";
    ctx.client.write(f2, 0, Stable4::FILE_SYNC,
                     reinterpret_cast<const uint8_t*>(payload2.data()),
                     static_cast<uint32_t>(payload2.size()));
    ctx.client.close(f2);

    Fattr4 after = ctx.client.getattr(fh);
    CHECK4(after.change.has_value() && *after.change > *before.change);

    ctx.client.remove(ctx.workdir_fh, "a4_change.txt");
}

void test_time_modify_advances_after_write(compliance4::Nfs4TestCtx& ctx) {
    const std::string payload1 = "first write";

    Nfs4File f1 = ctx.client.open_write(ctx.workdir_fh, "a4_mtime.txt");
    ctx.client.write(f1, 0, Stable4::FILE_SYNC,
                     reinterpret_cast<const uint8_t*>(payload1.data()),
                     static_cast<uint32_t>(payload1.size()));
    ctx.client.close(f1);

    Nfs4Fh fh = ctx.client.lookup(ctx.workdir_fh, "a4_mtime.txt");
    Fattr4 before = ctx.client.getattr(fh);
    CHECK4(before.time_modify.has_value());

    Nfs4File f2 = ctx.client.open_write(ctx.workdir_fh, "a4_mtime.txt");
    const std::string payload2 = "second write with more data here";
    ctx.client.write(f2, 0, Stable4::FILE_SYNC,
                     reinterpret_cast<const uint8_t*>(payload2.data()),
                     static_cast<uint32_t>(payload2.size()));
    ctx.client.close(f2);

    Fattr4 after = ctx.client.getattr(fh);
    CHECK4(after.time_modify.has_value());
    // time_modify must not go backwards (may be equal if writes are sub-second apart)
    CHECK4(after.time_modify->seconds >= before.time_modify->seconds);

    ctx.client.remove(ctx.workdir_fh, "a4_mtime.txt");
}

void test_owner_non_empty(compliance4::Nfs4TestCtx& ctx) {
    Nfs4File f = ctx.client.open_write(ctx.workdir_fh, "a4_owner.txt");
    ctx.client.close(f);

    Nfs4Fh fh = ctx.client.lookup(ctx.workdir_fh, "a4_owner.txt");
    Fattr4 attrs = ctx.client.getattr(fh);
    CHECK4(attrs.owner.has_value() && !attrs.owner->empty());

    ctx.client.remove(ctx.workdir_fh, "a4_owner.txt");
}

void test_owner_group_non_empty(compliance4::Nfs4TestCtx& ctx) {
    Nfs4File f = ctx.client.open_write(ctx.workdir_fh, "a4_group.txt");
    ctx.client.close(f);

    Nfs4Fh fh = ctx.client.lookup(ctx.workdir_fh, "a4_group.txt");
    Fattr4 attrs = ctx.client.getattr(fh);
    CHECK4(attrs.owner_group.has_value() && !attrs.owner_group->empty());

    ctx.client.remove(ctx.workdir_fh, "a4_group.txt");
}

}  // anonymous namespace

void register_attr4_tests(compliance4::TestRunner4& r) {
    using compliance4::ComplianceTest4;
    const std::string sec = "RFC 7530 §5.8";

    r.add({"Attr4.TypeRegularFile",           sec, test_type_regular_file});
    r.add({"Attr4.TypeDirectory",             sec, test_type_directory});
    r.add({"Attr4.SizeAfterWrite",            sec, test_size_after_write});
    r.add({"Attr4.ChangeAdvancesAfterWrite",  sec, test_change_advances_after_write});
    r.add({"Attr4.TimeModifyAfterWrite",      sec, test_time_modify_advances_after_write});
    r.add({"Attr4.OwnerNonEmpty",             sec, test_owner_non_empty});
    r.add({"Attr4.OwnerGroupNonEmpty",        sec, test_owner_group_non_empty});
}
