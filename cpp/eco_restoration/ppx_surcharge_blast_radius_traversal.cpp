// File: cpp/eco_restoration/ppx_surcharge_blast_radius_traversal.cpp
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ppx::eco_restoration {

struct CanalNode {
    std::int64_t node_id{};
    std::int64_t hex_anchor{};
    double elevation_m{};
};

struct CanalEdge {
    std::int64_t to_node_id{};
    double friction_head_m{};
};

struct TraversalResult {
    std::vector<std::int64_t> hex_anchors;
    std::size_t visited_nodes{};
    double elapsed_ms{};
};

TraversalResult surcharge_blast_radius(
    const std::unordered_map<std::int64_t, CanalNode>& nodes,
    const std::unordered_map<std::int64_t, std::vector<CanalEdge>>& edges,
    std::int64_t breach_node_id,
    double initial_head_m) {
    const auto root = nodes.find(breach_node_id);
    if (root == nodes.end() || initial_head_m < 0.0) return {};

    struct WorkItem { std::int64_t node_id; double remaining_head_m; };
    std::queue<WorkItem> queue;
    std::unordered_set<std::int64_t> visited;
    std::unordered_set<std::int64_t> anchors;
    queue.push({breach_node_id, initial_head_m});

    const auto start = std::chrono::steady_clock::now();
    while (!queue.empty()) {
        const WorkItem current = queue.front();
        queue.pop();
        if (!visited.insert(current.node_id).second) continue;

        const CanalNode& current_node = nodes.at(current.node_id);
        anchors.insert(current_node.hex_anchor);
        const auto outgoing = edges.find(current.node_id);
        if (outgoing == edges.end()) continue;

        for (const CanalEdge& edge : outgoing->second) {
            const auto next = nodes.find(edge.to_node_id);
            if (next == nodes.end()) continue;
            const double elevation_loss =
                std::max(0.0, next->second.elevation_m - current_node.elevation_m);
            const double remaining = current.remaining_head_m - edge.friction_head_m - elevation_loss;
            if (remaining > 0.0 && !visited.contains(edge.to_node_id)) {
                queue.push({edge.to_node_id, remaining});
            }
        }
    }

    const auto finish = std::chrono::steady_clock::now();
    return {
        std::vector<std::int64_t>(anchors.begin(), anchors.end()),
        visited.size(),
        std::chrono::duration<double, std::milli>(finish - start).count()
    };
}

}  // namespace ppx::eco_restoration
