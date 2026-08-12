// File: cpp/eco_restoration/thermal_biodiversity_risk.cpp
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace ppx::eco_restoration {

struct TemperatureSample {
    std::int64_t hex_anchor{};
    double celsius{};
};

struct HeatRiskCalibration {
    double base_temperature_c{};
    double temperature_range_c{};
    double variability_weight_per_c{};
};

struct HeatRiskResult {
    double daily_max_c{};
    double daily_mean_c{};
    double daily_standard_deviation_c{};
    double r_heat{};
};

class HexThermalRisk {
public:
    [[nodiscard]] HeatRiskResult evaluate(
        std::int64_t hex_anchor,
        std::span<const TemperatureSample> readings,
        const HeatRiskCalibration& calibration) const {
        if (readings.empty() || calibration.temperature_range_c <= 0.0 ||
            calibration.variability_weight_per_c < 0.0) {
            throw std::invalid_argument("invalid thermal-risk calibration or readings");
        }

        double maximum = -std::numeric_limits<double>::infinity();
        double total = 0.0;
        std::size_t count = 0;
        for (const TemperatureSample& reading : readings) {
            if (reading.hex_anchor != hex_anchor) continue;
            if (!std::isfinite(reading.celsius)) {
                throw std::invalid_argument("temperature reading must be finite");
            }
            maximum = std::max(maximum, reading.celsius);
            total += reading.celsius;
            ++count;
        }
        if (count == 0) throw std::invalid_argument("hex anchor has no temperature readings");

        const double mean = total / static_cast<double>(count);
        double squared_error = 0.0;
        for (const TemperatureSample& reading : readings) {
            if (reading.hex_anchor == hex_anchor) {
                const double error = reading.celsius - mean;
                squared_error += error * error;
            }
        }
        const double standard_deviation = std::sqrt(squared_error / static_cast<double>(count));
        const double exceedance =
            (maximum - calibration.base_temperature_c) / calibration.temperature_range_c;
        const double risk = std::clamp(
            exceedance + calibration.variability_weight_per_c * standard_deviation,
            0.0, 1.0);

        return {maximum, mean, standard_deviation, risk};
    }

    [[nodiscard]] static bool station_consistent(
        const HeatRiskResult& hex_result,
        double station_daily_max_c,
        double maximum_allowed_difference_c) {
        return std::isfinite(station_daily_max_c) &&
               maximum_allowed_difference_c >= 0.0 &&
               std::abs(hex_result.daily_max_c - station_daily_max_c) <=
                   maximum_allowed_difference_c;
    }
};

struct HabitatMetric {
    double species_richness_normalized{};
    double connectivity_normalized{};
    double fragmentation_normalized{};
};

struct BiodiversityWeights {
    double richness_weight{0.34};
    double connectivity_weight{0.33};
    double fragmentation_weight{0.33};
};

class HexBiodiversityRisk {
public:
    explicit HexBiodiversityRisk(std::unordered_map<std::int64_t, HabitatMetric> metrics)
        : metrics_(std::move(metrics)) {}

    [[nodiscard]] double operator()(
        std::int64_t hex_anchor,
        const BiodiversityWeights& weights = {}) const {
        const auto found = metrics_.find(hex_anchor);
        if (found == metrics_.end()) {
            throw std::out_of_range("habitat metric unavailable for hex anchor");
        }

        validate_weights(weights);
        const HabitatMetric& metric = found->second;
        validate_unit(metric.species_richness_normalized);
        validate_unit(metric.connectivity_normalized);
        validate_unit(metric.fragmentation_normalized);

        return std::clamp(
            weights.richness_weight * (1.0 - metric.species_richness_normalized) +
            weights.connectivity_weight * (1.0 - metric.connectivity_normalized) +
            weights.fragmentation_weight * metric.fragmentation_normalized,
            0.0, 1.0);
    }

private:
    std::unordered_map<std::int64_t, HabitatMetric> metrics_;

    static void validate_unit(double value) {
        if (!std::isfinite(value) || value < 0.0 || value > 1.0) {
            throw std::invalid_argument("normalized habitat metric must be within [0,1]");
        }
    }

    static void validate_weights(const BiodiversityWeights& weights) {
        validate_unit(weights.richness_weight);
        validate_unit(weights.connectivity_weight);
        validate_unit(weights.fragmentation_weight);
        const double sum = weights.richness_weight +
                           weights.connectivity_weight +
                           weights.fragmentation_weight;
        if (std::abs(sum - 1.0) > 1e-9) {
            throw std::invalid_argument("biodiversity weights must sum to one");
        }
    }
};

}  // namespace ppx::eco_restoration
