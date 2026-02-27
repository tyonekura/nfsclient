#include "runner4.hpp"
#include "nfs4/nfs4_error.hpp"

#include <iostream>
#include <string>

namespace compliance4 {

void TestRunner4::add(ComplianceTest4 t) {
    tests_.push_back(std::move(t));
}

int TestRunner4::run_all(Nfs4TestCtx& ctx, const std::string& filter) {
    int passes = 0, fails = 0, skips = 0;

    for (const auto& test : tests_) {
        if (!filter.empty() && test.name.find(filter) == std::string::npos) {
            continue;
        }

        try {
            test.fn(ctx);
            std::cout << "  [PASS] " << test.name
                      << "  (" << test.rfc_ref << ")\n";
            ++passes;
        } catch (const ComplianceFailure4& e) {
            std::cout << "  [FAIL] " << test.name
                      << "  (" << test.rfc_ref << ")\n"
                      << "         " << e.what() << "\n";
            ++fails;
        } catch (const Nfs4Error& e) {
            std::cout << "  [FAIL] " << test.name
                      << "  (" << test.rfc_ref << ")\n"
                      << "         NFS4 error: " << e.what() << "\n";
            ++fails;
        } catch (const std::runtime_error& e) {
            std::cout << "  [SKIP] " << test.name
                      << "  (" << test.rfc_ref << ")\n"
                      << "         " << e.what() << "\n";
            ++skips;
        }
    }

    std::cout << "\nResults: " << passes << " passed, "
              << fails << " failed, " << skips << " skipped\n";
    return fails;
}

void rmdir4_recursive(Nfs4Client& client, const Nfs4Fh& parent, const std::string& name) {
    Nfs4Fh dir = client.lookup(parent, name);
    auto entries = client.readdir(dir);
    for (const auto& e : entries) {
        if (e.name == "." || e.name == "..") continue;
        if (e.attrs.type.has_value() && *e.attrs.type == Ftype4::NF4DIR) {
            rmdir4_recursive(client, dir, e.name);
        } else {
            client.remove(dir, e.name);
        }
    }
    client.remove(parent, name);
}

}  // namespace compliance4
