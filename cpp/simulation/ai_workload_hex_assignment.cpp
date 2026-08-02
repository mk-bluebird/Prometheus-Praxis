// File: cpp/simulation/ai_workload_hex_assignment.cpp

#include <vector>
#include <string>
#include <iostream>
#include <random>
#include <cmath>
#include <algorithm>

/**
 * AI workload structuring for eco-restoration across a Phoenix hex grid.
 *
 * We model a simplified multi-agent reinforcement learning architecture
 * where each hex h is an agent node in a graph; actions are intervention
 * choices: TREE, ROOF, WATER, or NONE. Calibrated α, β, γ are embedded in
 * the reward signal to align agents with cooling-leverage optimization.[82][86][92][95]
 *
 * The adjacency constraints of hex anchors are respected by constructing
 * a hex graph with edges between neighboring hexes (q,r axial coordinates),
 * analogous to a GNN-based multi-agent topology. In a real system, this
 * would feed into a graph neural network (GNN) that learns policies across
 * the hex graph; here we encode the core data structures and a simple
 * synchronous update loop that demonstrates how reward calculation would
 * rely on α, β, γ and adjacency-aware coordination.
 */

enum class InterventionAction {
    NONE,
    TREE,
    ROOF,
    WATER
};

struct HexNode {
    std::string hex_id;
    int q;
    int r;
    double current_uhi;
    double ndvi;
    double roof_index;
    double water_index;
    InterventionAction action;
};

struct HexEdge {
    int from_index;
    int to_index;
};

struct CoolingCoefficients {
    double alpha_hat;
    double beta_hat;
    double gamma_hat;
};

struct MARLConfig {
    double learning_rate;
    double discount_factor;
    double adjacency_weight;
    double exploration_eps;
};

struct AgentState {
    double value; // simple scalar value estimate for each action state
    InterventionAction action;
};

class HexGraphEnvironment {
public:
    HexGraphEnvironment(const std::vector<HexNode>& nodes,
                        const std::vector<HexEdge>& edges,
                        const CoolingCoefficients& coeffs,
                        const MARLConfig& cfg)
        : nodes_(nodes), edges_(edges), coeffs_(coeffs), cfg_(cfg) {
        init_agents();
    }

    // One simulation step: agents select actions, environment computes rewards,
    // and updates agent value estimates. This is a very simplified stand-in for
    // a federated GNN-based MARL training loop.
    void step() {
        select_actions();
        std::vector<double> rewards = compute_rewards();
        update_values(rewards);
    }

    void print_state() const {
        std::cout << "HexGraph state:\n";
        for (std::size_t i = 0; i < nodes_.size(); ++i) {
            const auto& n = nodes_[i];
            std::cout << "Hex " << n.hex_id
                      << " (q=" << n.q << ", r=" << n.r << ")"
                      << " | UHI=" << n.current_uhi
                      << " | ndvi=" << n.ndvi
                      << " | roof=" << n.roof_index
                      << " | water=" << n.water_index
                      << " | action=" << action_to_string(n.action)
                      << " | value=" << agents_[i].value
                      << "\n";
        }
    }

private:
    std::vector<HexNode> nodes_;
    std::vector<HexEdge> edges_;
    CoolingCoefficients coeffs_;
    MARLConfig cfg_;
    std::vector<AgentState> agents_;
    std::mt19937 rng_;

    void init_agents() {
        rng_.seed(42u);
        agents_.resize(nodes_.size());
        for (auto& a : agents_) {
            a.value = 0.0;
            a.action = InterventionAction::NONE;
        }
    }

    void select_actions() {
        std::uniform_real_distribution<double> uni(0.0, 1.0);
        for (std::size_t i = 0; i < nodes_.size(); ++i) {
            double roll = uni(rng_);
            if (roll < cfg_.exploration_eps) {
                // Exploration: random action.
                int choice = static_cast<int>(std::floor(uni(rng_) * 4.0));
                nodes_[i].action = static_cast<InterventionAction>(choice);
            } else {
                // Exploitation: pick action aligned with agent's value sign and cooling leverage.
                nodes_[i].action = choose_greedy_action(nodes_[i], agents_[i]);
            }
        }
    }

    InterventionAction choose_greedy_action(const HexNode& node, const AgentState& state) const {
        // Use a simple heuristic: choose the intervention with largest instantaneous
        // cooling leverage contribution based on calibrated α, β, γ and local metrics.
        double tree_gain  = coeffs_.alpha_hat * potential_tree_increment(node);
        double roof_gain  = coeffs_.beta_hat  * potential_roof_increment(node);
        double water_gain = coeffs_.gamma_hat * potential_water_increment(node);

        // For now, we ignore NONE in greedy choice unless all gains are non-positive.
        double best_gain = tree_gain;
        InterventionAction best = InterventionAction::TREE;

        if (roof_gain < best_gain) { // note: α, γ usually negative, β positive; we want most cooling (more negative).
            best_gain = roof_gain;
            best = InterventionAction::ROOF;
        }
        if (water_gain < best_gain) {
            best_gain = water_gain;
            best = InterventionAction::WATER;
        }

        if (best_gain >= 0.0) {
            return InterventionAction::NONE;
        }
        return best;
    }

