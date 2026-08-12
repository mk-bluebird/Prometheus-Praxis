// File: cpp/eco_restoration/telemetry_quality_gate.cpp

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace eco_restoration {

enum class TelemetryQuality : std::uint8_t {
    Good,
    Suspect,
    Bad
};

struct TelemetryFrame {
    std::int64_t observed_unix_s{};
    double power_w{};
    double temperature_c{};
    double water_quality{};
    bool power_sensor_healthy{};
    bool temperature_sensor_healthy{};
    bool water_sensor_healthy{};
};

struct QualityLimits {
    double maximum_power_w{};
    double minimum_temperature_c{};
    double maximum_temperature_c{};
    double maximum_power_rate_w_s{};
    double maximum_temperature_rate_c_s{};
    double maximum_water_quality_rate_s{};
};

struct QualityResult {
    TelemetryQuality quality{};
    bool forward_to_lane_evaluator{};
};

QualityResult assess_quality(
    const TelemetryFrame& current,
    const TelemetryFrame* previous,
    const QualityLimits& limits) {

    const bool range_valid =
        current.power_w >= 0.0 && current.power_w <= limits.maximum_power_w &&
        current.temperature_c >= limits.minimum_temperature_c &&
        current.temperature_c <= limits.maximum_temperature_c &&
        current.water_quality >= 0.0 && current.water_quality <= 1.0;

    const bool diagnostics_valid =
        current.power_sensor_healthy &&
        current.temperature_sensor_healthy &&
        current.water_sensor_healthy;

    if (!range_valid || !diagnostics_valid) {
        return {TelemetryQuality::Bad, false};
    }

    if (previous == nullptr) return {TelemetryQuality::Good, true};

    const double dt = static_cast<double>(current.observed_unix_s - previous->observed_unix_s);
    if (dt <= 0.0) return {TelemetryQuality::Bad, false};

    const double power_rate = std::abs(current.power_w - previous->power_w) / dt;
    const double temperature_rate = std::abs(current.temperature_c - previous->temperature_c) / dt;
    const double water_rate = std::abs(current.water_quality - previous->water_quality) / dt;

    const bool rate_valid =
        power_rate <= limits.maximum_power_rate_w_s &&
        temperature_rate <= limits.maximum_temperature_rate_c_s &&
        water_rate <= limits.maximum_water_quality_rate_s;

    return rate_valid
        ? QualityResult{TelemetryQuality::Good, true}
        : QualityResult{TelemetryQuality::Suspect, false};
}

}  // namespace eco_restoration
