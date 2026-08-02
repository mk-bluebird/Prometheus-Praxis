// File: cpp/eco_restoration/hex_corridor_connectivity_resilience.cpp

#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <random>
#include <queue>
#include <iostream>
#include <cmath>

/**
 * 35. Hex-corridor connectivity resilience metric.
 *
 * We model the cool-refuge network as a graph G = (H, E), where:
 *   - H: set of hex nodes.
 *   - E: adjacency edges between neighboring hexes.
 *   - UHI_h: UHI intensity per hex.
 *
 * A hex is "safe" if UHI_h ≤ UHI_safe_threshold.
 *
 * We simulate random loss of vegetation (drought/disease) by removing a
 * random set of hexes (or equivalently marking them unsafe). Connectivity
 * resilience metric R is:
 *
 *   R = (1 / N_trials) Σ_k [ N_safe_connected(k) / N_total_hex ]
 *
 * where:
 *   - For trial k, we randomly remove a fraction f_loss of hexes (or apply
 *     synthetic UHI increase due to vegetation loss).
 *   - After removal, N_safe_connected(k) counts hexes that are:
 *       * safe (UHI_h ≤ threshold), and
 *       * in the same connected component as at least one refuge hex
 *         (e.g., hexes with cooling interventions).
 *
 * This yields resilience as the expected fraction of hexes still served
 * by cool corridors under random loss.[83][87][90]
 */

struct HexNode {
    std::string hex_id;
    double UHI;
    bool is_refuge; // true if this hex has a cooling intervention / refuge
};

struct HexGraph {
    std::unordered_map<std::string, HexNode> nodes;
    std::unordered_map<std::string, std::vector<std::string>> adj;
};

struct ResilienceConfig {
    double uhi_safe_threshold;
    double loss_fraction; // fraction of hexes randomly removed
    int trials;
};

double compute_resilience(const HexGraph& graph, const ResilienceConfig& cfg) {
    if (graph.nodes.empty()) {
        return 0.0;
    }

    std::mt19937 rng(42);
    std::vector<std::string> hex_ids;
    hex_ids.reserve(graph.nodes.size());
    for (const auto& kv : graph.nodes) {
        hex_ids.push_back(kv.first);
    }

    int N_total = static_cast<int>(hex_ids.size());
    double resilience_sum = 0.0;

    for (int trial = 0; trial < cfg.trials; ++trial) {
        // 1. Randomly remove a subset of hexes.
        int n_remove = static_cast<int>(std::round(cfg.loss_fraction * N_total));
        std::unordered_set<std::string> removed;

        std::shuffle(hex_ids.begin(), hex_ids.end(), rng);
        for (int i = 0; i < n_remove && i < N_total; ++i) {
            removed.insert(hex_ids[i]);
        }

        // 2. BFS from refuge hexes on the remaining graph,
        //    counting safe hexes connected to any refuge.
        std::unordered_set<std::string> visited;
        std::queue<std::string> q;

        // Seed queue with refuge hexes that are not removed.
        for (const auto& kv : graph.nodes) {
            const auto& h = kv.second;
            if (h.is_refuge && removed.find(h.hex_id) == removed.end()) {
                visited.insert(h.hex_id);
                q.push(h.hex_id);
            }
        }

        int safe_connected = 0;

        while (!q.empty()) {
            std::string curr = q.front();
            q.pop();

            const HexNode& node = graph.nodes.at(curr);
            if (node.UHI <= cfg.uhi_safe_threshold) {
                ++safe_connected;
            }

            auto it = graph.adj.find(curr);
            if (it == graph.adj.end()) continue;

            for (const auto& neigh : it->second) {
                if (removed.find(neigh) != removed.end()) continue;
                if (visited.find(neigh) != visited.end()) continue;
                visited.insert(neigh);
                q.push(neigh);
            }
        }

        double frac = static_cast<double>(safe_connected) / static_cast<double>(N_total);
        resilience_sum += frac;
    }

    return resilience_sum / static_cast<double>(cfg.trials);
}

int main() {
    HexGraph graph;
    graph.nodes["hex_10_20"] = {"hex_10_20", 6.0, true};
    graph.nodes["hex_11_20"] = {"hex_11_20", 7.5, true};
    graph.nodes["hex_10_21"] = {"hex_10_21", 5.5, false};
    graph.nodes["hex_11_21"] = {"hex_11_21", 8.0, false};

    graph.adj["hex_10_20"] = {"hex_11_20", "hex_10_21"};
    graph.adj["hex_11_20"] = {"hex_10_20", "hex_11_21"};
    graph.adj["hex_10_21"] = {"hex_10_20", "hex_11_21"};
    graph.adj["hex_11_21"] = {"hex_11_20", "hex_10_21"};

    ResilienceConfig cfg;
    cfg.uhi_safe_threshold = 7.0;
    cfg.loss_fraction = 0.25;
    cfg.trials = 1000;

    double R = compute_resilience(graph, cfg);
    std::cout << "Connectivity resilience R ≈ " << R << "\n";

    return 0;
}
