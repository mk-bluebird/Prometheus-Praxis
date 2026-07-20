// filename: cpp/core/prometheus_praxis_telemetry.hpp
// destination: github.com/mk-bluebird/Prometheus-Praxis/cpp/core
//
// Prometheus-Praxis telemetry/metrics library.
// - Thread-safe counters, gauges, and timers with label dimensions.
// - Text serialization compatible with Prometheus exposition format.
// - Designed for use by Rust ALNv2 kernels via C++ wrappers, and by CPP tools.

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

struct LabelHash {
    std::size_t operator()(const std::vector<Label>& labels) const {
        std::size_t seed = 0;
        for (const auto& label : labels) {
            seed ^= std::hash<std::string>()(label.name)
                ^ std::hash<std::string>()(label.value)
                ^ (0x9e3779b9 + (seed << 6) + (seed >> 2));
        }
        return seed;
    }
};

// -------------------------
// Basic metric primitives
// -------------------------

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
        if (amount <= 0.0) return;
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

class Gauge {
private:
    double value_{0.0};
    mutable std::mutex cell_mutex_;
    const std::vector<Label> dimensions_;

public:
    explicit Gauge(std::vector<Label> labels)
        : dimensions_(std::move(labels)) {}

    Gauge(const Gauge&) = delete;
    Gauge& operator=(const Gauge&) = delete;

    void set(double value) {
        std::lock_guard<std::mutex> lock(cell_mutex_);
        value_ = value;
    }

    void add(double delta) {
        std::lock_guard<std::mutex> lock(cell_mutex_);
        value_ += delta;
    }

    double value() const {
        std::lock_guard<std::mutex> lock(cell_mutex_);
        return value_;
    }

    const std::vector<Label>& labels() const {
        return dimensions_;
    }
};

class Timer {
private:
    std::chrono::steady_clock::time_point start_;
    double last_seconds_{0.0};
    mutable std::mutex cell_mutex_;
    const std::vector<Label> dimensions_;

public:
    explicit Timer(std::vector<Label> labels)
        : start_(std::chrono::steady_clock::now()),
          dimensions_(std::move(labels)) {}

    Timer(const Timer&) = delete;
    Timer& operator=(const Timer&) = delete;

    void reset() {
        std::lock_guard<std::mutex> lock(cell_mutex_);
        start_ = std::chrono::steady_clock::now();
        last_seconds_ = 0.0;
    }

    void stop() {
        std::lock_guard<std::mutex> lock(cell_mutex_);
        auto end = std::chrono::steady_clock::now();
        last_seconds_ = std::chrono::duration<double>(end - start_).count();
    }

    double last_seconds() const {
        std::lock_guard<std::mutex> lock(cell_mutex_);
        return last_seconds_;
    }

    const std::vector<Label>& labels() const {
        return dimensions_;
    }
};

// -------------------------
// Metric families and registry
// -------------------------

enum class MetricKind {
    Counter,
    Gauge,
    Timer,
};

struct MetricFamily {
    MetricKind kind;
    std::unordered_map<std::vector<Label>, std::shared_ptr<Counter>, LabelHash> counters;
    std::unordered_map<std::vector<Label>, std::shared_ptr<Gauge>, LabelHash>   gauges;
    std::unordered_map<std::vector<Label>, std::shared_ptr<Timer>, LabelHash>   timers;
};

class Registry {
private:
    std::mutex registry_mutex_;
    std::unordered_map<std::string, MetricFamily> families_;

public:
    Registry() = default;
    Registry(const Registry&) = delete;
    Registry& operator=(const Registry&) = delete;

    // Counter API
    std::shared_ptr<Counter> get_or_create_counter(
        const std::string& name,
        const std::vector<Label>& labels
    ) {
        std::lock_guard<std::mutex> lock(registry_mutex_);
        auto& family = families_[name];
        if (family.kind == MetricKind::Gauge || family.kind == MetricKind::Timer) {
            // mixed-kind family not allowed
            return nullptr;
        }
        family.kind = MetricKind::Counter;
        auto it = family.counters.find(labels);
        if (it != family.counters.end()) {
            return it->second;
        }
        auto counter = std::make_shared<Counter>(labels);
        family.counters[labels] = counter;
        return counter;
    }

