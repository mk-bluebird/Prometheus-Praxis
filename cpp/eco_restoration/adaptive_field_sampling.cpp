// File: cpp/eco_restoration/adaptive_field_sampling.cpp
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace eco_restoration {

struct HexUncertainty {
    std::uint64_t anchor{};
    double x_m{};
    double y_m{};
    double restoration_success_probability{};
    double predictive_uncertainty{};
    double survey_cost{};
};

struct SamplingPlan {
    std::vector<std::uint64_t> anchors;
    double expected_information_gain{};
    double knowledge_factor{};
    double eco_impact_value{};
};

double binary_entropy(double probability) {
    const double p = std::clamp(probability, 1e-12, 1.0 - 1e-12);
    return -p * std::log2(p) - (1.0 - p) * std::log2(1.0 - p);
}

SamplingPlan select_field_verification_hexes(const std::vector<HexUncertainty>& candidates,
                                             std::size_t visit_count, double budget,
                                             double diversity_length_m) {
    if (visit_count == 0 || budget < 0.0 || diversity_length_m <= 0.0)
        throw std::invalid_argument("invalid sampling constraints");

    std::vector<bool> selected(candidates.size(), false);
    SamplingPlan plan;
    double spent = 0.0;
    while (plan.anchors.size() < visit_count) {
        std::size_t best = candidates.size();
        double best_score = -1.0;
        for (std::size_t i = 0; i < candidates.size(); ++i) {
            const auto& candidate = candidates[i];
            if (selected[i] || candidate.predictive_uncertainty < 0.0 ||
                candidate.predictive_uncertainty > 1.0 || candidate.survey_cost < 0.0 ||
                spent + candidate.survey_cost > budget) continue;

            double nearest_distance = diversity_length_m;
            for (std::size_t j = 0; j < candidates.size(); ++j) {
                if (!selected[j]) continue;
                const double dx = candidate.x_m - candidates[j].x_m;
                const double dy = candidate.y_m - candidates[j].y_m;
                nearest_distance = std::min(nearest_distance, std::sqrt(dx * dx + dy * dy));
            }
            const double diversity = std::min(1.0, nearest_distance / diversity_length_m);
            const double score = binary_entropy(candidate.restoration_success_probability) *
                                 candidate.predictive_uncertainty * diversity /
                                 std::max(1.0, candidate.survey_cost);
            if (score > best_score) {
                best_score = score;
                best = i;
            }
        }
        if (best == candidates.size()) break;
        selected[best] = true;
        spent += candidates[best].survey_cost;
        plan.anchors.push_back(candidates[best].anchor);
        plan.expected_information_gain += best_score;
    }
    plan.knowledge_factor = visit_count == 0 ? 0.0 :
        static_cast<double>(plan.anchors.size()) / visit_count;
    plan.eco_impact_value = std::clamp(plan.knowledge_factor *
        (1.0 - std::exp(-plan.expected_information_gain)), 0.0, 1.0);
    return plan;
}

}  // namespace eco_restoration
