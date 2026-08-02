// File: cpp/tools/eco_machine_cycle_assessor.cpp
#include <iostream>
#include <string>
#include <vector>
#include "eco_restoration/eco_machine_cycle_assessor.hpp"

namespace prometheus { namespace eco {

MachineCycleScore EcoMachineCycleAssessor::assess(const MachineCycle &c) const {
    MachineCycleScore s{};
    s.knowledge_factor = 0.95;

    double energy_term = 1.0 / (1.0 + c.energy_kWh / 2.0);
    double water_term = 1.0 / (1.0 + c.water_L / 50.0);
    double emission_term = 1.0 / (1.0 + c.emissions_kg_CO2e / 1.0);
    double program_bonus = c.eco_program_used ? 0.1 : 0.0;

    s.eco_impact_value = 0.35 * energy_term
                       + 0.25 * water_term
                       + 0.30 * emission_term
                       + program_bonus;
    return s;
}

} } // namespace prometheus::eco
