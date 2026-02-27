#include "runner41.hpp"
#include "test_helpers41.hpp"
#include "nfs4/nfs4_error.hpp"

#include <algorithm>
#include <string>

// ── Session establishment and SEQUENCE tests ──────────────────────────────────

namespace {

void test_exchange_id(compliance41::Nfs41TestCtx& ctx) {
    // If we got here, EXCHANGE_ID succeeded.  Verify clientid is non-zero.
    CHECK41(ctx.client.client_id() != 0);
}

void test_create_session(compliance41::Nfs41TestCtx& ctx) {
    // Session ID is 16 bytes; at least one must be non-zero for a real session.
    const auto& sid = ctx.client.session_id();
    bool has_nonzero = std::any_of(sid.begin(), sid.end(),
                                    [](uint8_t b) { return b != 0; });
    CHECK41(has_nonzero);
}

void test_sequence_works(compliance41::Nfs41TestCtx& ctx) {
    // A normal GETATTR after session setup proves SEQUENCE is accepted by the server.
    Fattr4 attrs = ctx.client.getattr(ctx.root_fh);
    CHECK41(attrs.type.has_value());
}

void test_reclaim_complete(compliance41::Nfs41TestCtx& ctx) {
    // RECLAIM_COMPLETE is called during construction.  If the constructor
    // succeeded and we can open a file without NFS4ERR_GRACE, it worked.
    Nfs4File f = ctx.client.open_write(ctx.workdir_fh, "sess41_rc.txt");
    ctx.client.close(f);
    ctx.client.remove(ctx.workdir_fh, "sess41_rc.txt");
}

}  // anonymous namespace

void register_session41_tests(compliance41::TestRunner41& r) {
    using compliance41::ComplianceTest41;
    const std::string sec = "RFC 8881";

    r.add({"Session41.ExchangeId",      sec + " §18.35", test_exchange_id});
    r.add({"Session41.CreateSession",   sec + " §18.36", test_create_session});
    r.add({"Session41.SequenceWorks",   sec + " §18.46", test_sequence_works});
    r.add({"Session41.ReclaimComplete", sec + " §18.51", test_reclaim_complete});
}
