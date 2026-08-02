// File: cpp/simulation/habitat_regeneration_simulator.cpp
#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <iomanip>
#include <cmath>

namespace eco {

struct HabitatPatch {
    std::string name;
    double area_ha;
    double initial_biodiversity_index;
    double restoration_effort_index;
};

struct RegenerationParams {
    double B_max;
    double base_r;
    double fire_lambda;
    int    years;
};

struct RegenerationResult {
    std::vector<double> biodiversity_time_series;
    std::vector<int>    fire_events_time_series;
};

double compute_biodiversity_at_time(double B_max,
                                    double B0,
                                    double r_eff,
                                    double disturbance_fraction,
                                    int t_year) {
    double one_minus_d = 1.0 - disturbance_fraction;
    if (one_minus_d < 0.0) one_minus_d = 0.0;
    double exponent = -r_eff * one_minus_d * static_cast<double>(t_year);
    double term = std::exp(exponent);
    return B_max - (B_max - B0) * term;
}

class HabitatRegenerationSimulator {
public:
    HabitatRegenerationSimulator(std::vector<HabitatPatch> patches,
                                 RegenerationParams params)
        : patches_(std::move(patches)),
          params_(params),
          rng_(std::random_device{}()),
          poisson_dist_(params.fire_lambda) {}

    RegenerationResult run_patch_simulation(const HabitatPatch& patch) {
        RegenerationResult result;
        result.biodiversity_time_series.reserve(params_.years + 1);
        result.fire_events_time_series.reserve(params_.years);

        double B0 = patch.initial_biodiversity_index;
        double B_max = params_.B_max;
        double r_eff = params_.base_r * (0.5 + 0.5 * patch.restoration_effort_index);
        if (r_eff < 0.0) r_eff = 0.0;

        result.biodiversity_time_series.push_back(B0);

        for (int year = 1; year <= params_.years; ++year) {
            int fires_this_year = poisson_dist_(rng_);
            double disturbance_fraction =
                std::min(static_cast<double>(fires_this_year) / 3.0, 1.0);

            double B_t = compute_biodiversity_at_time(
                B_max, B0, r_eff, disturbance_fraction, year
            );

            result.fire_events_time_series.push_back(fires_this_year);
            result.biodiversity_time_series.push_back(B_t);
        }

        return result;
    }

    void run_and_print_summary() {
        std::cout << std::fixed << std::setprecision(3);
        for (const auto& patch : patches_) {
            RegenerationResult res = run_patch_simulation(patch);
            std::cout << "Patch: " << patch.name
                      << " (area=" << patch.area_ha << " ha)\n";
            for (int year = 0; year <= params_.years; ++year) {
                double B_t = res.biodiversity_time_series[year];
                std::cout << "  Year " << year
                          << " B_t=" << B_t;
                if (year > 0) {
                    int fires = res.fire_events_time_series[year - 1];
                    std::cout << " fires=" << fires;
                }
                std::cout << "\n";
            }
            std::cout << "\n";
        }
    }

    void step_year_incremental() {
        for (auto& p : patches_) {
            double effort = std::clamp(p.restoration_effort_index, 0.0, 1.0);
            double growth_rate = params_.base_r * (0.5 + 0.5 * effort);
            double remaining_gap = params_.B_max - p.initial_biodiversity_index;
            if (remaining_gap < 0.0) remaining_gap = 0.0;
            double delta = growth_rate * remaining_gap;
            p.initial_biodiversity_index += delta;
            if (p.initial_biodiversity_index > params_.B_max) {
                p.initial_biodiversity_index = params_.B_max;
            }
        }
    }

    void report_incremental(int year) const {
        std::cout << "Year " << year << " habitat status:\n";
        std::cout << std::fixed << std::setprecision(3);
        for (const auto& p : patches_) {
            std::cout << "  " << p.name
                      << " | area: " << p.area_ha
                      << " ha | biodiversity: "
                      << p.initial_biodiversity_index << "\n";
        }
    }

private:
    std::vector<HabitatPatch> patches_;
    RegenerationParams params_;
    std::mt19937 rng_;
    std::poisson_distribution<int> poisson_dist_;
};

} // namespace eco

int main() {
    using namespace eco;

    std::vector<HabitatPatch> patches{
        {"Phoenix Wash A", 12.0, 0.45, 0.8},
        {"Urban Block B", 5.5, 0.30, 0.5},
        {"Riparian corridor", 15.0, 0.40, 0.8},
        {"Urban pollinator garden", 2.0, 0.60, 0.9},
        {"Reforestation plot", 50.0, 0.30, 0.7}
    };

    RegenerationParams params;
    params.B_max = 0.95;
    params.base_r = 0.12;
    params.fire_lambda = 0.3;
    params.years = 20;

    HabitatRegenerationSimulator sim(patches, params);

    sim.run_and_print_summary();

    for (int year = 0; year < 10; ++year) {
        sim.step_year_incremental();
        sim.report_incremental(year);
    }

    return 0;
}
