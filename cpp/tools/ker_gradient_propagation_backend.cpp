// File: cpp/tools/ker_gradient_propagation_backend.cpp
#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <queue>
#include <iomanip>

// This backend computes ∂s_m / ∂s_d for KER scalars along a dependency graph.
// It is designed to feed a REST API that serves gradient propagation data to a dashboard.

namespace eco {

struct DependencyEdge {
    std::string from_module; // dependency / donor
    std::string to_module;   // dependent / recipient
    double influence;        // local influence factor (linearized ∂s_to/∂s_from in [0,1])
};

struct KerScalar {
    std::string module_id;
    double s; // KER scalar for the module
};

struct GradientEntry {
    std::string dependent_module;   // m
    std::string donor_module;       // d
    double ds_m_over_ds_d;          // ∂s_m / ∂s_d
};

class KerGradientGraph {
public:
    void add_module(const std::string& id, double s) {
        if (id_to_index.find(id) == id_to_index.end()) {
            int idx = static_cast<int>(modules.size());
            id_to_index[id] = idx;
            modules.push_back({id, s});
            adj.emplace_back();
        }
    }

    void add_dependency(const DependencyEdge& e) {
        int from = index_of(e.from_module);
        int to   = index_of(e.to_module);
        if (from < 0 || to < 0) return;
        adj[from].push_back({to, e.influence});
    }

    // Compute ∂s_m/∂s_d for all pairs via forward influence propagation.
    std::vector<GradientEntry> compute_gradients() const {
        std::vector<GradientEntry> out;
        int n = static_cast<int>(modules.size());

        // For each donor d, propagate influence through graph.
        for (int d = 0; d < n; ++d) {
            std::vector<double> grad(n, 0.0);
            grad[d] = 1.0; // ∂s_d/∂s_d = 1

            std::queue<int> q;
            q.push(d);

            while (!q.empty()) {
                int u = q.front();
                q.pop();
                double gu = grad[u];

                for (const auto& edge : adj[u]) {
                    int v = edge.first;
                    double influ = edge.second;
                    double new_grad = gu * influ;
                    if (new_grad <= 0.0) continue;
                    if (new_grad > grad[v] + 1e-12) {
                        grad[v] = new_grad;
                        q.push(v);
                    }
                }
            }

            for (int m = 0; m < n; ++m) {
                if (m == d) continue;
                if (grad[m] > 0.0) {
                    GradientEntry ge{};
                    ge.dependent_module = modules[m].module_id;
                    ge.donor_module = modules[d].module_id;
                    ge.ds_m_over_ds_d = grad[m];
                    out.push_back(ge);
                }
            }
        }
        return out;
    }

    // Emit JSON-like output suitable for a REST API response.
    void print_gradients_json(const std::vector<GradientEntry>& grads) const {
        std::cout << "{ \"ker_gradient_propagation\": [\n";
        for (std::size_t i = 0; i < grads.size(); ++i) {
            const auto& g = grads[i];
            std::cout << "  { "
                      << "\"dependent_module\": \"" << g.dependent_module << "\", "
                      << "\"donor_module\": \"" << g.donor_module << "\", "
                      << "\"ds_m_over_ds_d\": " << std::setprecision(6) << g.ds_m_over_ds_d
                      << " }";
            if (i + 1 < grads.size()) std::cout << ",";
            std::cout << "\n";
        }
        std::cout << "] }\n";
    }

private:
    int index_of(const std::string& id) const {
        auto it = id_to_index.find(id);
        if (it == id_to_index.end()) return -1;
        return it->second;
    }

    std::vector<KerScalar> modules;
    std::unordered_map<std::string,int> id_to_index;
    std::vector<std::vector<std::pair<int,double>>> adj;
};

} // namespace eco

int main() {
    using namespace eco;

    KerGradientGraph graph;
    // Example modules and KER scalars (would be read from module_ker_profile).
    graph.add_module("module_A", 0.35);
    graph.add_module("module_B", 0.45);
    graph.add_module("module_C", 0.50);
    graph.add_module("module_D", 0.25);

    // Example dependencies (from synapse_endpoint or dependency table).
    graph.add_dependency({"module_A", "module_B", 0.6});
    graph.add_dependency({"module_B", "module_C", 0.7});
    graph.add_dependency({"module_A", "module_D", 0.3});
    graph.add_dependency({"module_D", "module_C", 0.4});

    auto grads = graph.compute_gradients();
    graph.print_gradients_json(grads);

    return 0;
}
