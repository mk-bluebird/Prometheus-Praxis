// File: cpp/eco_restoration/rare_event_cqrs.hpp
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace eco_restoration {

struct RareEventCandidate {
    std::string site_id;
    std::vector<double> posterior_event_probabilities;
    double field_travel_cost{};
    double annotation_cost{};
    bool accessible{};
};

struct AcquisitionDecision {
    std::string site_id;
    double bald_nats{};
    double cost_aware_score{};
    double knowledge_factor{};
    double eco_impact_value{};
};

inline double bernoulli_entropy(double probability) {
    const double p = std::clamp(probability, 1e-12, 1.0 - 1e-12);
    return -p * std::log(p) - (1.0 - p) * std::log(1.0 - p);
}

/*
BALD(x)=H[y|x]-E_theta[H[y|x,theta]].
For ensemble event probabilities p_m, use:
p_bar=(1/M)sum_m p_m,
BALD=H(p_bar)-(1/M)sum_m H(p_m).

The cost-aware score is BALD divided by field travel plus annotation cost.
*/
inline AcquisitionDecision select_cost_aware_bald(
    const std::vector<RareEventCandidate>& candidates,
    double minimum_event_probability) {

    if (candidates.empty() || minimum_event_probability < 0.0 ||
        minimum_event_probability > 1.0) {
        throw std::invalid_argument("invalid rare-event acquisition inputs");
    }

    AcquisitionDecision best;
    best.cost_aware_score = -std::numeric_limits<double>::infinity();

    for (const auto& candidate : candidates) {
        if (!candidate.accessible || candidate.site_id.empty() ||
            candidate.posterior_event_probabilities.empty() ||
            candidate.field_travel_cost < 0.0 || candidate.annotation_cost < 0.0) {
            continue;
        }

        double mean_probability = 0.0;
        double conditional_entropy = 0.0;
        for (double probability : candidate.posterior_event_probabilities) {
            if (probability < 0.0 || probability > 1.0) {
                throw std::invalid_argument("event probability must lie in [0,1]");
            }
            mean_probability += probability;
            conditional_entropy += bernoulli_entropy(probability);
        }

        const double members = static_cast<double>(candidate.posterior_event_probabilities.size());
        mean_probability /= members;
        conditional_entropy /= members;
        if (mean_probability < minimum_event_probability) continue;

        const double bald = std::max(0.0, bernoulli_entropy(mean_probability) - conditional_entropy);
        const double field_cost = std::max(
            1e-9, candidate.field_travel_cost + candidate.annotation_cost);
        const double score = bald / field_cost;

        if (score > best.cost_aware_score) {
            const double knowledge = std::clamp(bald / std::log(2.0), 0.0, 1.0);
            const double impact = std::clamp(
                knowledge * std::min(1.0, mean_probability / 0.20), 0.0, 1.0);
            best = {candidate.site_id, bald, score, knowledge, impact};
        }
    }

    if (!std::isfinite(best.cost_aware_score)) {
        throw std::runtime_error("no accessible rare-event candidate meets selection criteria");
    }
    return best;
}

enum class CorridorEventType {
    CorridorObserved,
    RiskAssessed,
    EcoImpactAssessed
};

struct CorridorAggregatePayload {
    double corridor_quality{};
    double risk_of_harm{};
    double knowledge_factor{};
    double eco_impact_value{};
};

struct CorridorAggregateEvent {
    std::int64_t time_unix_s{};
    std::string aggregate_id;
    CorridorEventType type{};
    CorridorAggregatePayload payload;
    std::string policy_commitment;
};

struct CorridorReadModel {
    std::string aggregate_id;
    double latest_corridor_quality{};
    double maximum_risk_of_harm{};
    double latest_knowledge_factor{};
    double latest_eco_impact_value{};
    std::size_t event_count{};
};

class CorridorAggregateStore {
public:
    void append(CorridorAggregateEvent event) {
        if (event.aggregate_id.empty() || event.policy_commitment.empty()) {
            throw std::invalid_argument("aggregate ID and policy commitment are required");
        }
        validate_unit_interval(event.payload.corridor_quality, "corridor quality");
        validate_unit_interval(event.payload.risk_of_harm, "risk of harm");
        validate_unit_interval(event.payload.knowledge_factor, "knowledge factor");
        validate_unit_interval(event.payload.eco_impact_value, "eco-impact value");

        if (!events_.empty() && event.time_unix_s < events_.back().time_unix_s) {
            throw std::invalid_argument("event times must be nondecreasing");
        }
        events_.push_back(std::move(event));
    }

    CorridorReadModel query(const std::string& aggregate_id) const {
        if (aggregate_id.empty()) throw std::invalid_argument("aggregate ID is required");
        CorridorReadModel result{aggregate_id, 0.0, 0.0, 0.0, 0.0, 0};
        for (const auto& event : events_) {
            if (event.aggregate_id != aggregate_id) continue;
            result.latest_corridor_quality = event.payload.corridor_quality;
            result.maximum_risk_of_harm = std::max(
                result.maximum_risk_of_harm, event.payload.risk_of_harm);
            result.latest_knowledge_factor = event.payload.knowledge_factor;
            result.latest_eco_impact_value = event.payload.eco_impact_value;
            ++result.event_count;
        }
        return result;
    }

    const std::vector<CorridorAggregateEvent>& events() const noexcept {
        return events_;
    }

private:
    static void validate_unit_interval(double value, const char* name) {
        if (!(value >= 0.0 && value <= 1.0)) {
            throw std::invalid_argument(std::string(name) + " must lie in [0,1]");
        }
    }

    std::vector<CorridorAggregateEvent> events_;
};

}  // namespace eco_restoration
