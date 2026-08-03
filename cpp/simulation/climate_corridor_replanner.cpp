// File: cpp/simulation/climate_corridor_replanner.cpp
#include <iostream>
#include <vector>
#include <string>
#include <cmath>

namespace eco {

struct ClimateScenario {
    std::string name;
    // Downscaled projections: mean ΔV_t drift multiplier and variance multiplier per hex.
    double drift_multiplier;
    double variance_multiplier;
};

struct HexCorridorParams {
    std::string hex_id;
    double gamma;        // current KER coupling
    double delta_V_max;  // current per-workload ΔV_max
};

struct ReplannedCorridor {
    std::string hex_id;
    std::string scenario_name;
    double gamma_new;
    double delta_V_max_new;
    double target_breach_prob; // e.g. 0.05
};

class ClimateCorridorReplanner {
public:
    ClimateCorridorReplanner(double base_breach_prob,
                             double safety_factor)
        : base_breach_prob(base_breach_prob),
          safety_factor(safety_factor) {}

    ReplannedCorridor replan(const HexCorridorParams& params,
                             const ClimateScenario& scenario) const {
        // Simple heuristic: if drift/variance increase, tighten gamma and lower ΔV_max.
        double drift_mult = scenario.drift_multiplier;
        double var_mult = scenario.variance_multiplier;

        double gamma_new = params.gamma;
        double delta_V_max_new = params.delta_V_max;

        if (drift_mult > 1.0) {
            gamma_new *= 1.0 / (1.0 + safety_factor * (drift_mult - 1.0));
        }
        if (var_mult > 1.0) {
            delta_V_max_new *= 1.0 / (1.0 + safety_factor * (var_mult - 1.0));
        }

        if (gamma_new < 0.0) gamma_new = 0.0;
        if (delta_V_max_new < 0.0) delta_V_max_new = 0.0;

        ReplannedCorridor rc;
        rc.hex_id = params.hex_id;
        rc.scenario_name = scenario.name;
        rc.gamma_new = gamma_new;
        rc.delta_V_max_new = delta_V_max_new;
        rc.target_breach_prob = base_breach_prob;
        return rc;
    }

private:
    double base_breach_prob;
    double safety_factor;
};

void print_replan_sql(const ReplannedCorridor& rc) {
    std::cout << "INSERT INTO climate_corridor_plan "
              << "(hex_id, scenario_name, gamma_new, delta_v_max_new, target_breach_prob) "
              << "VALUES ('" << rc.hex_id << "', '"
              << rc.scenario_name << "', "
              << rc.gamma_new << ", "
              << rc.delta_V_max_new << ", "
              << rc.target_breach_prob << ");\n";
}

} // namespace eco

int main() {
    using namespace eco;

    // Example downscaled Phoenix climate scenarios
    ClimateScenario s1{"RCP4.5_2030s", 1.15, 1.20};
    ClimateScenario s2{"RCP8.5_2050s", 1.35, 1.50};

    HexCorridorParams h1{"hex_PHX_001", 0.1, 0.05};
    HexCorridorParams h2{"hex_PHX_002", 0.12, 0.05};

    ClimateCorridorReplanner replanner(/*base_breach_prob=*/0.05,
                                       /*safety_factor=*/0.8);

    std::vector<ClimateScenario> scenarios = {s1, s2};
    std::vector<HexCorridorParams> hexes = {h1, h2};

    for (const auto& hex : hexes) {
        for (const auto& sc : scenarios) {
            ReplannedCorridor rc = replanner.replan(hex, sc);
            print_replan_sql(rc);
        }
    }

    return 0;
}
