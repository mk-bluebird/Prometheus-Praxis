// filename: examples/ppx_observability_demo.cpp
// destination: github.com/mk-bluebird/Prometheus-Praxis/examples/ppx_observability_demo.cpp
//
// Example: end-to-end observability and diff analysis.
// - Instruments a simple local "agent" loop with metrics.
// - Parses a small unified diff payload.
// - Prints Prometheus-style metrics text plus a summary of file changes.
//
// Build (example):
//   g++ -std=c++17 -I../cpp/telemetry -I../cpp/diff ppx_observability_demo.cpp -o ppx_observability_demo

#include "../cpp/telemetry/ppx_telemetry.hpp"
#include "../cpp/diff/ppx_diff_analyzer.hpp"

#include <iostream>

int main() {
    using namespace praxis_metrics;
    using namespace praxis_analysis;

    Registry reg;

    // Metric: number of analyzed diffs.
    auto diffs_counter = reg.get_or_create_counter(
        "ppx_analyzed_diffs_total",
        { Label{"agent", "local-demo"}, Label{"scope", "repository"} }
    );

    // Metric: total added lines.
    auto added_lines_counter = reg.get_or_create_counter(
        "ppx_added_lines_total",
        { Label{"agent", "local-demo"} }
    );

    // Metric: total deleted lines.
    auto deleted_lines_counter = reg.get_or_create_counter(
        "ppx_deleted_lines_total",
        { Label{"agent", "local-demo"} }
    );

    // Example unified diff payload (normally from `git diff`).
    const std::string patch_payload =
        "--- a/src/example.cpp\n"
        "+++ b/src/example.cpp\n"
        "@@ -1,3 +1,4 @@\n"
        " int main() {\n"
        "-    return 0;\n"
        "+    int x = 1;\n"
        "+    return x;\n"
        " }\n";

    auto deltas = NativeDiffParser::analyze_unified_patch(patch_payload);

    // Update metrics from parsed deltas.
    for (const auto& delta : deltas) {
        diffs_counter->increment(1.0);
        for (const auto& hunk : delta.hunks) {
            for (const auto& line : hunk.active_lines) {
                if (line.type == ChangeType::ADDITION) {
                    added_lines_counter->increment(1.0);
                } else if (line.type == ChangeType::DELETION) {
                    deleted_lines_counter->increment(1.0);
                }
            }
        }
    }

    std::cout << "=== Prometheus Metrics Text ===\n";
    std::cout << reg.serialize_to_text() << "\n";

    std::cout << "=== Diff Summary ===\n";
    for (const auto& delta : deltas) {
        std::cout << "Origin: " << delta.origin_file << "\n";
        std::cout << "Target: " << delta.target_file << "\n";
        for (const auto& hunk : delta.hunks) {
            std::cout << "  Hunk: -"
                      << hunk.old_start << "," << hunk.old_count
                      << " +"
                      << hunk.new_start << "," << hunk.new_count << "\n";
            for (const auto& line : hunk.active_lines) {
                std::cout << "    ";
                if (line.type == ChangeType::CONTEXT) {
                    std::cout << " ";
                } else if (line.type == ChangeType::ADDITION) {
                    std::cout << "+";
                } else if (line.type == ChangeType::DELETION) {
                    std::cout << "-";
                }
                std::cout << line.payload << "\n";
            }
        }
    }

    return 0;
}
