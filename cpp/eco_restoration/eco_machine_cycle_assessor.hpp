// File: cpp/eco_restoration/eco_machine_cycle_assessor.hpp
#pragma once

#include <string>

namespace prometheus { namespace eco {

struct MachineCycle {
    std::string machine_name;
    double energy_kWh;
    double water_L;
    double emissions_kg_CO2e;
    bool eco_program_used;
};

struct MachineCycleScore {
    double knowledge_factor;
    double eco_impact_value;
};

class EcoMachineCycleAssessor {
public:
    MachineCycleScore assess(const MachineCycle &c) const;
};

} } // namespace prometheus::eco
