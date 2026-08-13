// File: cpp/tools/rare_event_cqrs_assessment.cpp
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

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

double bernoulli_entropy(double probability) {
    const double p = std::clamp(probability, 1e-12, 1.0 - 1e-12);
    return -p * std::log(p) - (1.0 - p) * std::log(1.0 - p);
}

/*
BALD(x)=H[y|x]-E_theta[H[y|x,theta]].
For ensemble probabilities p_m(y=1|x):
p_bar=(1/M)sum_m p_m,
BALD=H(p_bar)-(1/M)sum_m H(p_m).

Cost-aware field selection uses:
score(x)=BALD(x)*accessibility(x)/(travel_cost+annotation_cost).
This prioritizes rare-event uncertainty that can be resolved within field
logistics, while a separate minimum rare-event probability can be imposed to
avoid acquiring only ambiguous non-events.
*/
AcquisitionDecision select_cost_aware_bald(
    const std::vector<RareEventCandidate>& candidates, double minimum_event_probability) {

    if (candidates.empty() || minimum_event_probability < 0.0 ||
        minimum_event_probability > 1.0) {
        throw std::invalid_argument("invalid active-learning inputs");
    }

    AcquisitionDecision best;
    best.cost_aware_score = -1.0;

    for (const auto& candidate : candidates) {
        if (!candidate.accessible || candidate.posterior_event_probabilities.empty() ||
            candidate.field_travel_cost < 0.0 || candidate.annotation_cost < 0.0) {
            continue;
        }

        double mean_probability = 0.0;
        double conditional_entropy = 0.0;
        for (double probability : candidate.posterior_event_probabilities) {
            if (probability < 0.0 || probability > 1.0) {
                throw std::invalid_argument("event probability lies outside [0,1]");
            }
            mean_probability += probability;
            conditional_entropy += bernoulli_entropy(probability);
        }
        const double count = static_cast<double>(candidate.posterior_event_probabilities.size());
        mean_probability /= count;
        conditional_entropy /= count;
        if (mean_probability < minimum_event_probability) continue;

        const double bald = std::max(0.0, bernoulli_entropy(mean_probability) - conditional_entropy);
        const double cost = std::max(1e-9, candidate.field_travel_cost + candidate.annotation_cost);
        const double score = bald / cost;
        if (score > best.cost_aware_score) {
            const double knowledge = std::clamp(
                bald / std::log(2.0), 0.0, 1.0);
            const double impact = std::clamp(
                knowledge * std::min(1.0, mean_probability / 0.20), 0.0, 1.0);
            best = {candidate.site_id, bald, score, knowledge, impact};
        }
    }

    if (best.cost_aware_score < 0.0) {
        throw std::runtime_error("no accessible candidate meets rare-event policy");
    }
    return best;
}

/*
Append-only aggregate event:
E_t=(time, aggregate_id, type, payload, policy_commitment).

Payloads contain only corridor, risk, knowledge, and eco-impact aggregates.
The schema explicitly excludes raw biosignals and direct biological identifiers.
CQRS projection maintains separate read models:
- corridor status by aggregate;
- maximum risk-of-harm;
- latest KER-like knowledge, eco-impact, and risk coordinates.
*/
enum class EventType {
    CorridorObserved,
    RiskAssessed,
    EcoImpactAssessed
};

struct AggregatePayload {
    double corridor_quality{};
    double risk_of_harm{};
    double knowledge_factor{};
    double eco_impact_value{};
};

struct AggregateEvent {
    std::int64_t time_unix_s{};
    std::string aggregate_id;
    EventType type{};
    AggregatePayload payload;
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

class AppendOnlyAggregateStore {
public:
    void append(AggregateEvent event) {
        if (event.aggregate_id.empty() || event.policy_commitment.empty()) {
            throw std::invalid_argument("aggregate and policy commitment are required");
        }
        validate_unit_interval(event.payload.corridor_quality, "corridor quality");
        validate_unit_interval(event.payload.risk_of_harm, "risk of harm");
        validate_unit_interval(event.payload.knowledge_factor, "knowledge factor");
        validate_unit_interval(event.payload.eco_impact_value, "eco-impact value");

        if (!events_.empty() && event.time_unix_s < events_.back().time_unix_s) {
            throw std::invalid_argument("events must be appended in nondecreasing time order");
        }
        events_.push_back(std::move(event));
    }

    CorridorReadModel query_corridor(const std::string& aggregate_id) const {
        CorridorReadModel model{aggregate_id, 0.0, 0.0, 0.0, 0.0, 0};
        for (const auto& event : events_) {
            if (event.aggregate_id != aggregate_id) continue;
            model.latest_corridor_quality = event.payload.corridor_quality;
            model.maximum_risk_of_harm = std::max(
                model.maximum_risk_of_harm, event.payload.risk_of_harm);
            model.latest_knowledge_factor = event.payload.knowledge_factor;
            model.latest_eco_impact_value = event.payload.eco_impact_value;
            ++model.event_count;
        }
        return model;
    }

private:
    static void validate_unit_interval(double value, const char* name) {
        if (!(value >= 0.0 && value <= 1.0)) {
            throw std::invalid_argument(std::string(name) + " must lie in [0,1]");
        }
    }

    std::vector<AggregateEvent> events_;
};

}  // namespace

int main() {
    try {
        const std::vector<RareEventCandidate> candidates{
            {"wash_01", {0.04, 0.26, 0.61, 0.11}, 1.8, 0.7, true},
            {"canal_07", {0.09, 0.12, 0.17, 0.13}, 0.6, 0.5, true},
            {"upland_03", {0.20, 0.58, 0.74, 0.29}, 3.5, 1.1, true},
            {"restricted_02", {0.18, 0.62, 0.51, 0.24}, 0.4, 0.3, false}
        };
        const AcquisitionDecision acquisition = select_cost_aware_bald(candidates, 0.05);

        AppendOnlyAggregateStore store;
        store.append({1760000000, acquisition.site_id, EventType::CorridorObserved,
                      {0.78, 0.12, acquisition.knowledge_factor, acquisition.eco_impact_value},
                      "eco_corridor_policy_v1"});
        store.append({1760003600, acquisition.site_id, EventType::RiskAssessed,
                      {0.78, 0.18, 0.82, 0.71}, "eco_corridor_policy_v1"});
        store.append({1760007200, acquisition.site_id, EventType::EcoImpactAssessed,
                      {0.81, 0.16, 0.88, 0.76}, "eco_corridor_policy_v1"});

        const CorridorReadModel corridor = store.query_corridor(acquisition.site_id);
        std::cout << std::fixed << std::setprecision(6)
                  << "selected_field_site=" << acquisition.site_id << '\n'
                  << "bald_nats=" << acquisition.bald_nats << '\n'
                  << "cost_aware_acquisition_score=" << acquisition.cost_aware_score << '\n'
                  << "corridor_quality=" << corridor.latest_corridor_quality << '\n'
                  << "maximum_risk_of_harm=" << corridor.maximum_risk_of_harm << '\n'
                  << "knowledge_factor=" << corridor.latest_knowledge_factor << '\n'
                  << "eco_impact_value=" << corridor.latest_eco_impact_value << '\n'
                  << "aggregate_event_count=" << corridor.event_count << '\n';
        return corridor.maximum_risk_of_harm <= 0.30 ? 0 : 2;
    } catch (const std::exception& error) {
        std::cerr << "rare-event and aggregate assessment failed: " << error.what() << '\n';
        return 1;
    }
}
