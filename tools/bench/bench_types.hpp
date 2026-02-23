#pragma once

#include "bench_stats.hpp"
#include "nfs/nfs3_types.hpp"
#include "nfs_client.hpp"

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>

struct BenchConfig {
    std::string server;
    std::string export_path;
    std::string workload;
    uint32_t    bs       = 65536;          // block size in bytes
    uint64_t    size     = 1ULL << 30;     // data file size in bytes (1 GiB)
    uint32_t    threads  = 1;
    uint32_t    duration = 30;             // wall-clock seconds
    Stable3     stable   = Stable3::UNSTABLE; // write stability mode
    double      rw_ratio = 0.7;            // read fraction for 'mixed' workload
    std::string csv_path;                  // empty = no CSV output
};

// Signature for a workload function executed on each worker thread.
// Each thread receives its own NFSClient (dedicated TCP connection).
// workdir_fh is the per-run scratch directory shared across threads.
using WorkloadRunFn = std::function<void(
    NFSClient&         client,      // dedicated per-thread client
    const Fh3&         workdir_fh,  // per-run scratch directory
    const BenchConfig& cfg,
    int                tid,         // thread index [0, threads)
    std::atomic<bool>& stop,        // set to true after duration expires
    Reservoir&         res,         // per-thread latency reservoir
    uint64_t&          ops,         // incremented per operation
    uint64_t&          bytes        // incremented per bytes transferred
)>;

struct Workload {
    std::string name;

    // Called once on the main thread before worker threads start.
    // Use it to pre-create test files. May be nullptr.
    std::function<void(NFSClient&, const Fh3& workdir, const BenchConfig&)> setup
        = nullptr;

    WorkloadRunFn run;

    // Called once on the main thread after all worker threads finish.
    // Use it to remove files that setup() created. May be nullptr.
    std::function<void(NFSClient&, const Fh3& workdir, const BenchConfig&)> teardown
        = nullptr;
};
