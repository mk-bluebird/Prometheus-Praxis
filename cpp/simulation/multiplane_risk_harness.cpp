// File: cpp/simulation/multiplane_risk_harness.cpp
#include <iostream>
#include <vector>
#include <random>
#include <string>
#include <fstream>
#include <algorithm>

// KER/Lyapunov utilities from cpp/tools/ker_lyapunov_utils.cpp
namespace eco_tools {
    double ker_score(double k, double e, double r);
    double lyapunov_residual(const std::vector<double>& w,
                             const std::vector<double>& r);
}

struct RiskSample {
    double r_hydraulics;
    double r_energy;
    double r_topology;
    double r_biodiversity;
};

struct SimulationResult {
    double Vt;
    double ker;
    double r_max;
};

int main() {
    // Non-offsettable planes per KER docs: biodiversity and certain carbon-like channels
    // must not be compensated by improvements in other planes.[59]
    const double w_h = 0.25;
    const double w_e = 0.25;
    const double w_t = 0.25;
    const double w_b = 0.25;

    std::vector<double> weights = {w_h, w_e, w_t, w_b};

    // Monte-Carlo style simulation over risk planes.
    std::mt19937_64 rng{std::random_device{}()};
    std::uniform_real_distribution<double> dist(0.0, 1.0);

    const std::size_t n_samples = 1000;

    std::ofstream csv("multiplane_risk_summary.csv");
    csv << "sample_id,r_hydraulics,r_energy,r_topology,r_biodiversity,Vt,ker_score,r_max\n";

    for (std::size_t i = 0; i < n_samples; ++i) {
        RiskSample s{};
        s.r_hydraulics   = dist(rng);
        s.r_energy       = dist(rng);
        s.r_topology     = dist(rng);
        s.r_biodiversity = dist(rng);

        // Enforce non-offsettable rule for biodiversity: if biodiversity risk is high,
        // treat r_max as at least r_biodiversity regardless of other planes.[59]
        double r_max = std::max({s.r_hydraulics, s.r_energy, s.r_topology, s.r_biodiversity});

        std::vector<double> r = {
            s.r_hydraulics,
            s.r_energy,
            s.r_topology,
            s.r_biodiversity
        };

        double Vt = eco_tools::lyapunov_residual(weights, r);

        // Example K and E consistent with governance band:
        // K: fraction of Lyapunov-safe steps; here we approximate from Vt.[59]
        double k = (Vt < 0.5) ? 0.95 : 0.85;
        double e = 1.0 - r_max;
        if (e < 0.0) e = 0.0;
        double ker = eco_tools::ker_score(k, e, r_max);

        csv << i << ","
            << s.r_hydraulics << ","
            << s.r_energy << ","
            << s.r_topology << ","
            << s.r_biodiversity << ","
            << Vt << ","
            << ker << ","
            << r_max << "\n";
    }

    csv.close();

    std::cout << "Multiplane risk simulation completed. Summary written to multiplane_risk_summary.csv\n";
    return 0;
}
