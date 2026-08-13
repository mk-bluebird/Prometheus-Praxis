// File: cpp/eco_restoration/private_eco_metric_comparison.hpp
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace eco_restoration {

/*
Boolean-only disclosure is appropriate when the decision is intrinsically
threshold-based and the audit record retains metric name, units, threshold,
aggregation window, uncertainty allowance, and data-quality coverage.

Suitable metrics:
- Risk of harm below a declared maximum.
- Corridor quality above a declared minimum.
- Water use below an allocated conservation budget.
- Canopy-temperature upper confidence bound below a heat limit.
- Sensor coverage above a minimum completeness threshold.
- Ecological intervention eligibility requiring all mandatory predicates.

Unsuitable metrics:
- Rank ordering among communities or sites.
- Cost allocation, resource allocation, or tradeable rewards.
- Scientific effect-size reporting and model calibration.
- Any metric where magnitude, uncertainty distribution, or trend is material
  to the decision rather than merely threshold satisfaction.
*/
enum class EcoMetric {
    RiskOfHarm,
    CorridorQuality,
    WaterUse,
    CanopyTemperatureUpperBound,
    SensorCoverage
};

enum class Comparison {
    LessOrEqual,
    GreaterOrEqual
};

struct MetricThreshold {
    EcoMetric metric{};
    Comparison comparison{};
    double threshold{};
    double uncertainty_margin{};
    std::string units;
    std::string aggregation_window;
    std::string schema_version;
};

struct BooleanAuditRecord {
    EcoMetric metric{};
    Comparison comparison{};
    double declared_threshold{};
    double uncertainty_margin{};
    std::string units;
    std::string aggregation_window;
    std::string schema_version;
    bool passed{};
    double data_completeness{};
    double knowledge_factor{};
    double eco_impact_value{};
};

inline bool threshold_compare(double protected_value,
                              const MetricThreshold& threshold) {
    if (!std::isfinite(protected_value) || !std::isfinite(threshold.threshold) ||
        threshold.uncertainty_margin < 0.0 || threshold.units.empty() ||
        threshold.aggregation_window.empty() || threshold.schema_version.empty()) {
        throw std::invalid_argument("invalid protected metric or audit threshold");
    }

    if (threshold.comparison == Comparison::LessOrEqual) {
        return protected_value + threshold.uncertainty_margin <= threshold.threshold;
    }
    return protected_value - threshold.uncertainty_margin >= threshold.threshold;
}

inline BooleanAuditRecord compare_private_eco_metric(
    double protected_value, const MetricThreshold& threshold,
    double data_completeness) {

    if (!(data_completeness >= 0.0 && data_completeness <= 1.0)) {
        throw std::invalid_argument("data completeness must lie in [0,1]");
    }

    const bool passed = threshold_compare(protected_value, threshold);
    const double uncertainty_ratio = std::abs(threshold.uncertainty_margin) /
        std::max(1.0, std::abs(threshold.threshold));
    const double knowledge = std::clamp(
        data_completeness * (1.0 - std::min(0.50, uncertainty_ratio)), 0.0, 1.0);

    double impact = 0.0;
    switch (threshold.metric) {
        case EcoMetric::RiskOfHarm:
            impact = passed ? 1.0 : 0.0;
            break;
        case EcoMetric::CorridorQuality:
        case EcoMetric::SensorCoverage:
            impact = passed ? 0.85 : 0.15;
            break;
        case EcoMetric::WaterUse:
        case EcoMetric::CanopyTemperatureUpperBound:
            impact = passed ? 0.90 : 0.10;
            break;
    }

    return {threshold.metric, threshold.comparison, threshold.threshold,
            threshold.uncertainty_margin, threshold.units,
            threshold.aggregation_window, threshold.schema_version, passed,
            data_completeness, knowledge, impact};
}

inline bool all_required_private_predicates_pass(
    const std::vector<BooleanAuditRecord>& records) {
    if (records.empty()) return false;
    for (const auto& record : records) {
        if (!record.passed || record.data_completeness < 0.90 ||
            record.knowledge_factor < 0.70) {
            return false;
        }
    }
    return true;
}

}  // namespace eco_restoration
