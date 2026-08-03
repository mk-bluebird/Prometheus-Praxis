// File: cpp/tools/scenario_template_generator.cpp
#include <iostream>
#include <fstream>
#include <string>
#include <stdexcept>

struct ScenarioTemplate {
    std::string hex_id;
    std::string canal_node;
    double surcharge_level;
};

void write_pfas_template(const ScenarioTemplate& tpl, const std::string& path) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Failed to open PFAS scenario template: " + path);
    }
    out << "# PFAS Corridor Scenario Template\n"
        << "hex_id=" << tpl.hex_id << "\n"
        << "canal_node=" << tpl.canal_node << "\n"
        << "initial_mass_kg=0.001\n"
        << "initial_sorbed_fraction=0.50\n"
        << "initial_cold_survival_factor=1.00\n"
        << "base_degradation_rate=0.01\n"
        << "cold_temp_C=12.0\n"
        << "current_temp_C=10.0\n"
        << "sorption_increment=0.001\n";
}

void write_blast_radius_template(const ScenarioTemplate& tpl, const std::string& path) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Failed to open blast-radius scenario template: " + path);
    }
    out << "# Blast-Radius Scenario Template\n"
        << "hex_id=" << tpl.hex_id << "\n"
        << "canal_node=" << tpl.canal_node << "\n"
        << "surcharge_level=" << tpl.surcharge_level << "\n"
        << "dt_s=1.0\n"
        << "dx_m=10.0\n"
        << "dy_m=10.0\n"
        << "source_energy=1000.0\n";
}

void write_workload_template(const ScenarioTemplate& tpl, const std::string& path) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Failed to open workload scenario template: " + path);
    }
    out << "# Workload Energy Scenario Template\n"
        << "hex_id=" << tpl.hex_id << "\n"
        << "canal_node=" << tpl.canal_node << "\n"
        << "flow_rate_m3_s=0.3\n"
        << "head_m=6.0\n"
        << "efficiency=0.80\n"
        << "topology_stress_norm=0.30\n"
        << "max_energy_J=5000000.0\n"
        << "max_deltaVt=1.0\n"
        << "w_energy=0.6\n"
        << "w_topology=0.4\n";
}

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "Usage: scenario_template_generator <hex_id> <canal_node> <surcharge_level>\n";
        return 1;
    }

    ScenarioTemplate tpl{};
    tpl.hex_id = argv[1];
    tpl.canal_node = argv[2];
    tpl.surcharge_level = std::stod(argv[3]);

    try {
        write_pfas_template(tpl, "scenario_pfas_" + tpl.hex_id + ".conf");
        write_blast_radius_template(tpl, "scenario_blast_" + tpl.hex_id + ".conf");
        write_workload_template(tpl, "scenario_workload_" + tpl.hex_id + ".conf");
        std::cout << "Generated PFAS, blast-radius, and workload scenario templates for hex "
                  << tpl.hex_id << " and canal node " << tpl.canal_node << "\n";
    } catch (const std::exception& ex) {
        std::cerr << "Scenario template generator error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
