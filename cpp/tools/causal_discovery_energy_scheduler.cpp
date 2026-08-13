// File: cpp/tools/causal_discovery_energy_scheduler.cpp
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct CausalVariable {
    std::string name;
    int temporal_tier{};
    bool immutable_historical{};
};

struct DirectedEdge {
    std::size_t source{};
    std::size_t target{};
    bool required{};
    bool forbidden{};
};

/*
Structural equation model:
X_i=f_i(Pa(X_i))+epsilon_i.

Constraint-based discovery finds a graph G that satisfies observed conditional
independence statements X_i independent X_j | Z and all background constraints.
Use temporal tiers plus immutable-history constraints:
restoration_action -> historical_climate is prohibited,
and any edge from a later temporal tier to an earlier tier is prohibited.

The output is an equivalence class unless interventions, stronger assumptions,
or additional time structure identify a unique directed graph. Candidate edges
must therefore be marked as supported, prohibited, or unresolved rather than
presented as confirmed causal mechanisms.
*/
bool edge_allowed(const CausalVariable& source, const CausalVariable& target) {
    if (target.immutable_historical) return false;
    return source.temporal_tier <= target.temporal_tier;
}

struct CausalDiscoverySummary {
    std::size_t permitted_edges{};
    std::size_t prohibited_edges{};
    double knowledge_factor{};
    double eco_impact_value{};
};

CausalDiscoverySummary validate_causal_background(
    const std::vector<CausalVariable>& variables,
    const std::vector<DirectedEdge>& requested_edges) {

    if (variables.empty()) throw std::invalid_argument("causal graph needs variables");
    std::size_t permitted = 0;
    std::size_t prohibited = 0;

    for (const auto& edge : requested_edges) {
        if (edge.source >= variables.size() || edge.target >= variables.size() ||
            edge.source == edge.target) {
            throw std::invalid_argument("invalid causal edge");
        }
        const bool allowed = edge_allowed(variables[edge.source], variables[edge.target]);
        if (edge.forbidden || !allowed) {
            ++prohibited;
            if (edge.required && !allowed) {
                throw std::runtime_error("required edge conflicts with temporal or historical background");
            }
        } else {
            ++permitted;
        }
    }

    const double total = static_cast<double>(std::max<std::size_t>(1, requested_edges.size()));
    const double constraint_coverage = static_cast<double>(prohibited) / total;
    return {permitted, prohibited,
            std::clamp(0.60 + 0.40 * constraint_coverage, 0.0, 1.0),
            std::clamp(0.45 + 0.55 * constraint_coverage, 0.0, 1.0)};
}

enum class InferenceAction { Defer, InferLowPower, InferFullPower };

struct EdgeMdpState {
    double battery_j{};
    double harvested_energy_j{};
    double task_age_s{};
    double network_cost{};
};

struct EdgeMdpParameters {
    double battery_capacity_j{};
    double energy_minimum_j{};
    double low_power_energy_j{};
    double full_power_energy_j{};
    double low_power_latency_s{};
    double full_power_latency_s{};
    double maximum_latency_s{};
};

/*
MDP state s_t=(E_t,h_t,q_t,c_t), where E is battery energy, h is harvest
forecast/observation, q is deferred-inference age, and c is marginal cost.
Actions are {defer, low-power inference, full-power inference}. Transition:
E_(t+1)=min(E_cap,E_t+eta*h_t-a_t*e_infer), E_(t+1)>=E_min.

A feasible policy minimizes expected cumulative energy/latency cost while
requiring every selected inference action to preserve E_(t+1)>=E_min.
*/
struct EdgeDecision {
    InferenceAction action{InferenceAction::Defer};
    double next_energy_j{};
    double latency_s{};
    double immediate_cost{};
    bool feasible{};
};

