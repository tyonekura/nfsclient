#pragma once

#include "nfs4_client.hpp"

#include <functional>
#include <string>
#include <vector>

namespace compliance4 {

// Thrown by CHECK4 / EXPECT_NFS4_ERR when an assertion fails.
struct ComplianceFailure4 : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// Context passed to every compliance test.
// client is already connected and has its root_fh set.
// workdir_fh is a per-run scratch directory the test may freely use.
struct Nfs4TestCtx {
    Nfs4Client&  client;
    Nfs4Fh       root_fh;
    Nfs4Fh       workdir_fh;
    std::string  server;
    std::string  export_path;
};

struct ComplianceTest4 {
    std::string                         name;     // e.g. "Basic4.LookupExistingFile"
    std::string                         rfc_ref;  // e.g. "RFC 7530 §16.16"
    std::function<void(Nfs4TestCtx&)>   fn;
};

class TestRunner4 {
public:
    void add(ComplianceTest4 t);

    // Run all registered tests against ctx.
    // Only runs tests whose name contains `filter` (empty = run all).
    // Prints per-test status to stdout and a summary at the end.
    // Returns number of FAIL results.
    int run_all(Nfs4TestCtx& ctx, const std::string& filter = "");

private:
    std::vector<ComplianceTest4> tests_;
};

// Recursively remove all contents of the named directory, then remove it.
void rmdir4_recursive(Nfs4Client& client, const Nfs4Fh& parent, const std::string& name);

}  // namespace compliance4
