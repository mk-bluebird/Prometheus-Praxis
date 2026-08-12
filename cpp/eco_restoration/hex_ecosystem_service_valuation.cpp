// File: cpp/eco_restoration/hex_ecosystem_service_valuation.cpp
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace eco_restoration {

struct BenefitTransferCoefficients {
    std::string source_identifier;
    double cooling_usd_per_celsius_m2_year{};
    double carbon_usd_per_kg_c_year{};
    double purification_usd_per_kg_pollutant_removed{};
};

struct HexServiceInputs {
    double shaded_area_m2{};
    double cooling_celsius_reduction{};
    double sequestered_kg_c_year{};
    double pollutant_removed_kg_year{};
    double evidence_confidence{};
};

struct HexServiceValue {
    double cooling_usd_year{};
    double carbon_usd_year{};
    double water_purification_usd_year{};
    double total_usd_year{};
    double knowledge_factor{};
    double eco_impact_value{};
};

HexServiceValue value_ecosystem_services(const HexServiceInputs& input,
                                         const BenefitTransferCoefficients& coefficients) {
    if (coefficients.source_identifier.empty() ||
        coefficients.cooling_usd_per_celsius_m2_year < 0.0 ||
        coefficients.carbon_usd_per_kg_c_year < 0.0 ||
        coefficients.purification_usd_per_kg_pollutant_removed < 0.0 ||
        input.shaded_area_m2 < 0.0 || input.cooling_celsius_reduction < 0.0 ||
        input.sequestered_kg_c_year < 0.0 || input.pollutant_removed_kg_year < 0.0 ||
        input.evidence_confidence < 0.0 || input.evidence_confidence > 1.0)
        throw std::invalid_argument("invalid valuation inputs or coefficients");

    const double cooling = input.shaded_area_m2 * input.cooling_celsius_reduction *
                           coefficients.cooling_usd_per_celsius_m2_year;
    const double carbon = input.sequestered_kg_c_year * coefficients.carbon_usd_per_kg_c_year;
    const double purification = input.pollutant_removed_kg_year *
                                 coefficients.purification_usd_per_kg_pollutant_removed;
    const double total = cooling + carbon + purification;
    const double knowledge = std::clamp(input.evidence_confidence, 0.0, 1.0);
    const double impact = std::clamp(knowledge * (1.0 - std::exp(-total / 1000.0)), 0.0, 1.0);
    return {cooling, carbon, purification, total, knowledge, impact};
}

}  // namespace eco_restoration
