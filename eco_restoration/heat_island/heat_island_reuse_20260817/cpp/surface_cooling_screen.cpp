#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

struct Candidate {
    std::string name;
    double delta_absorbed_radiation_w_m2;
    double latent_heat_flux_w_m2;
    double heat_transfer_coefficient_w_m2_k;
    double lifecycle_cost;
    double water_suitability;
    double habitat_suitability;
};

struct Result {
    double delta_t_c;
    double cooling_per_cost;
    bool eligible;
};

static double clamp01(double value) {
    return std::clamp(value, 0.0, 1.0);
}

static Result evaluate(const Candidate& candidate) {
    if (candidate.name.empty() ||
        !std::isfinite(candidate.delta_absorbed_radiation_w_m2) ||
        !std::isfinite(candidate.latent_heat_flux_w_m2) ||
        candidate.heat_transfer_coefficient_w_m2_k <= 0.0 ||
        candidate.lifecycle_cost <= 0.0 ||
        candidate.water_suitability < 0.0 || candidate.water_suitability > 1.0 ||
        candidate.habitat_suitability < 0.0 || candidate.habitat_suitability > 1.0) {
        throw std::invalid_argument("candidate values are invalid");
    }

    const double delta_t_c = (
        candidate.delta_absorbed_radiation_w_m2 - candidate.latent_heat_flux_w_m2
    ) / candidate.heat_transfer_coefficient_w_m2_k;

    const double cooling_c = std::max(0.0, -delta_t_c);
    const bool eligible = cooling_c > 0.0 &&
        candidate.water_suitability > 0.0 &&
        candidate.habitat_suitability > 0.0;

    return {
        delta_t_c,
        eligible ? cooling_c / candidate.lifecycle_cost : 0.0,
        eligible
    };
}

int main(int argc, char** argv) {
    if (argc < 8 || ((argc - 1) % 7 != 0)) {
        std::cerr
            << "usage: " << argv[0]
            << " <name> <delta_absorbed_W_m2> <latent_flux_W_m2> <h_W_m2_K>"
            << " <lifecycle_cost> <water_suitability_0_to_1> <habitat_suitability_0_to_1> [...]\n";
        return 64;
    }

    try {
        std::string best_name;
        Result best_result{0.0, -1.0, false};
        double best_eco_impact = 0.0;
        double best_harm_risk = 1.0;

        for (int index = 1; index < argc; index += 7) {
            Candidate candidate{
                argv[index],
                std::stod(argv[index + 1]),
                std::stod(argv[index + 2]),
                std::stod(argv[index + 3]),
                std::stod(argv[index + 4]),
                std::stod(argv[index + 5]),
                std::stod(argv[index + 6])
            };

            const Result result = evaluate(candidate);
            const double cooling_c = std::max(0.0, -result.delta_t_c);
            const double knowledge_factor = clamp01(
                0.55 + 0.20 * candidate.water_suitability + 0.25 * candidate.habitat_suitability
            );
            const double harm_risk = clamp01(
                0.60 - 0.30 * candidate.water_suitability - 0.30 * candidate.habitat_suitability
            );
            const double eco_impact = clamp01(
                knowledge_factor * (1.0 - harm_risk) * std::min(1.0, cooling_c / 2.0)
            );

            std::cout << std::fixed << std::setprecision(8)
                      << "candidate=" << candidate.name
                      << " delta_T_C=" << result.delta_t_c
                      << " cooling_per_cost=" << result.cooling_per_cost
                      << " eligible=" << (result.eligible ? 1 : 0)
                      << " knowledge_factor=" << knowledge_factor
                      << " eco_impact_value=" << eco_impact
                      << " harm_risk=" << harm_risk
                      << '\n';

            if (result.eligible && result.cooling_per_cost > best_result.cooling_per_cost) {
                best_name = candidate.name;
                best_result = result;
                best_eco_impact = eco_impact;
                best_harm_risk = harm_risk;
            }
        }

        if (best_result.eligible) {
            std::cout << "recommended_candidate=" << best_name << '\n';
            std::cout << "recommended_cooling_per_cost=" << std::fixed << std::setprecision(8)
                      << best_result.cooling_per_cost << '\n';
            std::cout << "recommended_eco_impact_value=" << best_eco_impact << '\n';
            std::cout << "recommended_harm_risk=" << best_harm_risk << '\n';
        } else {
            std::cout << "recommended_candidate=NONE_REQUIRES_FIELD_DATA\n";
        }

        return 0;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 65;
    }
}
