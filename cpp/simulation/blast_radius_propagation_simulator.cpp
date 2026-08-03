// File: cpp/simulation/blast_radius_propagation_simulator.cpp
#include <iostream>
#include <vector>
#include <string>
#include <limits>
#include <queue>

namespace eco {

struct BlastRadiusEdge {
    std::string from_hex;
    std::string to_hex;
    double coupling; // failure propagation strength in [0,1]
};

struct HexNode {
    std::string hex_id;
    double initial_surged; // initial surge level at this hex
};

struct SurgeResult {
    std::string hex_id;
    double worst_case_surge;
    bool requires_proactive_check;
};

class BlastRadiusSimulator {
public:
    BlastRadiusSimulator(const std::vector<HexNode>& nodes,
                         const std::vector<BlastRadiusEdge>& edges,
                         double check_threshold)
        : check_threshold(check_threshold) {
        // Map hex_id to index
        for (std::size_t i = 0; i < nodes.size(); ++i) {
            id_to_index[nodes[i].hex_id] = static_cast<int>(i);
            hex_ids.push_back(nodes[i].hex_id);
        }
        int n = static_cast<int>(nodes.size());
        surge.assign(n, 0.0);
        for (const auto& node : nodes) {
            int idx = id_to_index[node.hex_id];
            surge[idx] = node.initial_surged;
        }
        // Build adjacency matrix from blast_radius table
        adj.assign(n, std::vector<double>(n, 0.0));
        for (const auto& e : edges) {
            auto it_from = id_to_index.find(e.from_hex);
            auto it_to   = id_to_index.find(e.to_hex);
            if (it_from == id_to_index.end() || it_to == id_to_index.end()) {
                continue;
            }
            int i = it_from->second;
            int j = it_to->second;
            adj[i][j] = e.coupling;
        }
    }

    // Compute worst-case surge paths by iterating failure propagation until convergence.
    std::vector<SurgeResult> run_simulation(int max_steps = 20, double tol = 1e-6) {
        int n = static_cast<int>(surge.size());
        std::vector<double> current = surge;
        std::vector<double> next(n, 0.0);

        for (int step = 0; step < max_steps; ++step) {
            for (int j = 0; j < n; ++j) {
                // Surge at hex j is its own initial surge plus incoming propagated surges.
                double propagated = 0.0;
                for (int i = 0; i < n; ++i) {
                    propagated += adj[i][j] * current[i];
                }
                next[j] = std::max(current[j], propagated);
            }
            double max_diff = 0.0;
            for (int j = 0; j < n; ++j) {
                max_diff = std::max(max_diff, std::fabs(next[j] - current[j]));
                current[j] = next[j];
            }
            if (max_diff < tol) {
                break;
            }
        }

        std::vector<SurgeResult> results;
        results.reserve(n);
        for (int j = 0; j < n; ++j) {
            SurgeResult r{};
            r.hex_id = hex_ids[j];
            r.worst_case_surge = current[j];
            r.requires_proactive_check = (current[j] >= check_threshold);
            results.push_back(r);
        }
        return results;
    }

private:
    std::vector<std::string> hex_ids;
    std::vector<double> surge;
    std::vector<std::vector<double>> adj;
    std::unordered_map<std::string,int> id_to_index;
    double check_threshold;
};

// Emit proactive check logs for hexes exceeding threshold.
void emit_proactive_checks_sql(const std::vector<SurgeResult>& res,
                               const std::string& run_id) {
    for (const auto& r : res) {
        if (!r.requires_proactive_check) continue;
        std::cout << "INSERT INTO blast_radius_proactive_check "
                  << "(run_id, hex_id, worst_case_surge) VALUES ('"
                  << run_id << "', '"
                  << r.hex_id << "', "
                  << r.worst_case_surge << ");\n";
    }
}

} // namespace eco

int main() {
    using namespace eco;

    // Example blast_radius data: hex coupling matrix
    std::vector<HexNode> nodes = {
        {"hex_BR_1", 0.2},
        {"hex_BR_2", 0.0},
        {"hex_BR_3", 0.1},
        {"hex_BR_4", 0.0}
    };
    std::vector<BlastRadiusEdge> edges = {
        {"hex_BR_1", "hex_BR_2", 0.5},
        {"hex_BR_2", "hex_BR_3", 0.4},
        {"hex_BR_3", "hex_BR_4", 0.3},
        {"hex_BR_1", "hex_BR_3", 0.2}
    };

    double check_threshold = 0.15;
    BlastRadiusSimulator sim(nodes, edges, check_threshold);
    auto results = sim.run_simulation();

    std::string run_id = "blast_radius_2026_08_03";
    std::cout << "Blast-radius worst-case surge prediction:\n";
    for (const auto& r : results) {
        std::cout << "  " << r.hex_id << " : surge=" << r.worst_case_surge
                  << " check=" << (r.requires_proactive_check ? "YES" : "NO") << "\n";
    }
    std::cout << "\n";
    emit_proactive_checks_sql(results, run_id);

    return 0;
}