    // Gauge API
    std::shared_ptr<Gauge> get_or_create_gauge(
        const std::string& name,
        const std::vector<Label>& labels
    ) {
        std::lock_guard<std::mutex> lock(registry_mutex_);
        auto& family = families_[name];
        if (family.kind == MetricKind::Counter || family.kind == MetricKind::Timer) {
            return nullptr;
        }
        family.kind = MetricKind::Gauge;
        auto it = family.gauges.find(labels);
        if (it != family.gauges.end()) {
            return it->second;
        }
        auto gauge = std::make_shared<Gauge>(labels);
        family.gauges[labels] = gauge;
        return gauge;
    }

    // Timer API
    std::shared_ptr<Timer> get_or_create_timer(
        const std::string& name,
        const std::vector<Label>& labels
    ) {
        std::lock_guard<std::mutex> lock(registry_mutex_);
        auto& family = families_[name];
        if (family.kind == MetricKind::Counter || family.kind == MetricKind::Gauge) {
            return nullptr;
        }
        family.kind = MetricKind::Timer;
        auto it = family.timers.find(labels);
        if (it != family.timers.end()) {
            return it->second;
        }
        auto timer = std::make_shared<Timer>(labels);
        family.timers[labels] = timer;
        return timer;
    }

    // Prometheus-style text exposition for counters and gauges.
    // Timers are exposed as _seconds gauge metrics.
    std::string serialize_to_text() {
        std::lock_guard<std::mutex> lock(registry_mutex_);
        std::stringstream ss;
        for (const auto& entry : families_) {
            const auto& name   = entry.first;
            const auto& family = entry.second;

            switch (family.kind) {
                case MetricKind::Counter:
                    ss << "# TYPE " << name << " counter\n";
                    for (const auto& pair : family.counters) {
                        const auto& counter = pair.second;
                        ss << name;
                        write_labels(ss, counter->labels());
                        ss << " " << counter->value() << "\n";
                    }
                    break;

                case MetricKind::Gauge:
                    ss << "# TYPE " << name << " gauge\n";
                    for (const auto& pair : family.gauges) {
                        const auto& gauge = pair.second;
                        ss << name;
                        write_labels(ss, gauge->labels());
                        ss << " " << gauge->value() << "\n";
                    }
                    break;

                case MetricKind::Timer:
                    ss << "# TYPE " << name << "_seconds gauge\n";
                    for (const auto& pair : family.timers) {
                        const auto& timer = pair.second;
                        ss << name << "_seconds";
                        write_labels(ss, timer->labels());
                        ss << " " << timer->last_seconds() << "\n";
                    }
                    break;
            }
        }
        return ss.str();
    }

private:
    static void write_labels(std::stringstream& ss, const std::vector<Label>& labels) {
        if (labels.empty()) {
            return;
        }
        ss << "{";
        for (std::size_t i = 0; i < labels.size(); ++i) {
            ss << labels[i].name << "=\"" << labels[i].value << "\"";
            if (i + 1 < labels.size()) {
                ss << ",";
            }
        }
        ss << "}";
    }
};

// -------------------------
// Helper: scope-based timer
// -------------------------

class ScopedTimer {
public:
    ScopedTimer(
        Registry& registry,
        std::string name,
        std::vector<Label> labels
    )
        : registry_(registry),
          name_(std::move(name)),
          labels_(std::move(labels)),
          timer_(registry_.get_or_create_timer(name_, labels_))
    {
        if (timer_) {
            timer_->reset();
        }
    }

    ~ScopedTimer() {
        if (timer_) {
            timer_->stop();
        }
    }

private:
    Registry& registry_;
    std::string name_;
    std::vector<Label> labels_;
    std::shared_ptr<Timer> timer_;
};

} // namespace praxis_metrics

#endif // INCLUDED_PROMETHEUS_PRAXIS_TELEMETRY_HPP
