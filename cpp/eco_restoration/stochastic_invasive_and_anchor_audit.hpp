// File: cpp/eco_restoration/stochastic_invasive_and_anchor_audit.hpp
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace eco_restoration {

/*
For dI=b(I,u)dt+sigma(I,u)dW, with running cost l(I,u), the value function
satisfies:
0=V_t+min_u{l(I,u)+b(I,u)V_I+0.5*sigma(I,u)^2 V_II},
V(T,I)=terminal_cost(I).

This finite-difference local selector uses supplied approximations to V_I and
V_II. The safe treatment set retains only actions where expected ecological
benefit exceeds treatment cost, RoH<=0.30, and next-state abundance remains
nonnegative.
*/
struct InvasiveControlCandidate {
    double treatment_intensity{};
    double treatment_cost{};
    double expected_benefit{};
    double risk_of_harm{};
};

struct StochasticPopulationModel {
    double current_abundance{};
    double time_step{};
    double drift_growth_rate{};
    double treatment_effect{};
    double diffusion_scale{};
    double running_abundance_cost{};
    double value_gradient{};
    double value_curvature{};
};

struct StochasticControlDecision {
    double treatment_intensity{};
    double hamilton_jacobi_bellman_value{};
    double expected_next_abundance{};
    double diffusion_variance{};
    bool safe{};
    double knowledge_factor{};
    double eco_impact_value{};
};

inline StochasticControlDecision select_safe_stochastic_invasive_control(
    const StochasticPopulationModel& model,
    const std::vector<InvasiveControlCandidate>& candidates) {

    if (candidates.empty() || model.current_abundance < 0.0 ||
        model.time_step <= 0.0 || model.diffusion_scale < 0.0) {
        throw std::invalid_argument("invalid stochastic invasive-control inputs");
    }

    StochasticControlDecision best;
    best.hamilton_jacobi_bellman_value = std::numeric_limits<double>::infinity();

    for (const auto& candidate : candidates) {
        if (candidate.treatment_intensity < 0.0 || candidate.treatment_cost < 0.0 ||
            candidate.expected_benefit < 0.0 || candidate.risk_of_harm < 0.0 ||
            candidate.risk_of_harm > 1.0) {
            throw std::invalid_argument("invalid control candidate");
        }

        const double drift = model.drift_growth_rate * model.current_abundance -
            model.treatment_effect * candidate.treatment_intensity * model.current_abundance;
        const double expected_next = model.current_abundance + model.time_step * drift;
        const double sigma = model.diffusion_scale *
            std::sqrt(std::max(0.0, model.current_abundance));
        const double variance = sigma * sigma * model.time_step;
        const bool safe = candidate.expected_benefit >= candidate.treatment_cost &&
            candidate.risk_of_harm <= 0.30 && expected_next >= 0.0;
        if (!safe) continue;

        const double running_cost = model.running_abundance_cost *
            model.current_abundance + candidate.treatment_cost -
            candidate.expected_benefit;
        const double hjb = running_cost + drift * model.value_gradient +
            0.5 * sigma * sigma * model.value_curvature;
        if (hjb < best.hamilton_jacobi_bellman_value) {
            const double margin = std::clamp(
                (candidate.expected_benefit - candidate.treatment_cost) /
                std::max(1.0, candidate.expected_benefit), 0.0, 1.0);
            best = {candidate.treatment_intensity, hjb, expected_next, variance, true,
                    std::clamp(0.60 + 0.40 * margin, 0.0, 1.0),
                    std::clamp(0.50 + 0.50 * margin, 0.0, 1.0)};
        }
    }
    return best;
}

/*
Anchor audit invariant:
SAT(CommitmentValid and RoH>0.30 and Allow)=unsat.

This component treats commitment verification as an external Boolean result.
It does not implement a cryptographic primitive. Each accepted audit record is
monotonic in sequence number and must reference the previous accepted record.
A record with RoH>0.30 can never have allow=true.
*/
struct AnchorAuditRecord {
    std::uint64_t sequence{};
    std::string previous_reference;
    std::string anchor_reference;
    std::uint64_t h3_cell_id{};
    std::int32_t heat_index_fixed{};
    std::int32_t risk_of_harm_fixed{};
    bool externally_verified_commitment{};
    bool allow{};
};

struct AnchorAuditDecision {
    bool accepted{};
    bool invariant_holds{};
    double knowledge_factor{};
    double eco_impact_value{};
};

class HexAnchorAuditStore {
public:
    AnchorAuditDecision append(const AnchorAuditRecord& record) {
        if (record.anchor_reference.empty() || record.risk_of_harm_fixed < 0 ||
            record.risk_of_harm_fixed > 1'000'000) {
            throw std::invalid_argument("invalid hex-anchor audit record");
        }
        if (!records_.empty()) {
            const auto& prior = records_.back();
            if (record.sequence != prior.sequence + 1 ||
                record.previous_reference != prior.anchor_reference) {
                throw std::invalid_argument("anchor sequence or predecessor reference is invalid");
            }
        } else if (record.sequence != 1 || !record.previous_reference.empty()) {
            throw std::invalid_argument("initial anchor record must begin at sequence one");
        }

        constexpr std::int32_t risk_limit_fixed = 300'000;
        const bool unsafe = record.risk_of_harm_fixed > risk_limit_fixed;
        const bool invariant_holds = !(record.externally_verified_commitment &&
                                       unsafe && record.allow);
        const bool accepted = record.externally_verified_commitment &&
                              invariant_holds && (!unsafe || !record.allow);

        if (accepted) records_.push_back(record);
        const double risk = static_cast<double>(record.risk_of_harm_fixed) / 1'000'000.0;
        return {accepted, invariant_holds,
                accepted ? std::clamp(1.0 - risk, 0.0, 1.0) : 0.0,
                accepted && record.allow ? std::clamp(1.0 - risk, 0.0, 1.0) : 0.0};
    }

    const std::vector<AnchorAuditRecord>& records() const noexcept {
        return records_;
    }

private:
    std::vector<AnchorAuditRecord> records_;
};

}  // namespace eco_restoration
