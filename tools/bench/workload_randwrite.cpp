#include "workloads.hpp"

#include <chrono>
#include <random>
#include <vector>

// Each thread writes to its own file at random block-aligned offsets.
Workload make_workload_randwrite() {
    return {
        "randwrite",

        nullptr,  // no shared setup

        // run: write random blocks within [0, cfg.size)
        [](NFSClient& client, const Fh3& workdir, const BenchConfig& cfg,
           int tid, std::atomic<bool>& stop, Reservoir& res,
           uint64_t& ops, uint64_t& bytes) {
            const std::string fname = "bench_rw_" + std::to_string(tid);
            Fh3 fh = client.create(workdir, fname, nfs3::CreateMode3::UNCHECKED);

            // Pre-extend the file to cfg.size so random writes don't grow it.
            {
                Sattr3 sa;
                sa.set_size = true;
                sa.size     = cfg.size;
                client.setattr(fh, sa);
            }

            std::vector<uint8_t> buf(cfg.bs, 0xDE);
            const uint64_t blocks    = cfg.size / cfg.bs;
            const uint64_t max_block = blocks > 0 ? blocks - 1 : 0;
            std::mt19937_64 rng(std::random_device{}() ^ static_cast<uint64_t>(tid));
            std::uniform_int_distribution<uint64_t> dist(0, max_block);

            while (!stop) {
                uint64_t offset = dist(rng) * cfg.bs;
                auto t0     = std::chrono::steady_clock::now();
                auto result = client.write(fh, offset, cfg.stable, buf.data(), cfg.bs);
                auto t1     = std::chrono::steady_clock::now();
                res.push(static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count()));
                bytes += result.count;
                ++ops;
            }
            client.remove(workdir, fname);
        },

        nullptr
    };
}
