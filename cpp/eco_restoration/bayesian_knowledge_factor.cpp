// File: cpp/eco_restoration/bayesian_knowledge_factor.cpp

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace eco_restoration {

struct Evidence {
    double confidence{};
    double reliability{};
};

double weighted_geometric_mean(
    double ai_confidence,
    const std::vector<Evidence>& evidence) {

    double log_sum = std::log(std::clamp(ai_confidence, 1e-12, 1.0));
    double weight_sum = 1.0;

    for (const Evidence& item : evidence) {
        if (item.confidence <= 0.0 || item.confidence > 1.0 ||
            item.reliability <= 0.0 || item.reliability > 1.0) {
            throw std::invalid_argument("evidence values must be in (0,1]");
        }
        log_sum += item.reliability * std::log(item.confidence);
        weight_sum += item.reliability;
    }
    return std::exp(log_sum / weight_sum);
}

double bayesian_knowledge_factor(
    double prior_positive_impact,
    double ai_confidence,
    const std::vector<Evidence>& evidence) {

    if (prior_positive_impact <= 0.0 || prior_positive_impact >= 1.0) {
        throw std::invalid_argument("prior must be in (0,1)");
    }

    const auto update = [](double posterior, double confidence, double reliability) {
        const double likelihood_positive =
            reliability * confidence + (1.0 - reliability) * (1.0 - confidence);
        const double likelihood_negative =
            reliability * (1.0 - confidence) + (1.0 - reliability) * confidence;
        const double numerator = posterior * likelihood_positive;
        return numerator / std::max(1e-12, numerator + (1.0 - posterior) * likelihood_negative);
    };

    double posterior = update(prior_positive_impact, ai_confidence, 0.90);
    for (const Evidence& item : evidence) {
        if (item.confidence < 0.0 || item.confidence > 1.0 ||
            item.reliability <= 0.0 || item.reliability > 1.0) {
            throw std::invalid_argument("invalid Bayesian evidence");
        }
        posterior = update(posterior, item.confidence, item.reliability);
    }
    return posterior;
}

}  // namespace eco_restoration

int main() {
    using namespace eco_restoration;

    const double ai_confidence = 0.88;
    const std::vector<Evidence> evidence{
        {0.92, 0.96},
        {0.77, 0.84},
        {0.81, 0.89}
    };

    std::cout << std::fixed << std::setprecision(6)
              << "{\"geometric_knowledge\":"
              << weighted_geometric_mean(ai_confidence, evidence)
              << ",\"bayesian_knowledge\":"
              << bayesian_knowledge_factor(0.50, ai_confidence, evidence)
              << "}\n";
}
