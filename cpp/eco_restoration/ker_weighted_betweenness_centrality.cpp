// File: cpp/eco_restoration/ker_weighted_betweenness_centrality.cpp
#include <iostream>
#include <vector>
#include <string>
#include <queue>
#include <limits>
#include <unordered_map>

namespace eco {

struct SynapseEdge {
    int src;
    int dst;
    double ker_weight; // e.g. derived from module KER s, higher = more eco-significant
};

struct Graph {
    int num_nodes;
    std::vector<SynapseEdge> edges;
    std::vector<std::vector<std::pair<int,double>>> adj;

    explicit Graph(int n) : num_nodes(n), adj(n) {}

    void add_edge(int src, int dst, double ker_weight) {
        edges.push_back({src, dst, ker_weight});
        // We use ker_weight as inverse cost for shortest paths; higher KER means lower "resistance".
        double cost = 1.0 / (ker_weight > 1e-9 ? ker_weight : 1e-9);
        adj[src].push_back({dst, cost});
        adj[dst].push_back({src, cost}); // treat as undirected for centrality
    }
};

// Brandes' betweenness centrality algorithm adapted to weighted graphs.
std::vector<double> ker_weighted_betweenness(const Graph& g) {
    int n = g.num_nodes;
    std::vector<double> Cb(n, 0.0);

    std::vector<double> dist(n);
    std::vector<double> delta(n);
    std::vector<std::vector<int>> P(n);
    std::vector<int> S;
    std::vector<int> sigma(n);
    std::vector<bool> visited(n);

    for (int s = 0; s < n; ++s) {
        // Initialization per source s
        for (int i = 0; i < n; ++i) {
            dist[i] = std::numeric_limits<double>::infinity();
            sigma[i] = 0;
            P[i].clear();
        }
        dist[s] = 0.0;
        sigma[s] = 1;

        // Min-heap for Dijkstra
        using NodeDist = std::pair<double,int>;
        std::priority_queue<NodeDist, std::vector<NodeDist>, std::greater<NodeDist>> pq;
        pq.push({0.0, s});

        S.clear();

        while (!pq.empty()) {
            auto [d, v] = pq.top();
            pq.pop();
            if (d > dist[v]) continue;
            S.push_back(v);
            for (const auto& edge : g.adj[v]) {
                int w = edge.first;
                double cost = edge.second;
                double vw_dist = dist[v] + cost;
                if (vw_dist < dist[w]) {
                    dist[w] = vw_dist;
                    pq.push({dist[w], w});
                    sigma[w] = sigma[v];
                    P[w].clear();
                    P[w].push_back(v);
                } else if (std::fabs(vw_dist - dist[w]) < 1e-12) {
                    sigma[w] += sigma[v];
                    P[w].push_back(v);
                }
            }
        }

        // Accumulation
        for (int i = 0; i < n; ++i) {
            delta[i] = 0.0;
        }

        for (int idx = static_cast<int>(S.size()) - 1; idx >= 0; --idx) {
            int w = S[idx];
            for (int v : P[w]) {
                if (sigma[w] > 0) {
                    double c = (static_cast<double>(sigma[v]) / static_cast<double>(sigma[w])) * (1.0 + delta[w]);
                    delta[v] += c;
                }
            }
            if (w != s) {
                Cb[w] += delta[w];
            }
        }
    }

    return Cb;
}

// Map from module_id to node index and emit SQL updates for betweenness centrality.
void emit_betweenness_sql(const std::unordered_map<int,std::string>& module_id_map,
                          const std::vector<double>& Cb) {
    for (const auto& kv : module_id_map) {
        int node = kv.first;
        const std::string& module_id = kv.second;
        double bc = (node >= 0 && node < static_cast<int>(Cb.size())) ? Cb[node] : 0.0;

        std::cout << "UPDATE module_ker_profile "
                  << "SET betweenness_centrality = " << bc
                  << " WHERE module_id = '" << module_id << "';\n";
    }
}

} // namespace eco

int main() {
    using namespace eco;

    // Example: 4 modules as nodes, synapse_endpoint edges with KER-derived weights.
    Graph g(4);
    // In practice, these edges would be loaded from synapse_endpoint (source_module, target_module, ker_s).
    g.add_edge(0, 1, 0.9);
    g.add_edge(1, 2, 0.8);
    g.add_edge(2, 3, 0.7);
    g.add_edge(0, 3, 0.6);

    std::unordered_map<int,std::string> module_id_map;
    module_id_map[0] = "module_A";
    module_id_map[1] = "module_B";
    module_id_map[2] = "module_C";
    module_id_map[3] = "module_D";

    std::vector<double> Cb = ker_weighted_betweenness(g);
    emit_betweenness_sql(module_id_map, Cb);

    return 0;
}
