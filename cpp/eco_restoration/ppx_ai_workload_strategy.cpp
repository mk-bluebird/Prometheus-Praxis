// File: cpp/eco_restoration/ppx_ai_workload_strategy.cpp
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

namespace ppx::eco_restoration {

struct WorkloadFrame {
    std::string workload_id;
    std::string node_id;
    double input_tokens{};
    double output_tokens{};
    double accelerator_seconds{};
    double average_power_w{};
    double renewable_fraction{};
    double grid_carbon_g_per_kwh{};
    double ecological_value{};
    double local_heat_risk{};
    double water_risk{};
};

struct StrategyResult {
    double energy_j{};
    double energy_kwh{};
    double carbon_g{};
    double knowledge_factor{};
    double eco_impact_value{};
    double residual{};
    bool admitted{};
    std::string reason;
};

class AiWorkloadStrategy {
public:
    static constexpr double kMaxResidual = 0.20;
    static constexpr double kMinimumRenewableFraction = 0.60;
    static constexpr double kMinimumEcoImpact = 0.55;
    static constexpr double kMaximumPowerWatts = 2500.0;

    [[nodiscard]] StrategyResult evaluate(const WorkloadFrame& f) const {
        validate(f);
        const double token_count = f.input_tokens + f.output_tokens;
        const double energy_j = f.accelerator_seconds * f.average_power_w;
        const double energy_kwh = energy_j / 3'600'000.0;
        const double carbon_g = energy_kwh * (1.0 - f.renewable_fraction) *
                                f.grid_carbon_g_per_kwh;
        const double intensity_risk = clamp01(f.average_power_w / kMaximumPowerWatts);
        const double carbon_risk = clamp01(carbon_g / 100.0);
        const double token_evidence = clamp01(token_count / (token_count + 4096.0));
        const double knowledge = clamp01(
            0.45 * token_evidence + 0.35 * f.renewable_fraction +
            0.20 * (1.0 - intensity_risk));
        const double residual = intensity_risk * intensity_risk * 0.25 +
                                carbon_risk * carbon_risk * 0.30 +
                                f.local_heat_risk * f.local_heat_risk * 0.25 +
                                f.water_risk * f.water_risk * 0.20;
        const double impact = clamp01(knowledge * f.ecological_value *
                                      (1.0 - residual));
        const bool admitted = f.renewable_fraction >= kMinimumRenewableFraction &&
                              residual <= kMaxResidual &&
                              impact >= kMinimumEcoImpact;
        return {energy_j, energy_kwh, carbon_g, knowledge, impact, residual,
                admitted, admitted ? "ADMIT_ECO_WORKLOAD" : "DEFER_FOR_LOWER_IMPACT_WINDOW"};
    }

private:
    static double clamp01(double value) {
        return std::clamp(value, 0.0, 1.0);
    }

    static void validate(const WorkloadFrame& f) {
        if (f.workload_id.empty() || f.node_id.empty() || f.input_tokens < 0.0 ||
            f.output_tokens < 0.0 || f.accelerator_seconds < 0.0 ||
            f.average_power_w < 0.0 || f.grid_carbon_g_per_kwh < 0.0 ||
            outside_unit_interval(f.renewable_fraction) ||
            outside_unit_interval(f.ecological_value) ||
            outside_unit_interval(f.local_heat_risk) ||
            outside_unit_interval(f.water_risk)) {
            throw std::invalid_argument("PPX workload frame violates required bounds");
        }
    }