EdgeDecision choose_energy_safe_action(const EdgeMdpState& state,
                                       const EdgeMdpParameters& parameters,
                                       double harvest_efficiency) {
    if (!(parameters.battery_capacity_j > 0.0 && parameters.energy_minimum_j >= 0.0 &&
          harvest_efficiency >= 0.0)) {
        throw std::invalid_argument("invalid edge-scheduling parameters");
    }

    struct Candidate {
        InferenceAction action;
        double energy_cost;
        double latency;
        double service_penalty;
    };
    const std::vector<Candidate> candidates{
        {InferenceAction::Defer, 0.0, state.task_age_s + 1.0,
         state.task_age_s >= parameters.maximum_latency_s ? 1000.0 : 1.0},
        {InferenceAction::InferLowPower, parameters.low_power_energy_j,
         parameters.low_power_latency_s, 0.25},
        {InferenceAction::InferFullPower, parameters.full_power_energy_j,
         parameters.full_power_latency_s, 0.0}
    };

    EdgeDecision best;
    best.immediate_cost = std::numeric_limits<double>::infinity();
    for (const auto& candidate : candidates) {
        const double next_energy = std::min(parameters.battery_capacity_j,
            state.battery_j + harvest_efficiency * state.harvested_energy_j -
            candidate.energy_cost);
        if (next_energy < parameters.energy_minimum_j) continue;

        const double cost = state.network_cost * candidate.energy_cost +
            0.08 * candidate.latency + candidate.service_penalty;
        if (cost < best.immediate_cost) {
            best = {candidate.action, next_energy, candidate.latency, cost, true};
        }
    }
    return best;
}

const char* action_name(InferenceAction action) {
    switch (action) {
        case InferenceAction::Defer: return "defer";
        case InferenceAction::InferLowPower: return "infer_low_power";
        case InferenceAction::InferFullPower: return "infer_full_power";
    }
    return "invalid";
}

}  // namespace

int main() {
    try {
        const std::vector<CausalVariable> variables{
            {"historical_climate", 0, true},
            {"soil_condition", 1, false},
            {"restoration_action", 2, false},
            {"canopy_temperature", 3, false},
            {"biodiversity_gain", 4, false}
        };
        const std::vector<DirectedEdge> edges{
            {0, 1, false, false},
            {0, 3, false, false},
            {2, 0, false, true},
            {2, 3, false, false},
            {3, 4, false, false}
        };
        const CausalDiscoverySummary causal = validate_causal_background(variables, edges);

        const EdgeMdpState state{12.0, 4.0, 14.0, 0.12};
        const EdgeMdpParameters parameters{20.0, 5.0, 1.6, 3.8, 1.4, 0.35, 18.0};
        const EdgeDecision decision = choose_energy_safe_action(state, parameters, 0.78);
        if (!decision.feasible) throw std::runtime_error("no energy-safe inference action");

        const double energy_margin = std::clamp(
            (decision.next_energy_j - parameters.energy_minimum_j) /
            (parameters.battery_capacity_j - parameters.energy_minimum_j), 0.0, 1.0);
        const double knowledge_factor = std::clamp(
            0.55 * causal.knowledge_factor + 0.45 * energy_margin, 0.0, 1.0);
        const double eco_impact_value = std::clamp(
            0.60 * causal.eco_impact_value + 0.40 * energy_margin, 0.0, 1.0);

        std::cout << std::fixed << std::setprecision(6)
                  << "causal_permitted_edges=" << causal.permitted_edges << '\n'
                  << "causal_prohibited_edges=" << causal.prohibited_edges << '\n'
                  << "energy_safe_action=" << action_name(decision.action) << '\n'
                  << "next_energy_j=" << decision.next_energy_j << '\n'
                  << "inference_latency_s=" << decision.latency_s << '\n'
                  << "immediate_cost=" << decision.immediate_cost << '\n'
                  << "knowledge_factor=" << knowledge_factor << '\n'
                  << "eco_impact_value=" << eco_impact_value << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "causal and energy scheduling assessment failed: "
                  << error.what() << '\n';
        return 1;
    }
}
