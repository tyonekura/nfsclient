#include "runner.hpp"
#include "test_helpers.hpp"
#include "nfs/nfs_error.hpp"
#include "nfs/readdirplus.hpp"

#include <iostream>
#include <stdexcept>

namespace compliance {

namespace {

const char* GREEN  = "\033[32m";
const char* RED    = "\033[31m";
const char* YELLOW = "\033[33m";
const char* RESET  = "\033[0m";

std::string status_tag(TestStatus s) {
    switch (s) {
        case TestStatus::PASS: return std::string(GREEN)  + "[PASS]" + RESET;
        case TestStatus::FAIL: return std::string(RED)    + "[FAIL]" + RESET;
        case TestStatus::SKIP: return std::string(YELLOW) + "[SKIP]" + RESET;
    }
    return "[????]";
}

// Recursively remove all children of dir_fh (depth-first).
void clear_dir(NFSClient& client, const Fh3& dir_fh) {
    try {
        auto entries = client.readdirplus(dir_fh);
        for (const auto& e : entries) {
            if (e.name == "." || e.name == "..") continue;
            try {
                bool is_dir = e.has_attrs && e.attrs.type == Ftype3::NF3DIR;
                if (is_dir) {
                    Fh3 child = e.has_fh ? e.fh : client.lookup(dir_fh, e.name);
                    clear_dir(client, child);
                    client.rmdir(dir_fh, e.name);
                } else {
                    client.remove(dir_fh, e.name);
                }
            } catch (...) {}
        }
    } catch (...) {}
}

}  // anonymous namespace

void TestRunner::add(ComplianceTest t) {
    tests_.push_back(std::move(t));
}

int TestRunner::run_all(TestCtx& ctx, const std::string& filter) {
    int pass = 0, fail = 0, skip = 0;

    for (auto& t : tests_) {
        if (!filter.empty() && t.name.find(filter) == std::string::npos) continue;

        TestStatus  status = TestStatus::PASS;
        std::string detail;

        try {
            t.fn(ctx);
        } catch (const ComplianceFailure& e) {
            status = TestStatus::FAIL;
            detail = e.what();
        } catch (const NfsError& e) {
            status = TestStatus::FAIL;
            detail = e.what();
        } catch (const std::runtime_error& e) {
            status = TestStatus::SKIP;
            detail = e.what();
        } catch (const std::exception& e) {
            status = TestStatus::SKIP;
            detail = e.what();
        }

        std::cout << status_tag(status) << " " << t.name;
        if (!t.rfc_ref.empty()) std::cout << " (" << t.rfc_ref << ")";
        if (!detail.empty())    std::cout << ": " << detail;
        std::cout << "\n";

        switch (status) {
            case TestStatus::PASS: ++pass; break;
            case TestStatus::FAIL: ++fail; break;
            case TestStatus::SKIP: ++skip; break;
        }
    }

    std::cout << "\nResults: "
              << GREEN  << pass << " passed" << RESET << ", "
              << RED    << fail << " failed" << RESET << ", "
              << YELLOW << skip << " skipped" << RESET
              << " out of " << (pass + fail + skip) << " tests.\n";

    return fail;
}

void rmdir_recursive(NFSClient& client, const Fh3& parent, const std::string& name) {
    try {
        Fh3 dir_fh = client.lookup(parent, name);
        clear_dir(client, dir_fh);
        client.rmdir(parent, name);
    } catch (...) {}
}

}  // namespace compliance
