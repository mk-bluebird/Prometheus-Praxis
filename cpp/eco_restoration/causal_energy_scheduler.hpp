// File: cpp/eco_restoration/causal_energy_scheduler.hpp
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace eco_restoration {

struct CausalVariable {
    std::string name;
    int temporal_tier{};
    bool immutable_historical{};
};

struct DirectedCausalEdge {
    std::size_t source{};
    std::size_t target{};
    bool required{};
    bool forbidden{};
};

struct CausalDiscoverySummary {
    std::size_t permitted_edges{};
    std::size_t prohibited_edges{};
    double knowledge_factor{};
    double eco_impact_value{};
};

/*
For X_i=f_i(Pa(X_i))+epsilon_i, only retain candidate directions consistent
with temporal order and declared immutable-history variables. Specifically,
no restoration or present-time intervention may point to historical climate.
Constraint-based tests determine adjacencies and equivalence classes only
within this admissible graph space.
*/
inline bool causal_edge_allowed(const CausalVariable& source,
                                const CausalVariable& target) {
    return !target.immutable_historical &&
           source.temporal_tier <= target.temporal_tier;
}

inline CausalDiscoverySummary validate_causal_background(
    const std::vector<CausalVariable>& variables,
    const std::vector<DirectedCausalEdge>& candidate_edges) {

    if (variables.empty()) throw std::invalid_argument("causal variables are required");

    std::size_t permitted = 0;
    std::size_t prohibited = 0;
    for (const auto& edge : candidate_edges) {
        if (edge.source >= variables.size() || edge.target >= variables.size() ||
            edge.source == edge.target) {
            throw std::invalid_argument("invalid directed causal edge");
        }

        const bool allowed = causal_edge_allowed(
            variables[edge.source], variables[edge.target]);
        if (edge.forbidden || !allowed) {
            ++prohibited;
            if (edge.required && !allowed) {
                throw std::runtime_error("required edge violates causal background knowledge");
            }
        } else {
            ++permitted;
        }
    }

    const double total = static_cast<double>(
        std::max<std::size_t>(candidate_edges.size(), 1));
    const double coverage = static_cast<double>(prohibited) / total;
    return {permitted, prohibited,
            std::clamp(0.60 + 0.40 * coverage, 0.0, 1.0),
            std::clamp(0.45 + 0.55 * coverage, 0.0, 1.0)};
}

enum class InferenceAction {
    Defer,
    InferLowPower,
    InferFullPower
};

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

struct EdgeDecision {
    InferenceAction action{InferenceAction::Defer};
    double next_energy_j{};
    double latency_s{};
    double immediate_cost{};
    bool feasible{};
};

/*
MDP state: (battery energy, harvested energy forecast, task age, network cost).
Action: defer, low-power inference, or full-power inference.
Transition:
E_(t+1)=min(E_capacity,E_t+eta*h_t-e_action),
with E_(t+1)>=E_min for every feasible action.
*/
inline EdgeDecision choose_energy_safe_action(
    const EdgeMdpState& state, const EdgeMdpParameters& parameters,
    double harvest_efficiency) {

    if (!(parameters.battery_capacity_j > 0.0 &&
          parameters.energy_minimum_j >= 0.0 &&
          parameters.energy_minimum_j <= parameters.battery_capacity_j &&
          harvest_efficiency >= 0.0)) {
        throw std::invalid_argument("invalid energy-aware scheduling parameters");
    }

    struct Candidate {
        InferenceAction action;
        double energy_cost;
        double latency_s;
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
        const double next_energy = std::min(
            parameters.battery_capacity_j,
            state.battery_j + harvest_efficiency * state.harvested_energy_j -
            candidate.energy_cost);
        if (next_energy < parameters.energy_minimum_j) continue;

        const double cost = state.network_cost * candidate.energy_cost +
            0.08 * candidate.latency_s + candidate.service_penalty;
        if (cost < best.immediate_cost) {
            best = {candidate.action, next_energy, candidate.latency_s, cost, true};
        }
    }
    return best;
}

inline const char* inference_action_name(InferenceAction action) {
    switch (action) {
        case InferenceAction::Defer: return "defer";
        case InferenceAction::InferLowPower: return "infer_low_power";
        case InferenceAction::InferFullPower: return "infer_full_power";
    }
    return "unknown";
}

}  // namespace eco_restoration
