// File: cpp/eco_restoration/knowledge_factor_reliability.cpp
#include <cmath>
#include <span>
#include <stdexcept>

namespace ppx::eco_restoration {

struct ReliabilityCoordinate {
    double reliability{};
    double weight{};
};

double knowledge_factor(
    double workload_confidence,
    double confidence_weight,
    std::span<const ReliabilityCoordinate> coordinates) {
    if (!std::isfinite(workload_confidence) || workload_confidence < 0.0 ||
        workload_confidence > 1.0 || !std::isfinite(confidence_weight) ||
        confidence_weight < 0.0 || coordinates.empty()) {
        throw std::invalid_argument("invalid confidence or reliability inputs");
    }

    if (workload_confidence == 0.0 && confidence_weight > 0.0) return 0.0;
    double weighted_log_sum =
        confidence_weight > 0.0 ? confidence_weight * std::log(workload_confidence) : 0.0;
    double weight_sum = confidence_weight;

    for (const ReliabilityCoordinate& coordinate : coordinates) {
        if (!std::isfinite(coordinate.reliability) || !std::isfinite(coordinate.weight) ||
            coordinate.reliability < 0.0 || coordinate.reliability > 1.0 ||
            coordinate.weight < 0.0) {
            throw std::invalid_argument("reliability coordinates must be weighted values in [0,1]");
        }
        if (coordinate.reliability == 0.0 && coordinate.weight > 0.0) return 0.0;
        if (coordinate.weight > 0.0) {
            weighted_log_sum += coordinate.weight * std::log(coordinate.reliability);
            weight_sum += coordinate.weight;
        }
    }
    if (weight_sum <= 0.0) throw std::invalid_argument("at least one positive weight is required");
    return std::exp(weighted_log_sum / weight_sum);
}

}  // namespace ppx::eco_restoration
