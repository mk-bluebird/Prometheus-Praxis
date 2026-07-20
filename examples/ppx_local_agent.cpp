// filename: examples/ppx_local_agent.cpp
// destination: github.com/mk-bluebird/Prometheus-Praxis/examples/ppx_local_agent.cpp
//
// Local agent skeleton with Prometheus-style telemetry.
// - Simulates a small diagnostic agent handling "tasks".
// - Instruments loop metrics using praxis_metrics::Registry.
// - Uses ppx::ScopedTimer and ppx::TelemetryMap to log structured metadata.
//
// Build example:
//   g++ -std=c++17 -I../cpp/core -I../cpp/telemetry ppx_local_agent.cpp -o ppx_local_agent

#include "../cpp/core/ppx_utils.hpp"
#include "../cpp/telemetry/ppx_telemetry.hpp"

#include <iostream>
#include <random>

struct Task {
    int id;
    std::string kind;     // e.g. "EcoRestoration", "SmartCityUpgrade"
    std::string severity; // e.g. "low", "medium", "high"
};

class LocalAgent {
public:
    LocalAgent()
        : rng_(std::random_device{}()),
          dist_ms_(50, 250) {}

    void run(std::size_t iterations) {
        using namespace praxis_metrics;
        using ppx::ScopedTimer;

        Registry registry;

        auto loop_counter = registry.get_or_create_counter(
            "ppx_agent_loop_iterations_total",
            { Label{"agent", "local"}, Label{"scope", "diagnostic"} }
        );

        auto task_counter = registry.get_or_create_counter(
            "ppx_agent_tasks_processed_total",
            { Label{"agent", "local"} }
        );

        auto error_counter = registry.get_or_create_counter(
            "ppx_agent_errors_total",
            { Label{"agent", "local"} }
        );

        for (std::size_t i = 0; i < iterations; ++i) {
            ScopedTimer timer("LocalAgent iteration");

            loop_counter->increment(1.0);

            Task t = generate_task(static_cast<int>(i));
            bool ok = handle_task(t);

            if (ok) {
                task_counter->increment(1.0);
            } else {
                error_counter->increment(1.0);
            }
        }

        std::cout << "=== Agent Metrics ===\n";
        std::cout << registry.serialize_to_text() << "\n";
    }

private:
    std::mt19937 rng_;
    std::uniform_int_distribution<int> dist_ms_;

    Task generate_task(int id) {
        Task t;
        t.id = id;
        t.kind = (id % 2 == 0) ? "EcoRestoration" : "SmartCityUpgrade";
        t.severity = (id % 3 == 0) ? "high" : "low";
        return t;
    }

    bool handle_task(const Task& t) {
        ppx::TelemetryMap tm;
        tm["task_id"] = std::to_string(t.id);
        tm["kind"] = t.kind;
        tm["severity"] = t.severity;
        tm["timestamp"] = ppx::utc_timestamp_iso8601();

        // Simulate variable work duration.
        int work_ms = dist_ms_(rng_);
        busy_wait(work_ms);

        bool ok = (t.severity != "high"); // trivial condition; real logic would be more complex.

        std::cout << "[ppx::LocalAgent] handled task: "
                  << ppx::telemetry_to_json(tm)
                  << " ok=" << (ok ? "true" : "false")
                  << "\n";

        return ok;
    }

    void busy_wait(int ms) {
        auto start = std::chrono::steady_clock::now();
        while (true) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed =
                std::chrono::duration_cast<std::chrono::milliseconds>(now - start)
                    .count();
            if (elapsed >= ms) {
                break;
            }
        }
    }
};

int main() {
    LocalAgent agent;
    agent.run(10);
    return 0;
}
