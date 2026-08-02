// File: cpp/tools/eco_machine_cycle_assessor.cpp
#include <iostream>
#include <string>
#include <vector>

namespace eco {

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
    MachineCycleScore assess(const MachineCycle &c) const {
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
};

} // namespace eco

int main() {
    eco::EcoMachineCycleAssessor assessor;
    eco::MachineCycle cycle{"Front-load washer", 0.8, 45.0, 0.4, true};
    auto score = assessor.assess(cycle);
    std::cout << "Machine cycle eco-impact: " << score.eco_impact_value << "\n";
    return 0;
}
