// File: cpp/simulation/cyboquatic_workload_energy_sim.cpp
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <stdexcept>
#include <iomanip>
#include <cstdlib>
#include <sstream>

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

// Simulate a single workload step given a sample and topology stress
WorkloadResult simulate_step(const WorkloadSample& sample, double topoStressNorm) {
    WorkloadCorridor corridor(5.0e6, 1.0, 0.6, 0.4);
    return corridor.evaluate(sample, topoStressNorm);
}

// Compute FOG route from deltaVt and topo_stress_norm
std::string compute_fog_route(double deltaVt, double topoStressNorm) {
    // High PFAS/topology stress maps to FOG:COLD_SURVIVAL_MONITOR
    // Low stress with safe deltaVt maps to FOG:RESTORATION_PREFERRED
    if (topoStressNorm >= 0.5 || deltaVt > 0.7) {
        return "FOG:COLD_SURVIVAL_MONITOR";
    } else if (deltaVt < 0.3 && topoStressNorm < 0.2) {
        return "FOG:RESTORATION_PREFERRED";
    } else {
        return "FOG:NEEDS_DIAGNOSTIC";
    }
}

// Persist telemetry result to SQLite using shell invocation
void persist_to_sqlite(const WorkloadResult& result, double topoStressNorm,
                       const std::string& canalNode, const std::string& dbPath) {
    // Compute derived values for telemetry
    std::string fogRoute = compute_fog_route(result.deltaVt, topoStressNorm);
    
    // Approximate environmental readings based on workload characteristics
    // Higher energy input -> higher temperature; higher topo stress -> higher PFAS
    double canalTempC = 15.0 + (result.energy_input_J / 1e6) * 2.0;  // baseline 15C + heating
    double pfasUgL = 0.05 + topoStressNorm * 0.3;  // baseline 0.05, up to 0.35 ug/L
    
    // Escape single quotes in strings for SQL safety
    std::string escapedNode = canalNode;
    size_t pos = 0;
    while ((pos = escapedNode.find("'", pos)) != std::string::npos) {
        escapedNode.replace(pos, 1, "''");
        pos += 2;
    }
    
    std::ostringstream sql;
    sql << std::fixed << std::setprecision(6);
    sql << "sqlite3 \"" << dbPath << "\" \"INSERT INTO cyboquatic_workload_telemetry "
        << "(node_id, timestamp_utc, energyreqJ, energy_input_J, deltaVt, "
        << "topo_stress_norm, canal_temperature_C, pfas_concentration_ugL, fog_route) "
        << "VALUES ("
        << "(SELECT node_id FROM canal_node WHERE node_code = '" << escapedNode << "' LIMIT 1), "
        << "strftime('%Y-%m-%dT%H:%M:%SZ','now'), "
        << result.energyreqJ << ", "
        << result.energy_input_J << ", "
        << result.deltaVt << ", "
        << topoStressNorm << ", "
        << canalTempC << ", "
        << pfasUgL << ", "
        << "'" << fogRoute << "'"
        << ");\"";
    
    std::string cmd = sql.str();
    int ret = std::system(cmd.c_str());
    if (ret != 0) {
        std::cerr << "Warning: sqlite3 insert returned non-zero: " << ret << "\n";
    }
}

// Ensure canal_node exists (insert or ignore)
void ensure_canal_node(const std::string& canalNode, const std::string& dbPath) {
    std::string escapedNode = canalNode;
    size_t pos = 0;
    while ((pos = escapedNode.find("'", pos)) != std::string::npos) {
        escapedNode.replace(pos, 1, "''");
        pos += 2;
    }
    
    std::ostringstream sql;
    sql << "sqlite3 \"" << dbPath << "\" \"INSERT OR IGNORE INTO canal_node "
        << "(node_code, description, ker_band, fog_band, canal_plane, active) "
        << "VALUES ('" << escapedNode << "', 'Test canal node for Cyboquatic workload', "
        << "'EXPPROD', 'RESTORATION_PREFERRED', 'HYDRAULICS', 1);\"";
    
    std::string cmd = sql.str();
    int ret = std::system(cmd.c_str());
    if (ret != 0) {
        std::cerr << "Warning: sqlite3 node insert returned non-zero: " << ret << "\n";
    }
}

} // namespace cyboquatic

int main(int argc, char* argv[]) {
    std::string dbPath = "eco_restoration_workload.sqlite";
    std::string canalNode = "PHX_CANAL_NODE_A";
    
    // Parse --db-path and --canal-node arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.find("--db-path=") == 0) {
            dbPath = arg.substr(10);
        } else if (arg.find("--canal-node=") == 0) {
            canalNode = arg.substr(13);
        }
    }
    
    std::cout << "Cyboquatic Workload Energy Simulator\n";
    std::cout << "DB Path: " << dbPath << "\n";
    std::cout << "Canal Node: " << canalNode << "\n\n";
    
    // Ensure canal_node exists in DB
    cyboquatic::ensure_canal_node(canalNode, dbPath);
    
    // Sample workload data with varying topology stress
    std::vector<cyboquatic::WorkloadSample> samples = {
        {0.0, 0.2, 5.0, 0.75},
        {60.0, 0.35, 6.0, 0.78},
        {120.0, 0.5, 7.0, 0.8}
    };
    
    // Topology stress values simulating different conditions
    std::vector<double> topoStressValues = {0.15, 0.35, 0.55};
    
    std::cout << std::fixed << std::setprecision(3);
    for (size_t i = 0; i < samples.size(); ++i) {
        const auto& s = samples[i];
        double topoStress = topoStressValues[i % topoStressValues.size()];
        
        try {
            cyboquatic::WorkloadResult r = cyboquatic::simulate_step(s, topoStress);
            std::cout << "t=" << s.timestamp_s
                      << "s energyreqJ=" << r.energyreqJ
                      << " energyInputJ=" << r.energy_input_J
                      << " deltaVt=" << r.deltaVt
                      << " topoStress=" << topoStress << "\n";
            
            // Persist to SQLite
            cyboquatic::persist_to_sqlite(r, topoStress, canalNode, dbPath);
            std::cout << "  -> Persisted to SQLite\n";
        } catch (const std::exception& ex) {
            std::cerr << "Corridor violation at t=" << s.timestamp_s
                      << "s: " << ex.what() << "\n";
        }
    }
    
    std::cout << "\nSimulation complete. Telemetry written to " << dbPath << "\n";
    return 0;
}
