#include "workloads.hpp"

#include <chrono>
#include <string>

// Metadata benchmark: measures CREATE + REMOVE pairs (one "op" = one pair).
// Reports metadata IOPS — typically the bottleneck for small-file workloads.
Workload make_workload_meta() {
    return {
        "meta",

        nullptr,  // no setup

        [](NFSClient& client, const Fh3& workdir, const BenchConfig& /*cfg*/,
           int tid, std::atomic<bool>& stop, Reservoir& res,
           uint64_t& ops, uint64_t& /*bytes*/) {
            uint64_t seq = 0;
            while (!stop) {
                const std::string name =
                    "m_" + std::to_string(tid) + "_" + std::to_string(seq++);
                auto t0 = std::chrono::steady_clock::now();
                (void)client.create(workdir, name, nfs3::CreateMode3::GUARDED);
                client.remove(workdir, name);
                auto t1 = std::chrono::steady_clock::now();
                res.push(static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count()));
                ++ops;  // one op = one CREATE+REMOVE pair
            }
        },

        nullptr
    };
}
