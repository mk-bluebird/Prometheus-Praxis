// File: cpp/simulation/energy_usage_analyzer.cpp
#include <iostream>
#include <vector>
#include <string>
#include <numeric>

namespace eco {

struct DeviceCycle {
    std::string device_name;
    double energy_kWh;
    bool eco_mode_enabled;
};

class EnergyUsageAnalyzer {
public:
    double compute_eco_fraction(const std::vector<DeviceCycle> &cycles) const {
        double eco_energy = 0.0;
        double total_energy = 0.0;
        for (const auto &c : cycles) {
            total_energy += c.energy_kWh;
            if (c.eco_mode_enabled) eco_energy += c.energy_kWh;
        }
        if (total_energy == 0.0) return 0.0;
        return eco_energy / total_energy;
    }

    double compute_total_energy(const std::vector<DeviceCycle> &cycles) const {
        double total = 0.0;
        for (const auto &c : cycles) total += c.energy_kWh;
        return total;
    }
};

} // namespace eco

int main() {
    std::vector<eco::DeviceCycle> cycles{
        {"Washer", 1.2, true},
        {"Dryer", 2.5, false},
        {"Dishwasher", 1.0, true}
    };
    eco::EnergyUsageAnalyzer analyzer;
    std::cout << "Total energy: " << analyzer.compute_total_energy(cycles) << " kWh\n";
    std::cout << "Eco fraction: " << analyzer.compute_eco_fraction(cycles) << "\n";
    return 0;
}
