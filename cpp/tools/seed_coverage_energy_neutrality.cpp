// File: cpp/tools/seed_coverage_energy_neutrality.cpp
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "../eco_restoration/swarm_coverage_and_energy_neutrality.hpp"

int main() {
    try {
        using namespace eco_restoration;

        const std::vector<Point2D> positions{
            {12.0, 12.0},
            {40.0, 16.0},
            {24.0, 42.0}
        };
        const std::vector<CoverageSample> priority_samples{
            {{8.0, 8.0}, 0.90},
            {{16.0, 18.0}, 0.75},
            {{32.0, 12.0}, 0.80},
            {{45.0, 20.0}, 1.00},
            {{20.0, 35.0}, 0.85},
            {{30.0, 46.0}, 0.95},
            {{42.0, 40.0}, 0.60}
        };
        const SafeCorridor corridor{
            0.0,
            55.0,
            0.0,
            55.0,
            {{28.0, 27.0}},
            5.0,
            6.0
        };

        const CoveragePlan coverage = plan_safe_voronoi_coverage(
            positions, priority_samples, corridor, 5.0);
        if (!coverage.corridor_safe) {
            throw std::runtime_error("coverage plan does not satisfy corridor clearances");
        }

        const EnergyNeutralAssessment energy = assess_energy_neutral_plan(
            500.0,
            100.0,
            0.80,
            60.0,
            {4.0, 4.0, 5.0, 4.0},
            {2.0, 2.0, 2.0, 2.0});
        const bool energy_neutral =
            energy.state == EnergyNeutralState::EnergyNeutralPlan;

        const double knowledge_factor = energy_neutral
            ? 0.50 * coverage.knowledge_factor +
              0.50 * energy.knowledge_factor
            : 0.0;
        const double eco_impact_value = energy_neutral
            ? 0.50 * coverage.eco_impact_value +
              0.50 * energy.eco_impact_value
            : 0.0;

        std::cout << std::fixed << std::setprecision(6)
                  << "coverage_corridor_safe="
                  << (coverage.corridor_safe ? 1 : 0) << '\n'
                  << "coverage_cost=" << coverage.coverage_cost << '\n'
                  << "energy_neutral_plan=" << (energy_neutral ? 1 : 0) << '\n'
                  << "final_energy_j=" << energy.final_energy_j << '\n'
                  << "minimum_energy_j=" << energy.minimum_energy_j << '\n'
                  << "harvested_energy_j=" << energy.harvested_energy_j << '\n'
                  << "required_energy_j=" << energy.required_energy_j << '\n'
                  << "knowledge_factor=" << knowledge_factor << '\n'
                  << "eco_impact_value=" << eco_impact_value << '\n';

        return energy_neutral ? 0 : 2;
    } catch (const std::exception& error) {
        std::cerr << "seed coverage and energy-neutrality assessment failed: "
                  << error.what() << '\n';
        return 1;
    }
}