    static bool outside_unit_interval(double value) {
        return value < 0.0 || value > 1.0 || !std::isfinite(value);
    }
};

extern "C" int ppx_evaluate_ai_workload(
    const char* workload_id, const char* node_id, double input_tokens,
    double output_tokens, double accelerator_seconds, double average_power_w,
    double renewable_fraction, double grid_carbon_g_per_kwh,
    double ecological_value, double local_heat_risk, double water_risk,
    double* energy_j, double* carbon_g, double* eco_impact_value,
    double* residual, int* admitted) {
    try {
        const WorkloadFrame frame{
            workload_id == nullptr ? "" : workload_id,
            node_id == nullptr ? "" : node_id,
            input_tokens, output_tokens, accelerator_seconds, average_power_w,
            renewable_fraction, grid_carbon_g_per_kwh, ecological_value,
            local_heat_risk, water_risk
        };
        const StrategyResult result = AiWorkloadStrategy{}.evaluate(frame);
        if (energy_j == nullptr || carbon_g == nullptr ||
            eco_impact_value == nullptr || residual == nullptr ||
            admitted == nullptr) {
            return 2;
        }
        *energy_j = result.energy_j;
        *carbon_g = result.carbon_g;
        *eco_impact_value = result.eco_impact_value;
        *residual = result.residual;
        *admitted = result.admitted ? 1 : 0;
        return 0;
    } catch (...) {
        return 1;
    }
}

void print_sql_schema() {
    std::cout <<
R"SQL(PRAGMA foreign_keys = ON;
CREATE TABLE IF NOT EXISTS ppx_ai_workload_frame (
  workload_id TEXT PRIMARY KEY,
  node_id TEXT NOT NULL,
  observed_utc TEXT NOT NULL,
  input_tokens REAL NOT NULL CHECK(input_tokens >= 0),
  output_tokens REAL NOT NULL CHECK(output_tokens >= 0),
  energy_j REAL NOT NULL CHECK(energy_j >= 0),
  carbon_g REAL NOT NULL CHECK(carbon_g >= 0),
  k_knowledge REAL NOT NULL CHECK(k_knowledge BETWEEN 0 AND 1),
  e_eco_impact REAL NOT NULL CHECK(e_eco_impact BETWEEN 0 AND 1),
  r_residual REAL NOT NULL CHECK(r_residual BETWEEN 0 AND 1),
  fog_media_class TEXT NOT NULL CHECK(fog_media_class IN ('AIR','WATER','SOIL')),
  canal_node_parameter REAL NOT NULL CHECK(canal_node_parameter >= 0),
  CHECK(r_residual <= 0.20),
  CHECK(e_eco_impact >= 0.55)
) STRICT;
CREATE INDEX IF NOT EXISTS idx_ppx_ai_workload_node_time
ON ppx_ai_workload_frame(node_id, observed_utc DESC);
)SQL";
}

}  // namespace ppx::eco_restoration

int main(int argc, char* argv[]) {
    using namespace ppx::eco_restoration;
    if (argc == 2 && std::string(argv[1]) == "--sql-schema") {
        print_sql_schema();
        return EXIT_SUCCESS;
    }
    if (argc != 12) {
        std::cerr << "Usage: ppx_ai_workload ID NODE IN OUT SECONDS WATTS RENEWABLE "
                     "GRID_CARBON ECO_VALUE HEAT_RISK WATER_RISK\n";
        return EXIT_FAILURE;
    }
    try {
        WorkloadFrame frame{
            argv[1], argv[2], std::stod(argv[3]), std::stod(argv[4]),
            std::stod(argv[5]), std::stod(argv[6]), std::stod(argv[7]),
            std::stod(argv[8]), std::stod(argv[9]), std::stod(argv[10]),
            std::stod(argv[11])
        };
        const StrategyResult r = AiWorkloadStrategy{}.evaluate(frame);
        std::cout << std::fixed << std::setprecision(6)
                  << "workload_id=" << frame.workload_id
                  << "\tenergy_j=" << r.energy_j
                  << "\tcarbon_g=" << r.carbon_g
                  << "\tK=" << r.knowledge_factor
                  << "\tE=" << r.eco_impact_value
                  << "\tR=" << r.residual
                  << "\tdecision=" << r.reason << '\n';
        return r.admitted ? EXIT_SUCCESS : 2;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
