// filename: cpp/core/ppx_metrics.hpp
#pragma once
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <string>

namespace ppx {

class Counter {
public:
    void inc(double value = 1.0) { total_.fetch_add(value, std::memory_order_relaxed); }
    double value() const { return total_.load(std::memory_order_relaxed); }
private:
    std::atomic<double> total_{0.0};
};

class Gauge {
public:
    void set(double value) { value_.store(value, std::memory_order_relaxed); }
    double value() const { return value_.load(std::memory_order_relaxed); }
private:
    std::atomic<double> value_{0.0};
};

class MetricsRegistry {
public:
    Counter& counter(const std::string& name) {
        std::lock_guard<std::mutex> lock(mu_);
        return counters_[name];
    }

    Gauge& gauge(const std::string& name) {
        std::lock_guard<std::mutex> lock(mu_);
        return gauges_[name];
    }

private:
    std::mutex                                    mu_;
    std::unordered_map<std::string, Counter>      counters_;
    std::unordered_map<std::string, Gauge>        gauges_;
};

} // namespace ppx
