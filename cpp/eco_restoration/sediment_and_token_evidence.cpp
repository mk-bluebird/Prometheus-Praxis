// File: cpp/eco_restoration/sediment_and_token_evidence.cpp
#include <algorithm>
#include <cmath>
#include <span>
#include <stdexcept>
#include <vector>

namespace ppx::eco_restoration {

struct SedimentTelemetry {
    double sediment_concentration_mg_l{};
    double flow_velocity_m_s{};
    double drag_coefficient{};
    double median_grain_diameter_m{};
    double grain_density_kg_m3{2650.0};
    double water_density_kg_m3{998.0};
    double critical_shields_parameter{0.047};
    double concentration_reference_mg_l{};
};

struct SedimentRiskResult {
    double bed_shear_pa{};
    double shields_parameter{};
    double mobility_deficit{};
    double concentration_availability{};
    double r_sediment{};
};

class SedimentDepositionRisk {
public:
    [[nodiscard]] SedimentRiskResult operator()(const SedimentTelemetry& t) const {
        validate(t);
        constexpr double gravity_m_s2 = 9.80665;
        const double shear =
            t.water_density_kg_m3 * t.drag_coefficient *
            t.flow_velocity_m_s * t.flow_velocity_m_s;
        const double shields = shear / (
            (t.grain_density_kg_m3 - t.water_density_kg_m3) *
            gravity_m_s2 * t.median_grain_diameter_m);
        const double mobility_deficit =
            std::clamp(1.0 - shields / t.critical_shields_parameter, 0.0, 1.0);
        const double sediment_availability =
            t.sediment_concentration_mg_l /
            (t.sediment_concentration_mg_l + t.concentration_reference_mg_l);
        const double deposition_risk =
            std::clamp(sediment_availability * mobility_deficit, 0.0, 1.0);

        return {shear, shields, mobility_deficit, sediment_availability, deposition_risk};
    }

private:
    static void validate(const SedimentTelemetry& t) {
        for (const double value : {
            t.sediment_concentration_mg_l, t.flow_velocity_m_s, t.drag_coefficient,
            t.median_grain_diameter_m, t.grain_density_kg_m3, t.water_density_kg_m3,
            t.critical_shields_parameter, t.concentration_reference_mg_l
        }) {
            if (!std::isfinite(value) || value <= 0.0) {
                throw std::invalid_argument("sediment telemetry values must be finite and positive");
            }
        }
        if (t.grain_density_kg_m3 <= t.water_density_kg_m3) {
            throw std::invalid_argument("grain density must exceed water density");
        }
    }
};

double derive_token_half_saturation_alpha(std::span<const double> historical_token_counts) {
    std::vector<double> values;
    values.reserve(historical_token_counts.size());
    for (const double count : historical_token_counts) {
        if (std::isfinite(count) && count > 0.0) values.push_back(count);
    }
    if (values.empty()) {
        throw std::invalid_argument("historical token distribution has no positive finite values");
    }

    const std::size_t middle = values.size() / 2;
    std::nth_element(values.begin(), values.begin() + middle, values.end());
    const double upper = values[middle];
    if (values.size() % 2 != 0) return upper;

    std::nth_element(values.begin(), values.begin() + middle - 1, values.end());
    return 0.5 * (upper + values[middle - 1]);
}

double token_evidence(double token_count, double model_confidence, double alpha) {
    if (!std::isfinite(token_count) || !std::isfinite(model_confidence) ||
        !std::isfinite(alpha) || token_count < 0.0 || alpha <= 0.0) {
        throw std::invalid_argument("invalid token-evidence inputs");
    }
    const double bounded_confidence = std::clamp(model_confidence, 0.0, 1.0);
    const double saturation = token_count / (token_count + alpha);
    return std::clamp(saturation * bounded_confidence, 0.0, 1.0);
}

}  // namespace ppx::eco_restoration
