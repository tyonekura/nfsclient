#pragma once

#include "nfs_client.hpp"
#include "nfs/nfs3_types.hpp"

#include <functional>
#include <string>
#include <vector>

namespace compliance {

enum class TestStatus { PASS, FAIL, SKIP };

// Context passed to every compliance test.
// client is a root-level (uid=0 AUTH_SYS) NFSClient already connected.
// workdir_fh is a per-run scratch directory the test may freely use.
struct TestCtx {
    NFSClient&  client;
    Fh3         root_fh;
    Fh3         workdir_fh;
    std::string server;
    std::string export_path;
};

struct ComplianceTest {
    std::string                   name;     // e.g. "Basic.LookupExistingFile"
    std::string                   rfc_ref;  // e.g. "RFC 1813 §3.3.14"
    std::function<void(TestCtx&)> fn;
};

class TestRunner {
public:
    void add(ComplianceTest t);

    // Run all registered tests against ctx.
    // Only runs tests whose name contains `filter` (empty = run all).
    // Prints per-test status to stdout and a summary at the end.
    // Returns number of FAIL results.
    int run_all(TestCtx& ctx, const std::string& filter = "");

private:
    std::vector<ComplianceTest> tests_;
};

// Recursively remove all contents of the named directory, then rmdir it.
void rmdir_recursive(NFSClient& client, const Fh3& parent, const std::string& name);

}  // namespace compliance
