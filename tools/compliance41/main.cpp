#include "runner41.hpp"
#include "nfs41_client.hpp"
#include "rpc/rpc_types.hpp"

#include <cstdlib>
#include <ctime>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unistd.h>

// Registration functions declared in each test_*.cpp file.
void register_session41_tests(compliance41::TestRunner41&);
void register_basic41_tests(compliance41::TestRunner41&);
void register_attrs41_tests(compliance41::TestRunner41&);
void register_stateid41_tests(compliance41::TestRunner41&);
void register_rename41_tests(compliance41::TestRunner41&);

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

    // ── Connect as root (AUTH_SYS uid=0) ─────────────────────────────────────
    AuthSys root_auth{};
    root_auth.stamp       = static_cast<uint32_t>(time(nullptr));
    root_auth.machinename = "nfsclient-compliance41";
    root_auth.uid         = 0;
    root_auth.gid         = 0;

    Nfs41Client client(server, root_auth);

    // ── Root FH ───────────────────────────────────────────────────────────────
    Nfs4Fh root_fh = client.root_fh();

    std::cerr << "[diag] NFSv4.1 session established\n";
    std::cerr << "[diag] root_fh sentinel (empty=" << root_fh.data.empty() << ")"
              << " — all root ops use PUTROOTFH instead of PUTFH\n";

    try {
        Fattr4 root_attrs = client.getattr(root_fh);
        std::cerr << "[diag] getattr(root_fh): OK"
                  << " type=" << (root_attrs.type ? static_cast<int>(*root_attrs.type) : -1)
                  << " mode=" << (root_attrs.mode ? std::to_string(*root_attrs.mode) : "?")
                  << "\n";
    } catch (const std::exception& e) {
        std::cerr << "[diag] getattr(root_fh): FAILED: " << e.what() << "\n";
    }

    try {
        uint32_t granted = client.access(root_fh, 0x1F);
        std::cerr << "[diag] access(root_fh): OK granted=0x" << std::hex << granted << std::dec << "\n";
    } catch (const std::exception& e) {
        std::cerr << "[diag] access(root_fh): FAILED: " << e.what() << "\n";
    }

    try {
        auto entries = client.readdir(root_fh);
        std::cerr << "[diag] readdir(root_fh): OK " << entries.size() << " entries\n";
    } catch (const std::exception& e) {
        std::cerr << "[diag] readdir(root_fh): FAILED: " << e.what() << "\n";
    }

    try {
        client.lookup(root_fh, "zzznonexistent_diag");
        std::cerr << "[diag] lookup(root_fh, nonexistent): unexpectedly succeeded\n";
    } catch (const Nfs4Error& e) {
        std::cerr << "[diag] lookup(root_fh, nonexistent): nfsstat4=" << e.status
                  << " (expect 2=NOENT)\n";
    } catch (const std::exception& e) {
        std::cerr << "[diag] lookup(root_fh, nonexistent): " << e.what() << "\n";
    }

    // ── Create per-run scratch directory ─────────────────────────────────────
    std::string workdir_name = "compliance41_" + std::to_string(getpid());

    try { compliance41::rmdir41_recursive(client, root_fh, workdir_name); }
    catch (...) { /* not present — that's fine */ }

    Nfs4Fh workdir_fh;
    try {
        workdir_fh = client.mkdir(root_fh, workdir_name);
    } catch (const std::exception& e) {
        std::cerr << "Fatal: cannot create workdir '" << workdir_name
                  << "': " << e.what() << "\n";
        return 1;
    }

    // ── Register and run tests ────────────────────────────────────────────────
    compliance41::TestRunner41 runner;
    register_session41_tests(runner);
    register_basic41_tests(runner);
    register_attrs41_tests(runner);
    register_stateid41_tests(runner);
    register_rename41_tests(runner);

    compliance41::Nfs41TestCtx ctx{client, root_fh, workdir_fh, server, export_path};
    std::cout << "Running NFSv4.1 compliance tests against "
              << server << ":" << export_path << "\n\n";
    int fails = runner.run_all(ctx, filter);

    // ── Clean up workdir ──────────────────────────────────────────────────────
    std::cout << "\nCleaning up workdir '" << workdir_name << "'...\n";
    try {
        compliance41::rmdir41_recursive(client, root_fh, workdir_name);
    } catch (const std::exception& e) {
        std::cerr << "Warning: cleanup failed: " << e.what() << "\n";
    }

    return fails > 0 ? 1 : 0;
}
