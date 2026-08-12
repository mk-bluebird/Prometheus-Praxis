// File: cpp/eco_restoration/ppx_telemetry_core.cpp
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ppx::eco_restoration {

constexpr std::uint32_t kTelemetryAbiVersion = 1;
constexpr std::uint32_t kExtra1Present = 1U << 0U;
constexpr std::uint32_t kExtra2Present = 1U << 1U;

extern "C" struct alignas(8) TelemetryC {
    std::uint32_t abi_version;
    std::uint32_t byte_size;
    std::uint64_t sequence;
    std::int64_t timestamp_unix_ns;
    std::uint32_t telemetry_domain;
    std::uint32_t presence_flags;
    char machine_id[64];
    char station_id[64];
    char timestamp_utc[32];
    double r_hydraulics;
    double r_energy;
    double r_uncertainty;
    double r_reliability;
    double r_extra_1;
    double r_extra_2;
    double roh;
    double vt_current;
    double vt_next;
};

static_assert(sizeof(TelemetryC) == 264);
static_assert(alignof(TelemetryC) == 8);
static_assert(offsetof(TelemetryC, r_hydraulics) == 192);
static_assert(offsetof(TelemetryC, vt_next) == 256);

bool text_valid(const char* value, std::size_t capacity) noexcept {
    return value != nullptr && value[0] != '\0' && std::memchr(value, '\0', capacity) != nullptr;
}

bool unit_value(double value) noexcept {
    return std::isfinite(value) && value >= 0.0 && value <= 1.0;
}

extern "C" int ppx_telemetry_c_valid(const TelemetryC* telemetry) {
    if (telemetry == nullptr || telemetry->abi_version != kTelemetryAbiVersion ||
        telemetry->byte_size != sizeof(TelemetryC) ||
        !text_valid(telemetry->machine_id, sizeof(telemetry->machine_id)) ||
        !text_valid(telemetry->station_id, sizeof(telemetry->station_id)) ||
        !text_valid(telemetry->timestamp_utc, sizeof(telemetry->timestamp_utc)) ||
        !std::isfinite(telemetry->vt_current) || !std::isfinite(telemetry->vt_next)) {
        return 0;
    }

    const bool extra_1_valid = (telemetry->presence_flags & kExtra1Present) != 0U
        ? unit_value(telemetry->r_extra_1) : telemetry->r_extra_1 == 0.0;
    const bool extra_2_valid = (telemetry->presence_flags & kExtra2Present) != 0U
        ? unit_value(telemetry->r_extra_2) : telemetry->r_extra_2 == 0.0;

    return unit_value(telemetry->r_hydraulics) &&
           unit_value(telemetry->r_energy) &&
           unit_value(telemetry->r_uncertainty) &&
           unit_value(telemetry->r_reliability) &&
           unit_value(telemetry->roh) &&
           extra_1_valid && extra_2_valid;
}

struct ReliabilityCoordinate {
    double reliability{};
    double weight{};
};

double knowledge_factor(double confidence, double confidence_weight,
                        std::span<const ReliabilityCoordinate> reliabilities) {
    if (!unit_value(confidence) || !std::isfinite(confidence_weight) ||
        confidence_weight < 0.0 || reliabilities.empty()) {
        throw std::invalid_argument("invalid knowledge-factor inputs");
    }

    double log_sum = 0.0;
    double weight_sum = confidence_weight;
    if (confidence_weight > 0.0) {
        if (confidence == 0.0) return 0.0;
        log_sum = confidence_weight * std::log(confidence);
    }

    for (const ReliabilityCoordinate& coordinate : reliabilities) {
        if (!unit_value(coordinate.reliability) || !std::isfinite(coordinate.weight) ||
            coordinate.weight < 0.0) {
            throw std::invalid_argument("invalid reliability coordinate");
        }
        if (coordinate.weight > 0.0) {
            if (coordinate.reliability == 0.0) return 0.0;
            log_sum += coordinate.weight * std::log(coordinate.reliability);
            weight_sum += coordinate.weight;
        }
    }
    if (weight_sum <= 0.0) throw std::invalid_argument("positive reliability weight required");
    return std::exp(log_sum / weight_sum);
}

double derive_token_alpha(std::span<const double> historical_counts) {
    std::vector<double> values;
    for (double count : historical_counts) {
        if (std::isfinite(count) && count > 0.0) values.push_back(count);
    }
    if (values.empty()) throw std::invalid_argument("positive token history required");
    const std::size_t upper_index = values.size() / 2;
    std::nth_element(values.begin(), values.begin() + upper_index, values.end());
    const double upper = values[upper_index];
    if (values.size() % 2 != 0) return upper;
    std::nth_element(values.begin(), values.begin() + upper_index - 1, values.end());
    return 0.5 * (upper + values[upper_index - 1]);
}

double token_evidence(double token_count, double confidence, double alpha) {
    if (!std::isfinite(token_count) || !std::isfinite(confidence) || !std::isfinite(alpha) ||
        token_count < 0.0 || alpha <= 0.0) {
        throw std::invalid_argument("invalid token-evidence inputs");
    }
    return std::clamp(token_count / (token_count + alpha) * std::clamp(confidence, 0.0, 1.0),
                      0.0, 1.0);
}

struct SedimentTelemetry {
    double concentration_mg_l{};
    double velocity_m_s{};
    double drag_coefficient{};
    double grain_diameter_m{};
    double grain_density_kg_m3{2650.0};
    double water_density_kg_m3{998.0};
    double critical_shields{0.047};
    double concentration_reference_mg_l{};
};

double sediment_risk(const SedimentTelemetry& value) {
    for (double field : {value.concentration_mg_l, value.velocity_m_s, value.drag_coefficient,
                         value.grain_diameter_m, value.grain_density_kg_m3,
                         value.water_density_kg_m3, value.critical_shields,
                         value.concentration_reference_mg_l}) {
        if (!std::isfinite(field) || field <= 0.0) throw std::invalid_argument("invalid sediment telemetry");
    }
    if (value.grain_density_kg_m3 <= value.water_density_kg_m3) {
        throw std::invalid_argument("grain density must exceed water density");
    }
    constexpr double gravity = 9.80665;
    const double shear = value.water_density_kg_m3 * value.drag_coefficient *
                         value.velocity_m_s * value.velocity_m_s;
    const double shields = shear / ((value.grain_density_kg_m3 - value.water_density_kg_m3) *
                                    gravity * value.grain_diameter_m);
    const double availability = value.concentration_mg_l /
                                (value.concentration_mg_l + value.concentration_reference_mg_l);
    return std::clamp(availability * std::clamp(1.0 - shields / value.critical_shields, 0.0, 1.0),
                      0.0, 1.0);
}

struct TemperatureSample {
    std::int64_t hex_anchor{};
    double celsius{};
};

double thermal_risk(std::int64_t hex_anchor, std::span<const TemperatureSample> samples,
                    double base_c, double range_c, double gamma_per_c) {
    if (range_c <= 0.0 || gamma_per_c < 0.0) throw std::invalid_argument("invalid heat calibration");
    double maximum = -std::numeric_limits<double>::infinity();
    double sum = 0.0;
    std::size_t count = 0;
    for (const TemperatureSample& sample : samples) {
        if (sample.hex_anchor == hex_anchor) {
            if (!std::isfinite(sample.celsius)) throw std::invalid_argument("non-finite temperature");
            maximum = std::max(maximum, sample.celsius);
            sum += sample.celsius;
            ++count;
        }
    }
    if (count == 0) throw std::out_of_range("no temperatures for hex anchor");
    const double mean = sum / static_cast<double>(count);
    double variance = 0.0;
    for (const TemperatureSample& sample : samples) {
        if (sample.hex_anchor == hex_anchor) variance += (sample.celsius - mean) * (sample.celsius - mean);
    }
    return std::clamp((maximum - base_c) / range_c +
                      gamma_per_c * std::sqrt(variance / static_cast<double>(count)), 0.0, 1.0);
}

struct HabitatMetric {
    double species_richness{};
    double connectivity{};
    double fragmentation{};
};

double biodiversity_risk(std::int64_t hex_anchor,
                         const std::unordered_map<std::int64_t, HabitatMetric>& habitats,
                         std::array<double, 3> weights = {0.34, 0.33, 0.33}) {
    const auto found = habitats.find(hex_anchor);
    if (found == habitats.end()) throw std::out_of_range("habitat metric unavailable");
    const HabitatMetric& metric = found->second;
    for (double value : {metric.species_richness, metric.connectivity, metric.fragmentation,
                         weights[0], weights[1], weights[2]}) {
        if (!unit_value(value)) throw std::invalid_argument("habitat metrics and weights must be in [0,1]");
    }
    if (std::abs(weights[0] + weights[1] + weights[2] - 1.0) > 1e-9) {
        throw std::invalid_argument("biodiversity weights must sum to one");
    }
    return std::clamp(weights[0] * (1.0 - metric.species_richness) +
                      weights[1] * (1.0 - metric.connectivity) +
                      weights[2] * metric.fragmentation, 0.0, 1.0);
}

}  // namespace ppx::eco_restoration