    double potential_tree_increment(const HexNode& node) const {
        // Available tree increment could depend on existing NDVI and space constraints.
        // Here we assume hexs with lower NDVI have higher potential.
        double max_inc = 0.20;
        double inc = max_inc * (1.0 - node.ndvi);
        if (inc < 0.0) inc = 0.0;
        return inc;
    }

    double potential_roof_increment(const HexNode& node) const {
        // Roof potential: more built-up (higher roof_index) means more cool-roof opportunity.
        double max_inc = -0.15; // negative because cool roofs reduce warming contribution β.
        double scale = std::min(1.0, node.roof_index);
        return max_inc * scale;
    }

    double potential_water_increment(const HexNode& node) const {
        double max_inc = 0.10;
        double inc = max_inc * (1.0 - node.water_index);
        if (inc < 0.0) inc = 0.0;
        return inc;
    }

    std::vector<double> compute_rewards() const {
        std::vector<double> rewards(nodes_.size(), 0.0);

        // Per-hex cooling reward using calibrated α, β, γ and chosen action.
        for (std::size_t i = 0; i < nodes_.size(); ++i) {
            const HexNode& node = nodes_[i];
            double cooling_gain = 0.0;
            switch (node.action) {
                case InterventionAction::TREE:
                    cooling_gain = coeffs_.alpha_hat * potential_tree_increment(node);
                    break;
                case InterventionAction::ROOF:
                    cooling_gain = coeffs_.beta_hat * potential_roof_increment(node);
                    break;
                case InterventionAction::WATER:
                    cooling_gain = coeffs_.gamma_hat * potential_water_increment(node);
                    break;
                case InterventionAction::NONE:
                    cooling_gain = 0.0;
                    break;
            }

            // Adjacency-based reward: encourage spatial coherence and avoid conflicting interventions.
            double adjacency_bonus = compute_adjacency_bonus(i, node.action);

            // Total reward: negative cooling_gain (since cooling_gain is ΔT; more negative is better),
            // plus adjacency bonus.
            rewards[i] = -cooling_gain + cfg_.adjacency_weight * adjacency_bonus;
        }

        return rewards;
    }

    double compute_adjacency_bonus(std::size_t node_index, InterventionAction action) const {
        double bonus = 0.0;
        for (const auto& e : edges_) {
            if (e.from_index == static_cast<int>(node_index)) {
                const HexNode& nb = nodes_[static_cast<std::size_t>(e.to_index)];
                if (nb.action == action && action != InterventionAction::NONE) {
                    // Reward coordinated cooling corridors (same intervention across neighbors).
                    bonus += 1.0;
                }
            }
        }
        return bonus;
    }

    void update_values(const std::vector<double>& rewards) {
        for (std::size_t i = 0; i < nodes_.size(); ++i) {
            // Simple TD(0)-like update with no state transitions for demonstration.
            agents_[i].value += cfg_.learning_rate * (rewards[i] - agents_[i].value);
        }
    }

    static std::string action_to_string(InterventionAction a) {
        switch (a) {
            case InterventionAction::NONE:  return "NONE";
            case InterventionAction::TREE:  return "TREE";
            case InterventionAction::ROOF:  return "ROOF";
            case InterventionAction::WATER: return "WATER";
        }
        return "UNKNOWN";
    }
};

// Example: build a small hex graph and run a few steps of the workload assignment.
int main() {
    std::vector<HexNode> nodes(4);
    nodes[0] = {"hex_10_20", 10, 20, 6.0, 0.25, 0.8, 0.10, InterventionAction::NONE};
    nodes[1] = {"hex_11_20", 11, 20, 5.5, 0.35, 0.7, 0.05, InterventionAction::NONE};
    nodes[2] = {"hex_10_21", 10, 21, 6.2, 0.20, 0.9, 0.08, InterventionAction::NONE};
    nodes[3] = {"hex_11_21", 11, 21, 5.8, 0.30, 0.6, 0.12, InterventionAction::NONE};

    // Hex adjacency edges (simplified rectangular approximation for this demo).
    std::vector<HexEdge> edges = {
        {0, 1}, {0, 2},
        {1, 0}, {1, 3},
        {2, 0}, {2, 3},
        {3, 1}, {3, 2}
    };

    CoolingCoefficients coeffs;
    coeffs.alpha_hat = -8.0;
    coeffs.beta_hat  = 3.0;
    coeffs.gamma_hat = -5.0;

    MARLConfig cfg;
    cfg.learning_rate = 0.1;
    cfg.discount_factor = 0.95;
    cfg.adjacency_weight = 0.5;
    cfg.exploration_eps = 0.2;

    HexGraphEnvironment env(nodes, edges, coeffs, cfg);

    for (int t = 0; t < 10; ++t) {
        std::cout << "Step " << t << ":\n";
        env.step();
        env.print_state();
        std::cout << "-----------------------\n";
    }

    return 0;
}
