#include "workloads.hpp"

#include <chrono>
#include <string>
#include <vector>

// Each thread writes to its own file to avoid offset contention.
Workload make_workload_seqwrite() {
    return {
        "seqwrite",

        nullptr,  // no shared setup needed

        // run: write sequentially to bench_write_<tid>, cycling at cfg.size
        [](NFSClient& client, const Fh3& workdir, const BenchConfig& cfg,
           int tid, std::atomic<bool>& stop, Reservoir& res,
           uint64_t& ops, uint64_t& bytes) {
            const std::string fname = "bench_write_" + std::to_string(tid);
            Fh3 fh = client.create(workdir, fname, nfs3::CreateMode3::UNCHECKED);
            std::vector<uint8_t> buf(cfg.bs, 0xBC);
            uint64_t offset = 0;
            while (!stop) {
                auto t0     = std::chrono::steady_clock::now();
                auto result = client.write(fh, offset, cfg.stable, buf.data(), cfg.bs);
                auto t1     = std::chrono::steady_clock::now();
                res.push(static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count()));
                bytes += result.count;
                ++ops;
                offset += result.count;
                if (offset >= cfg.size) offset = 0;
            }
            // Clean up this thread's file before returning.
            client.remove(workdir, fname);
        },

        nullptr   // no global teardown (each thread cleans up its own file)
    };
}
