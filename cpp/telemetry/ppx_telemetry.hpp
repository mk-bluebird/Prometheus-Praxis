// filename: cpp/telemetry/ppx_telemetry.hpp
// destination: github.com/mk-bluebird/Prometheus-Praxis/cpp/telemetry/ppx_telemetry.hpp
//
// High-performance, thread-safe Prometheus-style metrics registry for local agents.
// This header provides:
//   - Label: simple key/value metadata for metrics.
//   - Counter: atomic counter with minimal locking.
//   - Registry: family of counters keyed by metric name + label set, with text exposition.
//
// Intended usage:
//   - Local C++ agents and kernels call get_or_create_counter() and increment() on hot paths.
//   - Python tooling reads metrics via Prometheus text exposition or JSON bindings.

#ifndef INCLUDED_PROMETHEUS_PRAXIS_TELEMETRY_HPP
#define INCLUDED_PROMETHEUS_PRAXIS_TELEMETRY_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <memory>
#include <sstream>
#include <functional>

namespace praxis_metrics {

struct Label {
    std::string name;
    std::string value;

    bool operator==(const Label& other) const {
        return name == other.name && value == other.value;
    }
};

// Hash labels as a vector, suitable for unordered_map.
struct LabelHash {
    std::size_t operator()(const std::vector<Label>& labels) const {
        std::size_t seed = 0;
        for (const auto& label : labels) {
            std::size_t h1 = std::hash<std::string>()(label.name);
            std::size_t h2 = std::hash<std::string>()(label.value);
            seed ^= h1 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= h2 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
        return seed;
    }
};

class Counter {
private:
    double value_{0.0};
    mutable std::mutex cell_mutex_;
    const std::vector<Label> dimensions_;

public:
    explicit Counter(std::vector<Label> labels)
        : dimensions_(std::move(labels)) {}

    Counter(const Counter&) = delete;
    Counter& operator=(const Counter&) = delete;

    void increment(double amount = 1.0) {
        if (amount <= 0.0) {
            return;
        }
        std::lock_guard<std::mutex> lock(cell_mutex_);
        value_ += amount;
    }

    double value() const {
        std::lock_guard<std::mutex> lock(cell_mutex_);
        return value_;
    }

    const std::vector<Label>& labels() const {
        return dimensions_;
    }
};

class Registry {
private:
    std::mutex registry_mutex_;
    // metrics_[metric_name][labels_vector] -> Counter
    std::unordered_map<std::string,
        std::unordered_map<std::vector<Label>, std::shared_ptr<Counter>, LabelHash>
    > metrics_;

public:
    std::shared_ptr<Counter> get_or_create_counter(
        const std::string& name,
        const std::vector<Label>& labels
    ) {
        std::lock_guard<std::mutex> lock(registry_mutex_);
        auto& family = metrics_[name];
        auto it = family.find(labels);
        if (it != family.end()) {
            return it->second;
        }
        auto counter = std::make_shared<Counter>(labels);
        family[labels] = counter;
        return counter;
    }

    // Serialize registry contents in Prometheus text exposition format.
    std::string serialize_to_text() const {
        std::lock_guard<std::mutex> lock(registry_mutex_);
        std::stringstream ss;
        for (const auto& metric_family : metrics_) {
            const auto& name = metric_family.first;
            ss << "# TYPE " << name << " counter\n";
            for (const auto& pair : metric_family.second) {
                const auto& counter = pair.second;
                ss << name;
                const auto& labels = counter->labels();
                if (!labels.empty()) {
                    ss << "{";
                    for (std::size_t i = 0; i < labels.size(); ++i) {
                        ss << labels[i].name << "=\"" << labels[i].value << "\"";
                        if (i + 1 < labels.size()) {
                            ss << ",";
                        }
                    }
                    ss << "}";
                }
                ss << " " << counter->value() << "\n";
            }
        }
        return ss.str();
    }
};

} // namespace praxis_metrics

#endif // INCLUDED_PROMETHEUS_PRAXIS_TELEMETRY_HPP
