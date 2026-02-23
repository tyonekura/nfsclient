#include "workloads.hpp"

#include <chrono>
#include <random>
#include <vector>

static const std::string BENCH_FILE_RR = "bench_data";

Workload make_workload_randread() {
    return {
        "randread",

        // setup: fill bench_data with cfg.size bytes
        [](NFSClient& client, const Fh3& workdir, const BenchConfig& cfg) {
            std::vector<uint8_t> buf(cfg.bs, 0xCD);
            Fh3 fh = client.create(workdir, BENCH_FILE_RR, nfs3::CreateMode3::UNCHECKED);
            uint64_t written = 0;
            while (written < cfg.size) {
                uint32_t chunk = static_cast<uint32_t>(
                    std::min<uint64_t>(cfg.bs, cfg.size - written));
                client.write(fh, written, Stable3::FILE_SYNC, buf.data(), chunk);
                written += chunk;
            }
        },

        // run: read at uniformly random block-aligned offsets
        [](NFSClient& client, const Fh3& workdir, const BenchConfig& cfg,
           int tid, std::atomic<bool>& stop, Reservoir& res,
           uint64_t& ops, uint64_t& bytes) {
            Fh3 fh = client.lookup(workdir, BENCH_FILE_RR);
            const uint64_t blocks = cfg.size / cfg.bs;
            const uint64_t max_block = blocks > 0 ? blocks - 1 : 0;
            std::mt19937_64 rng(std::random_device{}() ^ static_cast<uint64_t>(tid));
            std::uniform_int_distribution<uint64_t> dist(0, max_block);
            while (!stop) {
                uint64_t offset = dist(rng) * cfg.bs;
                auto t0   = std::chrono::steady_clock::now();
                auto data = client.read(fh, offset, cfg.bs);
                auto t1   = std::chrono::steady_clock::now();
                res.push(static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count()));
                bytes += data.size();
                ++ops;
            }
        },

        // teardown: remove bench_data
        [](NFSClient& client, const Fh3& workdir, const BenchConfig& /*cfg*/) {
            client.remove(workdir, BENCH_FILE_RR);
        }
    };
}
