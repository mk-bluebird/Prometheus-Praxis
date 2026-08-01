// File: cpp/eco_restoration/cyboquatic_workload_model.cpp
#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <sqlite3.h>

// Simple RAII wrapper for SQLite connections.
class SqliteDb {
public:
    explicit SqliteDb(const std::string &path) : db(nullptr) {
        if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
            throw std::runtime_error("Failed to open SQLite database");
        }
    }

    ~SqliteDb() {
        if (db) {
            sqlite3_close(db);
        }
    }

    sqlite3 *get() { return db; }

private:
    sqlite3 *db;
};

struct RiskPlanes {
    double hydraulics;
    double energy;
    double topology;
    double biodiversity;
};

struct WorkloadCycle {
    int cycle_id;
    std::string hex_id;
    std::string canal_node_id;
    double energyreqJ;
    RiskPlanes risk_before;
    RiskPlanes risk_after;
    double delta_Vt;
    double K;
    double E;
    double R;
};

// Compute Lyapunov residual V_t = sum_j w_j r_j^2.
double computeLyapunov(const RiskPlanes &rp,
                       const RiskPlanes &weights) {
    return weights.hydraulics * rp.hydraulics * rp.hydraulics +
           weights.energy * rp.energy * rp.energy +
           weights.topology * rp.topology * rp.topology +
           weights.biodiversity * rp.biodiversity * rp.biodiversity;
}

// Compute ΔVt and basic K,E,R metrics.
// K: fraction of planes that improved or stayed the same.
// E: eco-impact proxy (lower energyreqJ, lower average risk).
// R: max residual risk after the cycle.
void computeWorkloadMetrics(WorkloadCycle &cycle,
                            const RiskPlanes &weights) {
    double V_before = computeLyapunov(cycle.risk_before, weights);
    double V_after  = computeLyapunov(cycle.risk_after,  weights);
    cycle.delta_Vt  = V_after - V_before;

    int total_planes = 4;
    int safe_planes  = 0;
    if (cycle.risk_after.hydraulics <= cycle.risk_before.hydraulics) safe_planes++;
    if (cycle.risk_after.energy     <= cycle.risk_before.energy)     safe_planes++;
    if (cycle.risk_after.topology   <= cycle.risk_before.topology)   safe_planes++;
    if (cycle.risk_after.biodiversity <= cycle.risk_before.biodiversity) safe_planes++;
    cycle.K = static_cast<double>(safe_planes) / static_cast<double>(total_planes);

    double avg_risk_after =
        (cycle.risk_after.hydraulics +
         cycle.risk_after.energy +
         cycle.risk_after.topology +
         cycle.risk_after.biodiversity) / 4.0;

    // Eco-impact value: higher when energyreqJ is low and residual risk is low.
    // Normalization assumes energyreqJ up to 1e6 J and risk in [0,1].
    double energy_factor = std::max(0.0, 1.0 - cycle.energyreqJ / 1e6);
    cycle.E = energy_factor * (1.0 - avg_risk_after);

    cycle.R = std::max(
        std::max(cycle.risk_after.hydraulics, cycle.risk_after.energy),
        std::max(cycle.risk_after.topology, cycle.risk_after.biodiversity)
    );
}

// Insert a workload cycle into SQLite workload_cycle table.
void insertWorkloadCycle(SqliteDb &db, const WorkloadCycle &cycle) {
    const char *sql =
        "INSERT INTO workload_cycle("
        "hex_id, canal_node_id, energyreqJ, "
        "risk_h_before, risk_e_before, risk_t_before, risk_b_before, "
        "risk_h_after, risk_e_after, risk_t_after, risk_b_after, "
        "delta_Vt, K, E, R"
        ") VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db.get(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare INSERT statement");
    }

    int idx = 1;
    sqlite3_bind_text(stmt, idx++, cycle.hex_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, idx++, cycle.canal_node_id.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, idx++, cycle.energyreqJ);
    sqlite3_bind_double(stmt, idx++, cycle.risk_before.hydraulics);
    sqlite3_bind_double(stmt, idx++, cycle.risk_before.energy);
    sqlite3_bind_double(stmt, idx++, cycle.risk_before.topology);
    sqlite3_bind_double(stmt, idx++, cycle.risk_before.biodiversity);
    sqlite3_bind_double(stmt, idx++, cycle.risk_after.hydraulics);
    sqlite3_bind_double(stmt, idx++, cycle.risk_after.energy);
    sqlite3_bind_double(stmt, idx++, cycle.risk_after.topology);
    sqlite3_bind_double(stmt, idx++, cycle.risk_after.biodiversity);
    sqlite3_bind_double(stmt, idx++, cycle.delta_Vt);
    sqlite3_bind_double(stmt, idx++, cycle.K);
    sqlite3_bind_double(stmt, idx++, cycle.E);
    sqlite3_bind_double(stmt, idx++, cycle.R);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to execute INSERT statement");
    }

    sqlite3_finalize(stmt);
}

// Simple CLI: compute metrics and insert a single cycle.
int main(int argc, char **argv) {
    if (argc < 3) {
        std::cerr << "Usage: cyboquatic_workload_model <db_path> <hex_id>\n";
        return 1;
    }
    std::string db_path = argv[1];
    std::string hex_id  = argv[2];

    try {
        SqliteDb db(db_path);

        WorkloadCycle cycle{};
        cycle.hex_id = hex_id;
        cycle.canal_node_id = "canal_node_001"; // In practice, derive from phoenix_hex_registry.
        cycle.energyreqJ = 25000.0; // Example: 25 kJ per cycle.

        // Example risk coordinates before and after.
        cycle.risk_before = {0.3, 0.4, 0.2, 0.1};
        cycle.risk_after  = {0.28, 0.35, 0.22, 0.1};

        RiskPlanes weights = {1.0, 1.0, 1.0, 1.0};
        computeWorkloadMetrics(cycle, weights);

        insertWorkloadCycle(db, cycle);

        std::cout << std::fixed << std::setprecision(4);
        std::cout << "Inserted workload cycle for hex " << cycle.hex_id << "\n";
        std::cout << "energyreqJ=" << cycle.energyreqJ
                  << " ΔVt=" << cycle.delta_Vt
                  << " K=" << cycle.K
                  << " E=" << cycle.E
                  << " R=" << cycle.R << "\n";

    } catch (const std::exception &ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
