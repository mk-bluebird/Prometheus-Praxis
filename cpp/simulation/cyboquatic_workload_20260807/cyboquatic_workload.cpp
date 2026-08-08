// File: cpp/simulation/cyboquatic_workload_20260807/cyboquatic_workload.cpp
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace cyboquatic {

struct Frame {
    std::string node_id;
    double energy_req_j;
    double delta_v_t;
    double renewable_fraction;
    double recovered_energy_j;
    double water_quality_gain;
    double knowledge_factor;
    double eco_impact_value;
    bool accepted;
};

double clamp01(double value) {
    return std::clamp(value, 0.0, 1.0);
}

Frame evaluate(const std::string& node_id, double energy_req_j, double delta_v_t,
               double renewable_fraction, double recovered_energy_j,
               double water_quality_gain) {
    const double net_energy_j = std::max(0.0, energy_req_j - recovered_energy_j);
    const double renewable = clamp01(renewable_fraction);
    const double stability = std::exp(-std::max(0.0, delta_v_t));
    const double energy_quality = std::exp(-net_energy_j / 50000.0);
    const double water_gain = clamp01(water_quality_gain);

    Frame frame{};
    frame.node_id = node_id;
    frame.energy_req_j = energy_req_j;
    frame.delta_v_t = delta_v_t;
    frame.renewable_fraction = renewable;
    frame.recovered_energy_j = recovered_energy_j;
    frame.water_quality_gain = water_gain;
    frame.knowledge_factor = clamp01(
        0.35 * stability + 0.30 * renewable + 0.20 * energy_quality + 0.15 * water_gain);
    frame.eco_impact_value = clamp01(
        0.40 * stability + 0.30 * renewable + 0.20 * water_gain + 0.10 * energy_quality);
    frame.accepted = delta_v_t <= 0.0 && renewable >= 0.70 &&
                     net_energy_j <= 50000.0 && water_gain >= 0.20;
    return frame;
}

std::string csv(const Frame& f) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(6)
        << f.node_id << ',' << f.energy_req_j << ',' << f.delta_v_t << ','
        << f.renewable_fraction << ',' << f.recovered_energy_j << ','
        << f.water_quality_gain << ',' << f.knowledge_factor << ','
        << f.eco_impact_value << ',' << (f.accepted ? "ACCEPT" : "REJECT");
    return out.str();
}

}  // namespace cyboquatic

int main(int argc, char** argv) {
    if (argc != 7) {
        std::cerr << "usage: cyboquatic_workload NODE ENERGYREQJ DELTAVT RENEWABLE "
                     "RECOVEREDJ WATERQUALITYGAIN\n";
        return EXIT_FAILURE;
    }

    try {
        const auto frame = cyboquatic::evaluate(
            argv[1], std::stod(argv[2]), std::stod(argv[3]), std::stod(argv[4]),
            std::stod(argv[5]), std::stod(argv[6]));
        std::cout << "node_id,energyreq_j,delta_vt,renewable_fraction,recovered_energy_j,"
                     "water_quality_gain,knowledge_factor,eco_impact_value,decision\n"
                  << cyboquatic::csv(frame) << '\n';
        return frame.accepted ? EXIT_SUCCESS : 2;
    } catch (const std::exception&) {
        std::cerr << "all numeric inputs must be finite decimal values\n";
        return EXIT_FAILURE;
    }
}
