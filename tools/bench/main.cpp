#include "bench_stats.hpp"
#include "bench_types.hpp"
#include "workloads.hpp"

#include "nfs/nfs3_types.hpp"
#include "nfs_client.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <thread>
#include <unistd.h>
#include <vector>

// ── Utility helpers ──────────────────────────────────────────────────────────

static uint64_t parse_size(const char* s) {
    char* end = nullptr;
    uint64_t v = strtoull(s, &end, 10);
    if (!end || end == s) throw std::runtime_error(std::string("bad size: ") + s);
    switch (*end) {
        case 'K': case 'k': v <<= 10; break;
        case 'M': case 'm': v <<= 20; break;
        case 'G': case 'g': v <<= 30; break;
        case '\0': break;
        default: throw std::runtime_error(std::string("bad size suffix: ") + end);
    }
    return v;
}

static std::string human_bytes(uint64_t n) {
    char buf[32];
    if      (n >= (1ULL << 30)) snprintf(buf, sizeof(buf), "%.1f GiB", n / double(1ULL << 30));
    else if (n >= (1ULL << 20)) snprintf(buf, sizeof(buf), "%.1f MiB", n / double(1ULL << 20));
    else if (n >= (1ULL << 10)) snprintf(buf, sizeof(buf), "%.1f KiB", n / double(1ULL << 10));
    else                        snprintf(buf, sizeof(buf), "%llu B", (unsigned long long)n);
    return buf;
}

