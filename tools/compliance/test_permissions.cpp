#include "runner.hpp"
#include "test_helpers.hpp"
#include "nfs/access.hpp"
#include "rpc/rpc_types.hpp"

#include <cstdint>
#include <ctime>
#include <string>

// ── Section 2.6: Permission / access control — RFC 1813 §3.3.4 ───────────────
//
// Tests that require a non-root perspective open a second NFSClient connected
// to the same server with uid=1001.  The root client (ctx.client) remains
// available to set up and clean up.

namespace {

// Build a non-root NFSClient connected to the same server.
// Note: the caller is responsible for the lifetime of this object.
static NFSClient make_unprivileged_client(const std::string& server) {
    NFSClient c(server);
    AuthSys auth{};
    auth.stamp       = static_cast<uint32_t>(time(nullptr));
    auth.machinename = "nfsclient-compliance";
    auth.uid         = 1001;
    auth.gid         = 1001;
    c.set_auth_sys(auth);
    return c;
}

void test_setattr_mode0000_read(compliance::TestCtx& ctx) {
    // Create a file and write some content as root.
    Fh3 fh = ctx.client.create(ctx.workdir_fh, "p_mode0000.txt");
    const std::string payload = "secret content";
    ctx.client.write(fh, 0, Stable3::FILE_SYNC,
                     reinterpret_cast<const uint8_t*>(payload.data()), payload.size());

    // Set mode to 0000 so no-one (except root) can read.
    Sattr3 s;
    s.set_mode = true;
    s.mode = 0000;
    ctx.client.setattr(fh, s);

    // A non-root client must get NFS3ERR_ACCES when reading.
    NFSClient unpriv = make_unprivileged_client(ctx.server);
    EXPECT_NFS_ERR(
        unpriv.read(fh, 0, 512),
        Nfsstat3::NFS3ERR_ACCES);

    // Restore mode and remove.
    s.mode = 0644;
    ctx.client.setattr(fh, s);
    ctx.client.remove(ctx.workdir_fh, "p_mode0000.txt");
}

void test_access_on_unreadable_file(compliance::TestCtx& ctx) {
    Fh3 fh = ctx.client.create(ctx.workdir_fh, "p_access_unread.txt");

    Sattr3 s;
    s.set_mode = true;
    s.mode = 0000;
    ctx.client.setattr(fh, s);

    NFSClient unpriv = make_unprivileged_client(ctx.server);
    uint32_t granted = unpriv.access(fh, nfs3::ACCESS3_READ);

    // No read access should be granted on a mode-0000 file for uid=1001
    CHECK((granted & nfs3::ACCESS3_READ) == 0u);

    // Restore and remove.
    s.mode = 0644;
    ctx.client.setattr(fh, s);
    ctx.client.remove(ctx.workdir_fh, "p_access_unread.txt");
}

void test_root_bypass_readonly(compliance::TestCtx& ctx) {
    Fh3 fh = ctx.client.create(ctx.workdir_fh, "p_root_bypass.txt");
    const std::string payload = "root bypass test";
    ctx.client.write(fh, 0, Stable3::FILE_SYNC,
                     reinterpret_cast<const uint8_t*>(payload.data()), payload.size());

    // Set mode 0000 so no ordinary user can access it.
    Sattr3 s;
    s.set_mode = true;
    s.mode = 0000;
    ctx.client.setattr(fh, s);

    // Root (uid=0) should still be able to read it.
    auto data = ctx.client.read(fh, 0, static_cast<uint32_t>(payload.size()));
    CHECK(data.size() == payload.size());

    // Restore and remove.
    s.mode = 0644;
    ctx.client.setattr(fh, s);
    ctx.client.remove(ctx.workdir_fh, "p_root_bypass.txt");
}

}  // anonymous namespace

void register_permission_tests(compliance::TestRunner& r) {
    using compliance::ComplianceTest;
    const std::string sec = "RFC 1813 §3.3.4";

    r.add({"Permission.SetattrMode0000Read",    sec, test_setattr_mode0000_read});
    r.add({"Permission.AccessOnUnreadableFile", sec, test_access_on_unreadable_file});
    r.add({"Permission.RootBypassReadOnly",     sec, test_root_bypass_readonly});
}
