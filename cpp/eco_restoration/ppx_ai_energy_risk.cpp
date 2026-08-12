// File: cpp/eco_restoration/ppx_ai_energy_risk.cpp
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace ppx::eco_restoration {

struct EnergySample {
    double power_w{};
    double duration_s{};
    double renewable_fraction{};
    double marginal_carbon_g_per_kwh{};
};

struct EnergyRiskReference {
    double approved_carbon_g_per_ecological_unit{};
    double reference_ecological_units{};
    double target_risk_at_reference{};
};

struct EnergyRiskResult {
    double marginal_carbon_g{};
    double reference_carbon_g{};
    double r_energy{};
};

double derive_reference_carbon_g(const EnergyRiskReference& reference) {
    if (!std::isfinite(reference.approved_carbon_g_per_ecological_unit) ||
        !std::isfinite(reference.reference_ecological_units) ||
        !std::isfinite(reference.target_risk_at_reference) ||
        reference.approved_carbon_g_per_ecological_unit <= 0.0 ||
        reference.reference_ecological_units <= 0.0 ||
        reference.target_risk_at_reference <= 0.0 ||
        reference.target_risk_at_reference > 1.0) {
        throw std::invalid_argument("invalid ecological carbon-reference calibration");
    }
    const double approved_carbon_g =
        reference.approved_carbon_g_per_ecological_unit *
        reference.reference_ecological_units;
    return approved_carbon_g / reference.target_risk_at_reference;
}

EnergyRiskResult compute_energy_risk(
    const std::vector<EnergySample>& samples,
    const EnergyRiskReference& reference) {
    double marginal_carbon_g = 0.0;
    for (const EnergySample& sample : samples) {
        if (!std::isfinite(sample.power_w) || !std::isfinite(sample.duration_s) ||
            !std::isfinite(sample.renewable_fraction) ||
            !std::isfinite(sample.marginal_carbon_g_per_kwh) ||
            sample.power_w < 0.0 || sample.duration_s < 0.0 ||
            sample.renewable_fraction < 0.0 || sample.renewable_fraction > 1.0 ||
            sample.marginal_carbon_g_per_kwh < 0.0) {
            throw std::invalid_argument("invalid AI-workload energy sample");
        }
        marginal_carbon_g +=
            sample.power_w * sample.duration_s *
            (1.0 - sample.renewable_fraction) *
            sample.marginal_carbon_g_per_kwh / 3'600'000.0;
    }
    const double reference_carbon_g = derive_reference_carbon_g(reference);
    return {marginal_carbon_g, reference_carbon_g,
            std::clamp(marginal_carbon_g / reference_carbon_g, 0.0, 1.0)};
}

double compute_ecological_benefit(
    double knowledge_factor, double risk_coordinate, double ecological_value) {
    for (double value : {knowledge_factor, risk_coordinate, ecological_value}) {
        if (!std::isfinite(value) || value < 0.0 || value > 1.0) {
            throw std::invalid_argument("K, R, and ecological value must be in [0,1]");
        }
    }
    return knowledge_factor * ecological_value * (1.0 - risk_coordinate);
}

extern "C" int ppx_compute_energy_and_ecological_scores(
    const EnergySample* samples, std::size_t count, EnergyRiskReference reference,
    double knowledge_factor, double ecological_value, double* r_energy,
    double* ecological_benefit) {
    if (samples == nullptr || r_energy == nullptr || ecological_benefit == nullptr) {
        return 2;
    }
    try {
        const EnergyRiskResult risk =
            compute_energy_risk(std::vector<EnergySample>(samples, samples + count), reference);
        *r_energy = risk.r_energy;
        *ecological_benefit =
            compute_ecological_benefit(knowledge_factor, risk.r_energy, ecological_value);
        return 0;
    } catch (...) {
        return 1;
    }
}

}  // namespace ppx::eco_restoration