static std::string human_ns(uint64_t ns) {
    char buf[32];
    if      (ns >= 1'000'000'000ULL) snprintf(buf, sizeof(buf), "%.2f s",  ns / 1e9);
    else if (ns >= 1'000'000ULL)     snprintf(buf, sizeof(buf), "%.2f ms", ns / 1e6);
    else if (ns >= 1'000ULL)         snprintf(buf, sizeof(buf), "%.2f us", ns / 1e3);
    else                             snprintf(buf, sizeof(buf), "%llu ns", (unsigned long long)ns);
    return buf;
}

static void print_usage(const char* prog) {
    fprintf(stderr,
        "Usage: %s --server HOST --export PATH --workload NAME [options]\n"
        "\n"
        "Workloads: seqread, seqwrite, randread, randwrite, meta, mixed\n"
        "\n"
        "Options:\n"
        "  --bs <bytes>       Block size (default 65536, supports K/M/G suffixes)\n"
        "  --size <bytes>     Data file size (default 1G)\n"
        "  --threads <n>      Concurrent connections/threads (default 1)\n"
        "  --duration <s>     Run time in seconds (default 30)\n"
        "  --stable <mode>    Write stability: unstable, datasync, filesync (default unstable)\n"
        "  --rw-ratio <0-1>   Read fraction for 'mixed' workload (default 0.7)\n"
        "  --csv <path>       Append results to a CSV file\n",
        prog);
}

// ── Recursive workdir cleanup ─────────────────────────────────────────────────

static void rmdir_recursive(NFSClient& client, const Fh3& parent, const std::string& name);

static void clear_dir(NFSClient& client, const Fh3& dir) {
    for (const auto& entry : client.readdirplus(dir)) {
        if (entry.name == "." || entry.name == "..") continue;
        if (entry.has_attrs && entry.attrs.type == Ftype3::NF3DIR) {
            rmdir_recursive(client, dir, entry.name);
        } else {
            client.remove(dir, entry.name);
        }
    }
}

static void rmdir_recursive(NFSClient& client, const Fh3& parent, const std::string& name) {
    Fh3 dir_fh = client.lookup(parent, name);
    clear_dir(client, dir_fh);
    client.rmdir(parent, name);
}

// ── Run a workload across N threads ──────────────────────────────────────────

struct RunResult {
    uint64_t        total_ops;
    uint64_t        total_bytes;
    double          elapsed_s;
    Reservoir::Stats lat;
};

static RunResult run_workload(const Workload& wl, const BenchConfig& cfg,
                              const std::string& host, const Fh3& workdir_fh) {
    // Per-thread state
    const int N = static_cast<int>(cfg.threads);
    std::vector<Reservoir>  reservoirs(N);
    std::vector<uint64_t>   thread_ops(N, 0);
    std::vector<uint64_t>   thread_bytes(N, 0);
    std::atomic<bool>       stop{false};

    auto worker = [&](int tid) {
        try {
            NFSClient client(host);
            AuthSys auth{};
            auth.uid = 0; auth.gid = 0;
            client.set_auth_sys(auth);
            wl.run(client, workdir_fh, cfg, tid, stop,
                   reservoirs[tid], thread_ops[tid], thread_bytes[tid]);
        } catch (const std::exception& e) {
            fprintf(stderr, "[thread %d] error: %s\n", tid, e.what());
        }
    };

    auto t_start = std::chrono::steady_clock::now();

    std::vector<std::thread> threads;
    threads.reserve(N);
    for (int i = 0; i < N; ++i)
        threads.emplace_back(worker, i);

    std::this_thread::sleep_for(std::chrono::seconds(cfg.duration));
    stop.store(true, std::memory_order_relaxed);

    for (auto& t : threads) t.join();

    auto t_end = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(t_end - t_start).count();

    // Merge per-thread reservoirs into one
    Reservoir merged;
    uint64_t total_ops   = 0;
    uint64_t total_bytes = 0;
    for (int i = 0; i < N; ++i) {
        merged.merge(reservoirs[i]);
        total_ops   += thread_ops[i];
        total_bytes += thread_bytes[i];
    }

    return {total_ops, total_bytes, elapsed, merged.compute()};
}

// ── Output formatting ─────────────────────────────────────────────────────────

static void print_result(const BenchConfig& cfg, const RunResult& r) {
    printf("\n");
    printf("Workload : %s\n", cfg.workload.c_str());
    printf("bs       : %s\n", human_bytes(cfg.bs).c_str());
    printf("size     : %s\n", human_bytes(cfg.size).c_str());
    printf("threads  : %u\n", cfg.threads);
    printf("duration : %.1f s\n", r.elapsed_s);
    printf("\n");
    printf("%-12s %-14s %-10s %-10s %-10s %-10s %-10s\n",
           "Ops", "Throughput", "lat_min", "lat_p50", "lat_p95", "lat_p99", "lat_max");
    printf("%-12s %-14s %-10s %-10s %-10s %-10s %-10s\n",
           "───────────", "─────────────", "─────────", "─────────",
           "─────────", "─────────", "─────────");

    // Throughput: for meta workload bytes is 0, show IOPS instead
    std::string tput;
    if (r.total_bytes > 0) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f MB/s", r.total_bytes / 1e6 / r.elapsed_s);
        tput = buf;
    } else {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.0f IOPS", r.total_ops / r.elapsed_s);
        tput = buf;
    }

    printf("%-12llu %-14s %-10s %-10s %-10s %-10s %-10s\n",
           (unsigned long long)r.total_ops,
           tput.c_str(),
           human_ns(r.lat.min_ns).c_str(),
           human_ns(r.lat.p50_ns).c_str(),
           human_ns(r.lat.p95_ns).c_str(),
           human_ns(r.lat.p99_ns).c_str(),
           human_ns(r.lat.max_ns).c_str());
    printf("\n");
}

static void write_csv(const std::string& path, const BenchConfig& cfg, const RunResult& r) {
    bool write_header = true;
    {
        std::ifstream test(path);
        if (test.good()) write_header = false;
    }
    std::ofstream f(path, std::ios::app);
    if (!f) {
        fprintf(stderr, "warning: cannot open CSV file '%s'\n", path.c_str());
        return;
    }
    if (write_header) {
        f << "workload,bs,size,threads,duration_s,ops,throughput_mb_s,"
          << "lat_min_us,lat_p50_us,lat_p95_us,lat_p99_us,lat_max_us\n";
    }
    auto to_us = [](uint64_t ns) { return ns / 1000.0; };
    f << cfg.workload << ","
      << cfg.bs << ","
      << cfg.size << ","
      << cfg.threads << ","
      << r.elapsed_s << ","
      << r.total_ops << ","
      << (r.total_bytes > 0 ? r.total_bytes / 1e6 / r.elapsed_s : 0.0) << ","
      << to_us(r.lat.min_ns) << ","
      << to_us(r.lat.p50_ns) << ","
      << to_us(r.lat.p95_ns) << ","
      << to_us(r.lat.p99_ns) << ","
      << to_us(r.lat.max_ns) << "\n";
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    BenchConfig cfg;

    for (int i = 1; i < argc; ++i) {
        auto arg = [&](const char* flag) -> bool {
            if (strcmp(argv[i], flag) == 0 && i + 1 < argc) { ++i; return true; }
            return false;
        };
        if      (arg("--server"))   cfg.server      = argv[i];
        else if (arg("--export"))   cfg.export_path = argv[i];
        else if (arg("--workload")) cfg.workload    = argv[i];
        else if (arg("--bs"))       cfg.bs          = static_cast<uint32_t>(parse_size(argv[i]));
        else if (arg("--size"))     cfg.size        = parse_size(argv[i]);
        else if (arg("--threads"))  cfg.threads     = static_cast<uint32_t>(atoi(argv[i]));
        else if (arg("--duration")) cfg.duration    = static_cast<uint32_t>(atoi(argv[i]));
        else if (arg("--rw-ratio")) cfg.rw_ratio    = atof(argv[i]);
        else if (arg("--csv"))      cfg.csv_path    = argv[i];
        else if (arg("--stable")) {
            std::string s = argv[i];
            if      (s == "unstable")  cfg.stable = Stable3::UNSTABLE;
            else if (s == "datasync")  cfg.stable = Stable3::DATA_SYNC;
            else if (s == "filesync")  cfg.stable = Stable3::FILE_SYNC;
            else { fprintf(stderr, "unknown stable mode: %s\n", s.c_str()); return 1; }
        } else {
            fprintf(stderr, "unknown argument: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    if (cfg.server.empty() || cfg.export_path.empty() || cfg.workload.empty()) {
        print_usage(argv[0]);
        return 1;
    }
    if (cfg.bs == 0 || cfg.size == 0 || cfg.threads == 0 || cfg.duration == 0) {
        fprintf(stderr, "bs, size, threads, and duration must be > 0\n");
        return 1;
    }

    // Build workload registry
    std::map<std::string, std::function<Workload()>> registry = {
        {"seqread",   make_workload_seqread},
        {"seqwrite",  make_workload_seqwrite},
        {"randread",  make_workload_randread},
        {"randwrite", make_workload_randwrite},
        {"meta",      make_workload_meta},
        {"mixed",     make_workload_mixed},
    };

    auto it = registry.find(cfg.workload);
    if (it == registry.end()) {
        fprintf(stderr, "unknown workload '%s'\n", cfg.workload.c_str());
        fprintf(stderr, "available: seqread, seqwrite, randread, randwrite, meta, mixed\n");
        return 1;
    }
    Workload wl = it->second();

    // Connect main client (uid=0 so it can create the workdir and test files)
    NFSClient main_client(cfg.server);
    {
        AuthSys auth{};
        auth.uid = 0; auth.gid = 0;
        main_client.set_auth_sys(auth);
    }
    Fh3 root_fh = main_client.mount(cfg.export_path);

    // Create per-run workdir: bench_<pid>
    const std::string workdir_name = "bench_" + std::to_string(getpid());
    Fh3 workdir_fh = main_client.mkdir(root_fh, workdir_name);

    int rc = 0;
    try {
        // Phase 1: setup (pre-create test files)
        if (wl.setup) {
            fprintf(stderr, "Setting up workload '%s' (file size %s)...\n",
                    cfg.workload.c_str(), human_bytes(cfg.size).c_str());
            wl.setup(main_client, workdir_fh, cfg);
        }

        // Phase 2: run workers
        fprintf(stderr, "Running '%s' for %u s with %u thread(s)...\n",
                cfg.workload.c_str(), cfg.duration, cfg.threads);
        RunResult result = run_workload(wl, cfg, cfg.server, workdir_fh);

        // Phase 3: teardown (remove test files created by setup)
        if (wl.teardown) wl.teardown(main_client, workdir_fh, cfg);

        print_result(cfg, result);
        if (!cfg.csv_path.empty()) write_csv(cfg.csv_path, cfg, result);

    } catch (const std::exception& e) {
        fprintf(stderr, "error: %s\n", e.what());
        rc = 1;
    }

    // Always clean up the workdir
    try {
        rmdir_recursive(main_client, root_fh, workdir_name);
    } catch (const std::exception& e) {
        fprintf(stderr, "warning: workdir cleanup failed: %s\n", e.what());
    }

    return rc;
}
