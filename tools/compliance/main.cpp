#include "runner.hpp"
#include "nfs_client.hpp"
#include "rpc/rpc_types.hpp"

#include <cstdlib>
#include <ctime>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unistd.h>

// Registration functions declared in each test_*.cpp file.
void register_basic_tests(compliance::TestRunner&);
void register_wcc_tests(compliance::TestRunner&);
void register_commit_tests(compliance::TestRunner&);
void register_attribute_tests(compliance::TestRunner&);
void register_permission_tests(compliance::TestRunner&);
void register_stale_tests(compliance::TestRunner&);
void register_edge_case_tests(compliance::TestRunner&);

static void usage(const char* prog) {
    std::cerr << "Usage: " << prog
              << " --server <host> --export <path> [--filter <pattern>]\n";
}

int main(int argc, char* argv[]) {
    std::string server, export_path, filter;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "--server" || arg == "-s") && i + 1 < argc) {
            server = argv[++i];
        } else if ((arg == "--export" || arg == "-e") && i + 1 < argc) {
            export_path = argv[++i];
        } else if ((arg == "--filter" || arg == "-f") && i + 1 < argc) {
            filter = argv[++i];
        } else {
            usage(argv[0]);
            return 2;
        }
    }

    if (server.empty() || export_path.empty()) {
        usage(argv[0]);
        return 2;
    }

    // ── Connect as root (uid=0, AUTH_SYS) ────────────────────────────────────
    NFSClient client(server);

    AuthSys root_auth{};
    root_auth.stamp       = static_cast<uint32_t>(time(nullptr));
    root_auth.machinename = "nfsclient-compliance";
    root_auth.uid         = 0;
    root_auth.gid         = 0;
    client.set_auth_sys(root_auth);

    // ── Mount ─────────────────────────────────────────────────────────────────
    Fh3 root_fh;
    try {
        root_fh = client.mount(export_path);
    } catch (const std::exception& e) {
        std::cerr << "Fatal: mount failed: " << e.what() << "\n";
        return 1;
    }

    // ── Create per-run scratch directory ─────────────────────────────────────
    std::string workdir_name = "compliance_" + std::to_string(getpid());
    Fh3 workdir_fh;
    try {
        workdir_fh = client.mkdir(root_fh, workdir_name);
    } catch (const std::exception& e) {
        std::cerr << "Fatal: cannot create workdir '" << workdir_name
                  << "': " << e.what() << "\n";
        return 1;
    }

    // ── Register and run tests ────────────────────────────────────────────────
    compliance::TestRunner runner;
    register_basic_tests(runner);
    register_wcc_tests(runner);
    register_commit_tests(runner);
    register_attribute_tests(runner);
    register_permission_tests(runner);
    register_stale_tests(runner);
    register_edge_case_tests(runner);

    compliance::TestCtx ctx{client, root_fh, workdir_fh, server, export_path};
    std::cout << "Running compliance tests against " << server << ":" << export_path << "\n\n";
    int fails = runner.run_all(ctx, filter);

    // ── Clean up workdir ──────────────────────────────────────────────────────
    std::cout << "\nCleaning up workdir '" << workdir_name << "'...\n";
    compliance::rmdir_recursive(client, root_fh, workdir_name);

    return fails > 0 ? 1 : 0;
}
