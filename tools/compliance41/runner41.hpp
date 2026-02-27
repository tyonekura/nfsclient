#pragma once

#include "nfs41_client.hpp"

#include <functional>
#include <string>
#include <vector>

namespace compliance41 {

// Thrown by CHECK41 / EXPECT_NFS41_ERR when an assertion fails.
struct ComplianceFailure41 : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// Context passed to every compliance test.
// client is already connected and has its root_fh set.
// workdir_fh is a per-run scratch directory the test may freely use.
struct Nfs41TestCtx {
    Nfs41Client& client;
    Nfs4Fh       root_fh;
    Nfs4Fh       workdir_fh;
    std::string  server;
    std::string  export_path;
};

struct ComplianceTest41 {
    std::string                          name;     // e.g. "Session41.ExchangeId"
    std::string                          rfc_ref;  // e.g. "RFC 8881 §18.35"
    std::function<void(Nfs41TestCtx&)>   fn;
};

class TestRunner41 {
public:
    void add(ComplianceTest41 t);

    // Run all registered tests against ctx.
    // Only runs tests whose name contains `filter` (empty = run all).
    // Prints per-test status to stdout and a summary at the end.
    // Returns number of FAIL results.
    int run_all(Nfs41TestCtx& ctx, const std::string& filter = "");

private:
    std::vector<ComplianceTest41> tests_;
};

// Recursively remove all contents of the named directory, then remove it.
void rmdir41_recursive(Nfs41Client& client, const Nfs4Fh& parent, const std::string& name);

}  // namespace compliance41
