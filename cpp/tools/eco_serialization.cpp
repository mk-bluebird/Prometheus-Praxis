// File: cpp/tools/eco_serialization.cpp
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <iomanip>
#include <sstream>

namespace eco_serialization {

struct EcoMetric {
    std::string node_code;
    std::string hex_id;
    double deltaVt;
    double ker_score;
    double r_hydraulics;
    double r_energy;
    double r_topology;
    double r_biodiversity;
    double pfas_mass_kg;
    double pfas_sorbed_fraction;
    double pfas_cold_survival_factor;
};

static std::string escape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '"' || c == '\\') {
            out.push_back('\\');
        }
        out.push_back(c);
    }
    return out;
}

void write_json_lines(const std::vector<EcoMetric>& metrics,
                      const std::string& path) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Failed to open JSON output: " + path);
    }
    out << std::fixed << std::setprecision(6);
    for (const auto& m : metrics) {
        out << "{"
            << "\"node_code\":\"" << escape(m.node_code) << "\","
            << "\"hex_id\":\"" << escape(m.hex_id) << "\","
            << "\"deltaVt\":" << m.deltaVt << ","
            << "\"ker_score\":" << m.ker_score << ","
            << "\"r_hydraulics\":" << m.r_hydraulics << ","
            << "\"r_energy\":" << m.r_energy << ","
            << "\"r_topology\":" << m.r_topology << ","
            << "\"r_biodiversity\":" << m.r_biodiversity << ","
            << "\"pfas_mass_kg\":" << m.pfas_mass_kg << ","
            << "\"pfas_sorbed_fraction\":" << m.pfas_sorbed_fraction << ","
            << "\"pfas_cold_survival_factor\":" << m.pfas_cold_survival_factor
            << "}\n";
    }
}

void write_csv(const std::vector<EcoMetric>& metrics,
               const std::string& path) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Failed to open CSV output: " + path);
    }
    out << "node_code,hex_id,deltaVt,ker_score,"
        << "r_hydraulics,r_energy,r_topology,r_biodiversity,"
        << "pfas_mass_kg,pfas_sorbed_fraction,pfas_cold_survival_factor\n";
    out << std::fixed << std::setprecision(6);
    for (const auto& m : metrics) {
        out << m.node_code << ","
            << m.hex_id << ","
            << m.deltaVt << ","
            << m.ker_score << ","
            << m.r_hydraulics << ","
            << m.r_energy << ","
            << m.r_topology << ","
            << m.r_biodiversity << ","
            << m.pfas_mass_kg << ","
            << m.pfas_sorbed_fraction << ","
            << m.pfas_cold_survival_factor << "\n";
    }
}

} // namespace eco_serialization

int main() {
    std::vector<eco_serialization::EcoMetric> metrics = {
        {"PHX_CANAL_NODE_A", "HEX-001", 0.42, 0.35, 0.20, 0.15, 0.10, 0.12, 0.0012, 0.55, 1.10},
        {"PHX_CANAL_NODE_B", "HEX-002", 0.38, 0.40, 0.18, 0.14, 0.09, 0.11, 0.0008, 0.52, 0.95}
    };

    eco_serialization::write_json_lines(metrics, "eco_metrics.jsonl");
    eco_serialization::write_csv(metrics, "eco_metrics.csv");

    std::cout << "Eco metrics serialized to eco_metrics.jsonl and eco_metrics.csv\n";
    return 0;
}
