#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

// Accumulates per-operation latency samples (nanoseconds) and computes
// percentile statistics. Not thread-safe; use one Reservoir per thread.
class Reservoir {
public:
    void push(uint64_t ns) { samples_.push_back(ns); }
    size_t count() const { return samples_.size(); }
    bool   empty() const { return samples_.empty(); }

    void merge(const Reservoir& other) {
        samples_.insert(samples_.end(), other.samples_.begin(), other.samples_.end());
    }

    struct Stats {
        uint64_t min_ns = 0;
        uint64_t p50_ns = 0;
        uint64_t p95_ns = 0;
        uint64_t p99_ns = 0;
        uint64_t max_ns = 0;
    };

    Stats compute() {
        if (samples_.empty()) return {};
        std::sort(samples_.begin(), samples_.end());
        auto pct = [&](double p) -> uint64_t {
            size_t idx = static_cast<size_t>(p * static_cast<double>(samples_.size() - 1) + 0.5);
            if (idx >= samples_.size()) idx = samples_.size() - 1;
            return samples_[idx];
        };
        return {samples_.front(), pct(0.50), pct(0.95), pct(0.99), samples_.back()};
    }

private:
    std::vector<uint64_t> samples_;
};
