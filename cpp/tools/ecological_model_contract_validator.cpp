// File: cpp/tools/ecological_model_contract_validator.cpp

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace eco_restoration {

struct HexCoordinate {
    std::int32_t q{};
    std::int32_t r{};

    [[nodiscard]] std::array<HexCoordinate, 6> neighbors() const {
        return {{
            {q + 1, r}, {q + 1, r - 1}, {q, r - 1},
            {q - 1, r}, {q - 1, r + 1}, {q, r + 1}
        }};
    }

    [[nodiscard]] std::int32_t distance_to(const HexCoordinate& other) const {
        const std::int32_t dq = q - other.q;
        const std::int32_t dr = r - other.r;
        const std::int32_t ds = -dq - dr;
        return std::max({std::abs(dq), std::abs(dr), std::abs(ds)});
    }
};

struct SoilChemistrySample {
    std::int64_t observed_unix_s{};
    double potassium_mg_kg{};
    double acidity_ph{};
    double redox_mv{};
    double potassium_standard_deviation{};
    double acidity_standard_deviation{};
    double redox_standard_deviation{};
};

struct EcologicalTriad {
    double knowledge_factor{};
    double eco_impact_value{};
    double risk_of_harm{};
    double lyapunov_delta{};
};

struct ValidationReport {
    bool geometry_valid{};
    bool chemistry_valid{};
    bool triad_valid{};
    double chemistry_relative_uncertainty{};
    std::string status;
};

bool valid_chemistry(const SoilChemistrySample& sample) {
    return sample.observed_unix_s > 0 &&
           std::isfinite(sample.potassium_mg_kg) && sample.potassium_mg_kg >= 0.0 &&
           std::isfinite(sample.acidity_ph) && sample.acidity_ph >= 0.0 && sample.acidity_ph <= 14.0 &&
           std::isfinite(sample.redox_mv) && sample.redox_mv >= -1500.0 && sample.redox_mv <= 1500.0 &&
           sample.potassium_standard_deviation >= 0.0 &&
           sample.acidity_standard_deviation >= 0.0 &&
           sample.redox_standard_deviation >= 0.0;
}

bool valid_triad(const EcologicalTriad& triad) {
    return triad.knowledge_factor >= 0.0 && triad.knowledge_factor <= 1.0 &&
           triad.eco_impact_value >= 0.0 && triad.eco_impact_value <= 1.0 &&
           triad.risk_of_harm >= 0.0 && triad.risk_of_harm <= 1.0 &&
           triad.lyapunov_delta >= 0.0 &&
           triad.risk_of_harm >= triad.lyapunov_delta;
}

double relative_chemistry_uncertainty(const SoilChemistrySample& sample) {
    const double potassium_relative = sample.potassium_standard_deviation /
                                      std::max(sample.potassium_mg_kg, 1e-12);
    const double acidity_relative = sample.acidity_standard_deviation / 14.0;
    const double redox_relative = sample.redox_standard_deviation / 3000.0;
    return std::sqrt(
        potassium_relative * potassium_relative +
        acidity_relative * acidity_relative +
        redox_relative * redox_relative);
}

ValidationReport validate(
    const HexCoordinate& origin,
    const HexCoordinate& candidate,
    const SoilChemistrySample& chemistry,
    const EcologicalTriad& triad,
    double maximum_relative_uncertainty) {

    if (maximum_relative_uncertainty < 0.0) {
        throw std::invalid_argument("uncertainty limit must be nonnegative");
    }

    const auto neighbors = origin.neighbors();
    bool geometry_valid = origin.distance_to(origin) == 0;
    for (const HexCoordinate& neighbor : neighbors) {
        geometry_valid = geometry_valid && origin.distance_to(neighbor) == 1 &&
                         neighbor.distance_to(origin) == 1;
    }
    geometry_valid = geometry_valid && candidate.distance_to(origin) >= 0;

    const bool chemistry_valid = valid_chemistry(chemistry);
    const bool triad_valid = valid_triad(triad);
    const double uncertainty = chemistry_valid
        ? relative_chemistry_uncertainty(chemistry)
        : std::numeric_limits<double>::infinity();

    const std::string status =
        !geometry_valid || !chemistry_valid || !triad_valid ? "REJECT" :
        uncertainty > maximum_relative_uncertainty ? "INVESTIGATE" : "ACCEPT";

    return {geometry_valid, chemistry_valid, triad_valid, uncertainty, status};
}

}  // namespace eco_restoration

int main() {
    using namespace eco_restoration;

    const HexCoordinate origin{0, 0};
    const HexCoordinate candidate{3, -2};
    const SoilChemistrySample chemistry{
        1'770'000'000, 180.0, 7.1, 220.0, 12.0, 0.08, 15.0
    };
    const EcologicalTriad triad{0.91, 0.78, 0.18, 0.012};

    const ValidationReport report = validate(origin, candidate, chemistry, triad, 0.20);

    std::cout << std::fixed << std::setprecision(6)
              << "{\"geometry_valid\":" << (report.geometry_valid ? "true" : "false")
              << ",\"chemistry_valid\":" << (report.chemistry_valid ? "true" : "false")
              << ",\"triad_valid\":" << (report.triad_valid ? "true" : "false")
              << ",\"chemistry_relative_uncertainty\":" << report.chemistry_relative_uncertainty
              << ",\"status\":\"" << report.status << "\"}\n";
}
