// File: cpp/tools/eco_impact_prometheus_endpoint.cpp
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <iomanip>
#include <mutex>

// Reuse the EcoImpactEntry / EcoImpactReporter grammar from existing tools.
namespace eco {

struct EcoImpactEntry {
    std::string entity_name;
    double knowledge_factor;   // K in [0,1]
    double eco_impact_value;   // E in [0,1]
};

// Simple reporter that stores entries in memory.
class EcoImpactReporter {
public:
    void add_entry(const EcoImpactEntry& entry) {
        std::lock_guard<std::mutex> lock(mutex_);
        entries_.push_back(entry);
    }

    std::vector<EcoImpactEntry> snapshot_entries() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return entries_;
    }

private:
    mutable std::mutex mutex_;
    std::vector<EcoImpactEntry> entries_;
};

// Prometheus metrics endpoint exposing EcoImpactEntry as gauges.
// Wiring pattern:
// - Each EcoImpactEntry becomes a gauge "eco_impact_score".
// - Labels: entity_name, knowledge_factor, eco_impact.
// - Grafana agent scrapes this endpoint over HTTP.
class PrometheusEndpoint {
public:
    explicit PrometheusEndpoint(const EcoImpactReporter& reporter)
        : reporter_(reporter)
    {}

    // Render metrics in Prometheus text format to the given output stream.
    void render_metrics(std::ostream& os) const {
        auto entries = reporter_.snapshot_entries();

        os << "# HELP eco_impact_score Eco impact score per entity (0..1).\n";
        os << "# TYPE eco_impact_score gauge\n";

        for (const auto& e : entries) {
            os << "eco_impact_score"
               << "{entity_name=\"" << escape_label(e.entity_name) << "\""
               << ",knowledge_factor=\"" << format_double(e.knowledge_factor) << "\""
               << ",eco_impact=\"" << format_double(e.eco_impact_value) << "\""
               << "} "
               << format_double(e.eco_impact_value)
               << "\n";
        }
    }

private:
    const EcoImpactReporter& reporter_;

    static std::string escape_label(const std::string& s) {
        std::ostringstream oss;
        for (char c : s) {
            if (c == '\\' || c == '\"') {
                oss << '\\';
            }
            oss << c;
        }
        return oss.str();
    }

    static std::string format_double(double v) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(3) << v;
        return oss.str();
    }
};

} // namespace eco

// Minimal HTTP server stub: for a real deployment, map this to an HTTP GET handler
// (e.g., using civetweb, cpp-httplib, or another small embedded server).
int main() {
    using namespace eco;

    EcoImpactReporter reporter;
    reporter.add_entry({"Phoenix_Wash_A", 0.94, 0.88});
    reporter.add_entry({"Urban_Block_B", 0.91, 0.82});
    reporter.add_entry({"Community_Laundry_Node", 0.95, 0.79});

    PrometheusEndpoint endpoint(reporter);

    // Simulate a single scrape: in a real edge device, this would be called
    // when an HTTP client (Grafana agent / Prometheus) hits /metrics.
    endpoint.render_metrics(std::cout);

    return 0;
}
