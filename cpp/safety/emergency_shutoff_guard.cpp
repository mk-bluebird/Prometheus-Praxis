// File: cpp/safety/emergency_shutoff_guard.cpp
#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <iomanip>
#include <cmath>

// emergency_shutoff_guard:
// - Monitors hardware watchdog (simulated) and sensor sanity.
// - Tracks eco-impact values (e.g., water quality index, air quality, soil safety).
// - If any eco-impact metric drops below a critical threshold, it opens a relay
//   to stop non-essential machinery.
// - Designed for integration on industrial controllers in Prometheus-Praxis.

namespace eco {

struct EcoImpactMetric {
    std::string name;      // e.g., "water_quality_index"
    double value;          // 0..1 (higher is safer)
    double critical_min;   // critical minimum safety threshold
};

class HardwareWatchdog {
public:
    HardwareWatchdog()
        : last_heartbeat_(std::chrono::steady_clock::now()),
          timeout_ms_(5000) {}

    void heartbeat() {
        last_heartbeat_ = std::chrono::steady_clock::now();
    }

    bool is_alive() const {
        auto now = std::chrono::steady_clock::now();
        auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_heartbeat_).count();
        return diff < timeout_ms_;
    }

private:
    std::chrono::steady_clock::time_point last_heartbeat_;
    int timeout_ms_;
};

class EmergencyRelay {
public:
    EmergencyRelay() : open_(false) {}

    void open() {
        if (!open_) {
            open_ = true;
            std::cout << "[EmergencyRelay] Relay OPENED: non-essential machinery STOPPED.\n";
        }
    }

    void close() {
        if (open_) {
            open_ = false;
            std::cout << "[EmergencyRelay] Relay CLOSED: machinery allowed.\n";
        }
    }

    bool is_open() const { return open_; }

private:
    bool open_;
};

class SensorSanityChecker {
public:
    bool is_sane(const std::vector<EcoImpactMetric>& metrics) const {
        for (const auto& m : metrics) {
            if (std::isnan(m.value) || m.value < 0.0 || m.value > 1.0) {
                std::cout << "[SensorSanityChecker] Metric " << m.name
                          << " out of bounds: " << m.value << "\n";
                return false;
            }
        }
        return true;
    }
};

class EmergencyShutoffGuard {
public:
    EmergencyShutoffGuard()
        : watchdog_(), relay_(), sanity_checker_() {}

    void run_guard_loop(int cycles, double interval_seconds) {
        std::cout << "Starting emergency shutoff guard.\n";

        for (int i = 0; i < cycles; ++i) {
            watchdog_.heartbeat();

            std::vector<EcoImpactMetric> metrics = read_current_metrics(i);

            bool sane = sanity_checker_.is_sane(metrics);
            bool watchdog_ok = watchdog_.is_alive();
            bool critical = has_critical_drop(metrics);

            if (!sane || !watchdog_ok || critical) {
                relay_.open();
            } else {
                // For safety, we may keep relay open until manual reset,
                // but here we close if everything is stable.
                relay_.close();
            }

            log_metrics(metrics, critical);

            std::this_thread::sleep_for(
                std::chrono::milliseconds(static_cast<int>(interval_seconds * 1000))
            );
        }

        std::cout << "Emergency shutoff guard loop complete.\n";
    }

private:
    HardwareWatchdog    watchdog_;
    EmergencyRelay      relay_;
    SensorSanityChecker sanity_checker_;

    static std::vector<EcoImpactMetric> read_current_metrics(int cycle_idx) {
        // Simulated eco-impact metrics: water, air, soil.
        // We introduce a contamination spike at cycle 5.
        std::vector<EcoImpactMetric> metrics;
        double water_quality = 0.9 - 0.05 * cycle_idx;
        double air_quality   = 0.8;
        double soil_safety   = 0.85;

        if (cycle_idx == 5) {
            water_quality = 0.2; // contamination spike
        }

        metrics.push_back(EcoImpactMetric{"water_quality_index", water_quality, 0.5});
        metrics.push_back(EcoImpactMetric{"air_quality_index",   air_quality,   0.4});
        metrics.push_back(EcoImpactMetric{"soil_safety_index",   soil_safety,   0.4});

        return metrics;
    }

    static bool has_critical_drop(const std::vector<EcoImpactMetric>& metrics) {
        for (const auto& m : metrics) {
            if (m.value < m.critical_min) {
                std::cout << "[EmergencyShutoffGuard] Critical drop detected in "
                          << m.name << ": " << m.value
                          << " < " << m.critical_min << "\n";
                return true;
            }
        }
        return false;
    }

    static void log_metrics(const std::vector<EcoImpactMetric>& metrics, bool critical) {
        std::cout << std::fixed << std::setprecision(3);
        std::cout << "[EcoImpact] ";
        for (const auto& m : metrics) {
            std::cout << m.name << "=" << m.value << " ";
        }
        std::cout << " status=" << (critical ? "CRITICAL" : "OK") << "\n";
    }
};

} // namespace eco

int main() {
    using namespace eco;

    EmergencyShutoffGuard guard;
    guard.run_guard_loop(10, 1.0);

    return 0;
}
