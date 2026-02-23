#include "workloads.hpp"

#include <chrono>
#include <vector>

static const std::string BENCH_FILE_SR = "bench_data";

Workload make_workload_seqread() {
    return {
        "seqread",

        // setup: fill bench_data with cfg.size bytes of pattern data
        [](NFSClient& client, const Fh3& workdir, const BenchConfig& cfg) {
            std::vector<uint8_t> buf(cfg.bs, 0xAB);
            Fh3 fh = client.create(workdir, BENCH_FILE_SR, nfs3::CreateMode3::UNCHECKED);
            uint64_t written = 0;
            while (written < cfg.size) {
                uint32_t chunk = static_cast<uint32_t>(
                    std::min<uint64_t>(cfg.bs, cfg.size - written));
                client.write(fh, written, Stable3::FILE_SYNC, buf.data(), chunk);
                written += chunk;
            }
        },

        // run: read bench_data sequentially, wrapping at EOF
        [](NFSClient& client, const Fh3& workdir, const BenchConfig& cfg,
           int /*tid*/, std::atomic<bool>& stop, Reservoir& res,
           uint64_t& ops, uint64_t& bytes) {
            Fh3 fh = client.lookup(workdir, BENCH_FILE_SR);
            uint64_t offset = 0;
            while (!stop) {
                auto t0   = std::chrono::steady_clock::now();
                auto data = client.read(fh, offset, cfg.bs);
                auto t1   = std::chrono::steady_clock::now();
                res.push(static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count()));
                bytes += data.size();
                ++ops;
                offset += static_cast<uint64_t>(data.size());
                if (data.empty() || offset >= cfg.size) offset = 0;
            }
        },

        // teardown: remove bench_data
        [](NFSClient& client, const Fh3& workdir, const BenchConfig& /*cfg*/) {
            client.remove(workdir, BENCH_FILE_SR);
        }
    };
}
