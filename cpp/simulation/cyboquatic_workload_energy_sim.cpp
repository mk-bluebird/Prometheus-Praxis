// File: cpp/simulation/cyboquatic_workload_energy_sim.cpp
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <stdexcept>
#include <iomanip>

namespace cyboquatic {

struct WorkloadSample {
    double timestamp_s;      // seconds since start
    double flow_rate_m3_s;   // volumetric flow through machinery
    double head_m;           // hydraulic head / vertical lift
    double efficiency;       // pump/machine efficiency [0,1]
};

struct WorkloadResult {
    double energyreqJ;       // required mechanical energy in Joules
    double energy_input_J;   // input energy accounting for efficiency
    double deltaVt;          // Lyapunov-like workload residual
};

class WorkloadCorridor {
public:
    WorkloadCorridor(double maxEnergyJ,
                     double maxDeltaVt,
                     double ecoWeightEnergy,
                     double ecoWeightTopology)
        : maxEnergyJ_(maxEnergyJ),
          maxDeltaVt_(maxDeltaVt),
          w_energy_(ecoWeightEnergy),
          w_topology_(ecoWeightTopology) {
        if (maxEnergyJ_ <= 0.0 || maxDeltaVt_ <= 0.0) {
            throw std::invalid_argument("Corridor limits must be positive");
        }
        if (w_energy_ <= 0.0 || w_topology_ <= 0.0) {
            throw std::invalid_argument("Eco weights must be positive");
        }
    }

    WorkloadResult evaluate(const WorkloadSample& sample,
                            double topologyStressNorm) const {
        // Mechanical energy requirement: E = rho * g * Q * h * dt.
        // Here we treat flow_rate_m3_s * head_m as proxy at dt=1s.
        constexpr double rho = 1000.0;       // kg/m3 (water)
        constexpr double g   = 9.80665;      // m/s2

        double dt = 1.0;
        double mechEnergyJ = rho * g * sample.flow_rate_m3_s * sample.head_m * dt;
        if (mechEnergyJ < 0.0) mechEnergyJ = 0.0;

        double eff = sample.efficiency;
        if (eff <= 0.0) eff = 1e-6;
        if (eff > 1.0) eff = 1.0;

        double energyInputJ = mechEnergyJ / eff;

        // Lyapunov-like workload residual aggregates normalized planes.
        double r_energy = mechEnergyJ / maxEnergyJ_;
        if (r_energy > 1.0) r_energy = 1.0;

        double r_topology = topologyStressNorm;
        if (r_topology < 0.0) r_topology = 0.0;
        if (r_topology > 1.0) r_topology = 1.0;

        double deltaVt = w_energy_ * r_energy * r_energy
                       + w_topology_ * r_topology * r_topology;

        // Hard corridor: reject if residual exceeds 1.
        if (deltaVt > 1.0) {
            throw std::runtime_error("Workload corridor breach: deltaVt > 1.0");
        }

        return WorkloadResult{mechEnergyJ, energyInputJ, deltaVt};
    }

private:
    double maxEnergyJ_;
    double maxDeltaVt_;
    double w_energy_;
    double w_topology_;
};

void run_demo() {
    WorkloadCorridor corridor(5.0e6, 1.0, 0.6, 0.4);

    std::vector<WorkloadSample> samples = {
        {0.0, 0.2, 5.0, 0.75},
        {60.0, 0.35, 6.0, 0.78},
        {120.0, 0.5, 7.0, 0.8}
    };

    std::cout << std::fixed << std::setprecision(3);
    for (const auto& s : samples) {
        double topoStress = 0.3; // example normalized canal/topology stress
        try {
            WorkloadResult r = corridor.evaluate(s, topoStress);
            std::cout << "t=" << s.timestamp_s
                      << "s energyreqJ=" << r.energyreqJ
                      << " energyInputJ=" << r.energy_input_J
                      << " deltaVt=" << r.deltaVt << "\n";
        } catch (const std::exception& ex) {
            std::cerr << "Corridor violation at t=" << s.timestamp_s
                      << "s: " << ex.what() << "\n";
        }
    }
}

} // namespace cyboquatic

int main() {
    cyboquatic::run_demo();
    return 0;
}
