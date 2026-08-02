// File: cpp/optimization/system_workload_optimizer.cpp
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <cmath>
#include <stdexcept>
#include <sqlite3.h>

struct SystemNode {
    int node_id;
    std::string hex_id;
    std::string process_stage;
};

struct SystemAsset {
    int asset_id;
    std::string asset_type;
    double rated_flow_m3h;
    double energy_efficiency_kWh_per_m3;
    double carbon_factor_kgCO2_per_kWh;
};

struct WorkloadDemand {
    int node_id;
    double flow_m3h;
};

struct AssignmentDecision {
    int node_id;
    int asset_id;
    double assigned_flow_m3h;
    double energy_kWh;
    double carbon_kgCO2;
};

class SqliteDb {
public:
    explicit SqliteDb(const std::string &path) {
        if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
            throw std::runtime_error("Failed to open database");
        }
    }
    ~SqliteDb() {
        if (db_) {
            sqlite3_close(db_);
        }
    }
    sqlite3 *get() { return db_; }
private:
    sqlite3 *db_;
};

static std::vector<SystemNode> loadNodes(SqliteDb &db) {
    std::vector<SystemNode> nodes;
    const char *sql = "SELECT node_id, hex_id, process_stage FROM system_node;";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db.get(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare node query");
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        SystemNode n;
        n.node_id = sqlite3_column_int(stmt, 0);
        n.hex_id = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
        n.process_stage = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2));
        nodes.push_back(n);
    }
    sqlite3_finalize(stmt);
    return nodes;
}

static std::vector<SystemAsset> loadAssets(SqliteDb &db) {
    std::vector<SystemAsset> assets;
    const char *sql = "SELECT asset_id, asset_type, rated_flow_m3h, "
                      "energy_efficiency_kWh_per_m3, carbon_factor_kgCO2_per_kWh "
                      "FROM system_asset;";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db.get(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare asset query");
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        SystemAsset a;
        a.asset_id = sqlite3_column_int(stmt, 0);
        a.asset_type = reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1));
        a.rated_flow_m3h = sqlite3_column_double(stmt, 2);
        a.energy_efficiency_kWh_per_m3 = sqlite3_column_double(stmt, 3);
        a.carbon_factor_kgCO2_per_kWh = sqlite3_column_double(stmt, 4);
        assets.push_back(a);
    }
    sqlite3_finalize(stmt);
    return assets;
}

static std::vector<WorkloadDemand> loadDemands(SqliteDb &db, const std::string &window_label) {
    std::vector<WorkloadDemand> demands;
    const char *sql = "SELECT node_id, SUM(flow_m3h) "
                      "FROM system_workload "
                      "WHERE window_label = ? "
                      "GROUP BY node_id;";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db.get(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare workload query");
    }
    sqlite3_bind_text(stmt, 1, window_label.c_str(), -1, SQLITE_TRANSIENT);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        WorkloadDemand d;
        d.node_id = sqlite3_column_int(stmt, 0);
        d.flow_m3h = sqlite3_column_double(stmt, 1);
        demands.push_back(d);
    }
    sqlite3_finalize(stmt);
    return demands;
}

static void saveAssignments(SqliteDb &db,
                            const std::string &window_label,
                            const std::vector<AssignmentDecision> &assignments) {
    const char *sql = "INSERT INTO system_workload_plan("
                      "window_label, node_id, asset_id, "
                      "assigned_flow_m3h, energy_kWh, carbon_kgCO2"
                      ") VALUES (?,?,?,?,?,?);";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db.get(), sql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error("Failed to prepare plan insert");
    }
    for (const auto &dec : assignments) {
        int idx = 1;
        sqlite3_bind_text(stmt, idx++, window_label.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, idx++, dec.node_id);
        sqlite3_bind_int(stmt, idx++, dec.asset_id);
        sqlite3_bind_double(stmt, idx++, dec.assigned_flow_m3h);
        sqlite3_bind_double(stmt, idx++, dec.energy_kWh);
        sqlite3_bind_double(stmt, idx++, dec.carbon_kgCO2);
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_reset(stmt);
            throw std::runtime_error("Failed to insert workload plan");
        }
        sqlite3_reset(stmt);
    }
    sqlite3_finalize(stmt);
}

static std::vector<AssignmentDecision> optimizeWorkload(
        const std::vector<SystemNode> &nodes,
        const std::vector<SystemAsset> &assets,
        const std::vector<WorkloadDemand> &demands) {

    std::vector<AssignmentDecision> decisions;
    std::map<int, std::vector<SystemAsset>> assetsByStage;

    for (const auto &a : assets) {
        assetsByStage[a.asset_type].push_back(a);
    }

    for (const auto &d : demands) {
        auto nodeIt = std::find_if(nodes.begin(), nodes.end(),
                                   [&](const SystemNode &n){ return n.node_id == d.node_id; });
        if (nodeIt == nodes.end()) {
            continue;
        }
        const std::string &stage = nodeIt->process_stage;
        auto assetListIt = assetsByStage.find(stage);
        if (assetListIt == assetsByStage.end() || assetListIt->second.empty()) {
            continue;
        }
        const auto &stageAssets = assetListIt->second;

        double remainingFlow = d.flow_m3h;
        std::vector<SystemAsset> sortedAssets = stageAssets;
        std::sort(sortedAssets.begin(), sortedAssets.end(),
                  [](const SystemAsset &a, const SystemAsset &b) {
                      return a.energy_efficiency_kWh_per_m3 < b.energy_efficiency_kWh_per_m3;
                  });

        for (const auto &a : sortedAssets) {
            if (remainingFlow <= 0.0) {
                break;
            }
            double assignFlow = std::min(remainingFlow, a.rated_flow_m3h);
            double energy = assignFlow * a.energy_efficiency_kWh_per_m3;
            double carbon = energy * a.carbon_factor_kgCO2_per_kWh;

            AssignmentDecision dec;
            dec.node_id = d.node_id;
            dec.asset_id = a.asset_id;
            dec.assigned_flow_m3h = assignFlow;
            dec.energy_kWh = energy;
            dec.carbon_kgCO2 = carbon;
            decisions.push_back(dec);

            remainingFlow -= assignFlow;
        }
    }

    return decisions;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        std::cerr << "Usage: system_workload_optimizer <db_path> <window_label>\n";
        return 1;
    }
    std::string dbPath = argv[1];
    std::string windowLabel = argv[2];

    try {
        SqliteDb db(dbPath);
        auto nodes = loadNodes(db);
        auto assets = loadAssets(db);
        auto demands = loadDemands(db, windowLabel);

        auto decisions = optimizeWorkload(nodes, assets, demands);
        saveAssignments(db, windowLabel, decisions);

        double totalEnergy = 0.0;
        double totalCarbon = 0.0;
        for (const auto &dec : decisions) {
            totalEnergy += dec.energy_kWh;
            totalCarbon += dec.carbon_kgCO2;
        }

        std::cout << "Workload plan stored for window " << windowLabel << "\n";
        std::cout << "Total energy (kWh): " << totalEnergy << "\n";
        std::cout << "Total carbon (kg CO2e): " << totalCarbon << "\n";

    } catch (const std::exception &ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
