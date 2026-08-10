// File: cpp/simulation/blast_radius_propagation_simulator.cpp
#include "blast_radius_kernel.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace eco {

struct BlastRadiusEdge {
    std::string from_hex;
    std::string to_hex;
    double coupling;
};

struct HexNode {
    std::string hex_id;
    double initial_surge;
};

struct SurgeResult {
    std::string hex_id;
    double worst_case_surge;
    bool requires_proactive_check;
};

class BlastRadiusSimulator {
public:
    BlastRadiusSimulator(
        const std::vector<HexNode>& nodes,
        const std::vector<BlastRadiusEdge>& edges,
        double check_threshold
    ) : check_threshold_(check_threshold) {
        if (nodes.empty() || !std::isfinite(check_threshold_) ||
            check_threshold_ < 0.0 || check_threshold_ > 1.0) {
            throw std::invalid_argument("invalid blast-radius simulator configuration");
        }

        for (std::size_t index = 0; index < nodes.size(); ++index) {
            const HexNode& node = nodes[index];
            if (node.hex_id.empty() || !std::isfinite(node.initial_surge) ||
                node.initial_surge < 0.0 || node.initial_surge > 1.0 ||
                !id_to_index_.emplace(node.hex_id, index).second) {
                throw std::invalid_argument("invalid or duplicate hex node");
            }
            hex_ids_.push_back(node.hex_id);
            initial_surge_.push_back(node.initial_surge);
        }

        adjacency_.assign(nodes.size(), std::vector<double>(nodes.size(), 0.0));
        for (const BlastRadiusEdge& edge : edges) {
            const auto from = id_to_index_.find(edge.from_hex);
            const auto to = id_to_index_.find(edge.to_hex);
            if (from == id_to_index_.end() || to == id_to_index_.end() ||
                !std::isfinite(edge.coupling) || edge.coupling < 0.0 ||
                edge.coupling > 1.0) {
                throw std::invalid_argument("invalid blast-radius edge");
            }
            adjacency_[from->second][to->second] =
                std::max(adjacency_[from->second][to->second], edge.coupling);
        }
    }

    std::vector<SurgeResult> run_simulation(
        int max_steps = 64,
        double tolerance = 1.0e-9
    ) const {
        if (max_steps <= 0 || !std::isfinite(tolerance) || tolerance <= 0.0) {
            throw std::invalid_argument("invalid convergence parameters");
        }

        std::vector<double> current = initial_surge_;
        std::vector<double> next(current.size(), 0.0);

        for (int step = 0; step < max_steps; ++step) {
            double maximum_difference = 0.0;

            for (std::size_t target = 0; target < current.size(); ++target) {
                double propagated = 0.0;
                for (std::size_t source = 0; source < current.size(); ++source) {
                    propagated += adjacency_[source][target] * current[source];
                }

                next[target] = std::clamp(
                    std::max(initial_surge_[target], propagated),
                    0.0,
                    1.0
                );
                maximum_difference = std::max(
                    maximum_difference,
                    std::abs(next[target] - current[target])
                );
            }

            current.swap(next);
            if (maximum_difference <= tolerance) {
                break;
            }
        }

        std::vector<SurgeResult> results;
        results.reserve(current.size());
        for (std::size_t index = 0; index < current.size(); ++index) {
            results.push_back({
                hex_ids_[index],
                current[index],
                current[index] >= check_threshold_
            });
        }
        return results;
    }

private:
    double check_threshold_;
    std::vector<std::string> hex_ids_;
    std::vector<double> initial_surge_;
    std::vector<std::vector<double>> adjacency_;
    std::unordered_map<std::string, std::size_t> id_to_index_;
};

std::string sql_quote(const std::string& value) {
    std::string quoted;
    quoted.reserve(value.size() + 2);
    quoted.push_back('\'');
    for (const char character : value) {
        quoted += character == '\'' ? "''" : std::string(1, character);
    }
    quoted.push_back('\'');
    return quoted;
}

void emit_proactive_checks_sql(
    const std::vector<SurgeResult>& results,
    const std::string& run_id
) {
    for (const SurgeResult& result : results) {
        if (result.requires_proactive_check) {
            std::cout << "INSERT INTO blast_radius_proactive_check "
                      << "(run_id, hex_id, worst_case_surge) VALUES ("
                      << sql_quote(run_id) << ", "
                      << sql_quote(result.hex_id) << ", "
                      << result.worst_case_surge << ");\n";
        }
    }
}

}  // namespace eco

int main(int argc, char* argv[]) {
    const double energy_j = argc > 1 ? std::strtod(argv[1], nullptr) : 120000.0;
    const std::string run_id =
        argc > 2 ? argv[2] : "blast_radius_2026_08_10";

    const BlastRadiusInput input{
        energy_j,
        250000.0,
        0.025,
        15.0,
        0.08
    };
    BlastRadiusOutput ecological_radius{};

    const int status = compute_ecological_blast_radius(&input, &ecological_radius);
    if (status != 0) {
        std::cerr << "ecological blast-radius assessment failed; status="
                  << status << '\n';
        return status;
    }

    try {
        const std::vector<eco::HexNode> nodes{
            {"hex_BR_1", 0.20},
            {"hex_BR_2", 0.00},
            {"hex_BR_3", 0.10},
            {"hex_BR_4", 0.00}
        };
        const std::vector<eco::BlastRadiusEdge> edges{
            {"hex_BR_1", "hex_BR_2", 0.50},
            {"hex_BR_2", "hex_BR_3", 0.40},
            {"hex_BR_3", "hex_BR_4", 0.30},
            {"hex_BR_1", "hex_BR_3", 0.20}
        };

        const double threshold = ecological_radius.within_corridor != 0 ? 0.15 : 0.05;
        const eco::BlastRadiusSimulator simulator(nodes, edges, threshold);
        const std::vector<eco::SurgeResult> results = simulator.run_simulation();

        std::cout << std::fixed << std::setprecision(6)
                  << "energy_j=" << input.energy_j << '\n'
                  << "ecological_radius_m=" << ecological_radius.radius_m << '\n'
                  << "normalized_energy_risk="
                  << ecological_radius.normalized_energy_risk << '\n'
                  << "eco_impact_value=" << ecological_radius.eco_impact_value << '\n'
                  << "within_corridor="
                  << static_cast<int>(ecological_radius.within_corridor) << '\n'
                  << "surge_threshold=" << threshold << '\n';

        for (const eco::SurgeResult& result : results) {
            std::cout << "hex_id=" << result.hex_id
                      << " worst_case_surge=" << result.worst_case_surge
                      << " proactive_check="
                      << (result.requires_proactive_check ? "1" : "0") << '\n';
        }
        eco::emit_proactive_checks_sql(results, run_id);
    } catch (const std::exception& error) {
        std::cerr << "simulation configuration failed: " << error.what() << '\n';
        return 3;
    }

    return ecological_radius.within_corridor != 0 ? 0 : 2;
}
