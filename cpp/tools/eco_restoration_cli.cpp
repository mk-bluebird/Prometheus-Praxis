// File: cpp/tools/eco_restoration_cli.cpp
#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <cstdlib>
#include <sqlite3.h>

// Forward declarations of engines implemented elsewhere in the repo.
extern "C" {
    struct MaterialTestParamsC {
        double oxygen_depletion_percent;
        double co2_evolution_percent;
        double bod_removal_percent;
        double doc_removal_percent;
        double days_to_pass_window;
        double toxicity_score;
        double pfas_presence;
    };
    struct MaterialEcoImpactC {
        double k_safe_fraction;
        double e_eco_benefit_band;
        double r_risk_max;
        double ker_score;
        double biodegradability_score;
    };
    MaterialEcoImpactC compute_material_eco_impact(MaterialTestParamsC params);
}

namespace eco_cli {

void usage() {
    std::cout <<
        "eco_restoration_cli SUBCOMMAND [ARGS]\n\n"
        "Subcommands:\n"
        "  material-score <O2%> <CO2%> <BOD%> <DOC%> <days> <toxicity> <pfas>\n"
        "      Compute material eco-impact scores (KER + biodegradability).\n"
        "  pfas-corridor-step <db-path> <node_id>\n"
        "      Step PFAS corridor state stored in SQLite and print Lyapunov residual.\n"
        "  blast-radius <db-path>\n"
        "      Run a Phoenix canal blast-radius update using grid parameters from SQLite.\n"
        "  ker-summary <db-path>\n"
        "      Summarize KER and Lyapunov residuals over hex anchors.\n"
        "\n"
        "Notes:\n"
        "  - Uses only the C++ standard library and SQLite C API.\n"
        "  - Argument parsing is minimal: all numeric values are parsed using std::stod.\n"
        "  - DB schemas should match existing eco_restoration_shard SQLite layouts.\n";
}

void cmd_material_score(int argc, char** argv) {
    if (argc < 9) {
        std::cerr << "material-score requires 7 numeric arguments.\n";
        usage();
        std::exit(1);
    }
    MaterialTestParamsC p{};
    p.oxygen_depletion_percent = std::stod(argv[2]);
    p.co2_evolution_percent    = std::stod(argv[3]);
    p.bod_removal_percent      = std::stod(argv[4]);
    p.doc_removal_percent      = std::stod(argv[5]);
    p.days_to_pass_window      = std::stod(argv[6]);
    p.toxicity_score           = std::stod(argv[7]);
    p.pfas_presence            = std::stod(argv[8]);

    MaterialEcoImpactC impact = compute_material_eco_impact(p);

    std::cout << "Material eco-impact:\n"
              << "  k_safe_fraction=" << impact.k_safe_fraction << "\n"
              << "  e_eco_benefit_band=" << impact.e_eco_benefit_band << "\n"
              << "  r_risk_max=" << impact.r_risk_max << "\n"
              << "  ker_score=" << impact.ker_score << "\n"
              << "  biodegradability_score=" << impact.biodegradability_score << "\n";
}

void cmd_pfas_corridor_step(const std::string& db_path, const std::string& node_id) {
    sqlite3* db = nullptr;
    if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK) {
        throw std::runtime_error("Failed to open SQLite DB: " + db_path);
    }
    sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);

    const char* select_sql =
        "SELECT mass_kg, sorbed_fraction, cold_survival_factor "
        "FROM pfas_corridor_state WHERE node_id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, select_sql, -1, &stmt, nullptr) != SQLITE_OK) {
        sqlite3_close(db);
        throw std::runtime_error("Failed to prepare PFAS state query");
    }
    sqlite3_bind_text(stmt, 1, node_id.c_str(), -1, SQLITE_TRANSIENT);

    double mass_kg = 0.0;
    double sorbed_fraction = 0.0;
    double cold_survival_factor = 1.0;

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        mass_kg            = sqlite3_column_double(stmt, 0);
        sorbed_fraction    = sqlite3_column_double(stmt, 1);
        cold_survival_factor = sqlite3_column_double(stmt, 2);
    } else {
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        throw std::runtime_error("PFAS state not found for node_id=" + node_id);
    }
    sqlite3_finalize(stmt);

    // Simple update aligned with qpudatashard corridor semantics.[59]
    double base_rate = 0.01;
    double current_temp_C = 10.0; // Example: cold corridor
    double cold_temp_C = 12.0;
    double sorption_increment = 0.001;

    if (current_temp_C <= cold_temp_C) {
        cold_survival_factor *= 1.02;
    } else {
        cold_survival_factor *= 0.99;
    }
    double effective_rate = base_rate / (1.0 + cold_survival_factor);
    double next_mass = mass_kg * (1.0 - effective_rate);
    if (next_mass < 0.0) next_mass = 0.0;

    sorbed_fraction += sorption_increment;
    if (sorbed_fraction > 1.0) sorbed_fraction = 1.0;
    if (sorbed_fraction < 0.0) sorbed_fraction = 0.0;

    const char* update_sql =
        "UPDATE pfas_corridor_state "
        "SET mass_kg = ?, sorbed_fraction = ?, cold_survival_factor = ? "
        "WHERE node_id = ?;";
    sqlite3_stmt* upd = nullptr;
    if (sqlite3_prepare_v2(db, update_sql, -1, &upd, nullptr) != SQLITE_OK) {
        sqlite3_close(db);
        throw std::runtime_error("Failed to prepare PFAS state update");
    }
    sqlite3_bind_double(upd, 1, next_mass);
    sqlite3_bind_double(upd, 2, sorbed_fraction);
    sqlite3_bind_double(upd, 3, cold_survival_factor);
    sqlite3_bind_text(upd, 4, node_id.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(upd) != SQLITE_DONE) {
        sqlite3_finalize(upd);
        sqlite3_close(db);
        throw std::runtime_error("Failed to update PFAS state");
    }
    sqlite3_finalize(upd);
    sqlite3_close(db);

    std::cout << "PFAS corridor step for node_id=" << node_id
              << " mass_kg=" << next_mass
              << " sorbed_fraction=" << sorbed_fraction
              << " cold_survival_factor=" << cold_survival_factor
              << "\n";
}

void cmd_blast_radius(const std::string& db_path) {
    sqlite3* db = nullptr;
    if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK) {
        throw std::runtime_error("Failed to open SQLite DB: " + db_path);
    }
    sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);

    // Example: read grid parameters from a table phoenix_blast_grid.[59]
    const char* sql =
        "SELECT x, y, soil_diffusivity, drain_adv_x, drain_adv_y, topology_decay "
        "FROM phoenix_blast_grid ORDER BY x ASC;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        sqlite3_close(db);
        throw std::runtime_error("Failed to prepare phoenix_blast_grid query");
    }

    struct GridPoint {
        double x;
        double y;
        double soil_diffusivity;
        double drain_adv_x;
        double drain_adv_y;
        double topology_decay;
    };
    std::vector<GridPoint> grid;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        GridPoint gp{};
        gp.x               = sqlite3_column_double(stmt, 0);
        gp.y               = sqlite3_column_double(stmt, 1);
        gp.soil_diffusivity= sqlite3_column_double(stmt, 2);
        gp.drain_adv_x     = sqlite3_column_double(stmt, 3);
        gp.drain_adv_y     = sqlite3_column_double(stmt, 4);
        gp.topology_decay  = sqlite3_column_double(stmt, 5);
        grid.push_back(gp);
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    if (grid.empty()) {
        throw std::runtime_error("No blast grid points found");
    }

    // Minimal blast-radius: compute normalized risk coordinates from energy-like weights.[59]
    double hydraulic_threshold = 20.0;
    double topology_threshold = 0.5;
    double energy_threshold = 1000.0;

    double radius_energy_sum = 0.0;
    double topology_weighted_energy = 0.0;
    double max_energy = 0.0;

    for (std::size_t i = 0; i < grid.size(); ++i) {
        double E = energy_threshold * std::exp(-grid[i].topology_decay * (i + 1));
        if (E > max_energy) max_energy = E;
        double r = std::sqrt(grid[i].x * grid[i].x + grid[i].y * grid[i].y);
        radius_energy_sum += r * E;
        topology_weighted_energy += grid[i].topology_decay * E;
    }

    double r_energy = max_energy / energy_threshold;
    if (r_energy > 1.0) r_energy = 1.0;
    if (r_energy < 0.0) r_energy = 0.0;

    double avg_radius_energy = radius_energy_sum / (energy_threshold + 1e-9);
    double r_hydraulics = avg_radius_energy / hydraulic_threshold;
    if (r_hydraulics > 1.0) r_hydraulics = 1.0;
    if (r_hydraulics < 0.0) r_hydraulics = 0.0;

    double avg_topology_energy = topology_weighted_energy / (energy_threshold + 1e-9);
    double r_topology = avg_topology_energy / topology_threshold;
    if (r_topology > 1.0) r_topology = 1.0;
    if (r_topology < 0.0) r_topology = 0.0;

    std::cout << "Blast-radius risk:\n"
              << "  r_hydraulics=" << r_hydraulics << "\n"
              << "  r_topology=" << r_topology << "\n"
              << "  r_energy=" << r_energy << "\n";
}

void cmd_ker_summary(const std::string& db_path) {
    sqlite3* db = nullptr;
    if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK) {
        throw std::runtime_error("Failed to open SQLite DB: " + db_path);
    }
    sqlite3_exec(db, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr);

    // Summarize KER over hex anchors using phoenix_hex_registry.[59]
    const char* sql =
        "SELECT hex_id, r_hydraulics, r_energy, r_topology, r_biodiversity, "
        "       w_h, w_e, w_t, w_b "
        "FROM phoenix_hex_registry;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        sqlite3_close(db);
        throw std::runtime_error("Failed to prepare phoenix_hex_registry query");
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char* hex = sqlite3_column_text(stmt, 0);
        std::string hex_id = hex ? reinterpret_cast<const char*>(hex) : "";

        double r_h = sqlite3_column_double(stmt, 1);
        double r_e = sqlite3_column_double(stmt, 2);
        double r_t = sqlite3_column_double(stmt, 3);
        double r_b = sqlite3_column_double(stmt, 4);
        double w_h = sqlite3_column_double(stmt, 5);
        double w_e = sqlite3_column_double(stmt, 6);
        double w_t = sqlite3_column_double(stmt, 7);
        double w_b = sqlite3_column_double(stmt, 8);

        double Vt = w_h * r_h * r_h
                  + w_e * r_e * r_e
                  + w_t * r_t * r_t
                  + w_b * r_b * r_b;

        double k = 0.9; // Example: window K from governance band.[59]
        double e = 1.0 - std::max({r_h, r_e, r_t, r_b});
        if (e < 0.0) e = 0.0;
        double r = std::max({r_h, r_e, r_t, r_b});
        double s = k * e - r;

        std::cout << "Hex " << hex_id
                  << " Vt=" << Vt
                  << " ker_score=" << s
                  << " r_max=" << r
                  << "\n";
    }

    sqlite3_finalize(stmt);
    sqlite3_close(db);
}

} // namespace eco_cli

int main(int argc, char** argv) {
    if (argc < 2) {
        eco_cli::usage();
        return 1;
    }
    std::string sub = argv[1];
    try {
        if (sub == "material-score") {
            eco_cli::cmd_material_score(argc, argv);
        } else if (sub == "pfas-corridor-step") {
            if (argc < 4) {
                std::cerr << "pfas-corridor-step <db-path> <node_id>\n";
                return 1;
            }
            eco_cli::cmd_pfas_corridor_step(argv[2], argv[3]);
        } else if (sub == "blast-radius") {
            if (argc < 3) {
                std::cerr << "blast-radius <db-path>\n";
                return 1;
            }
            eco_cli::cmd_blast_radius(argv[2]);
        } else if (sub == "ker-summary") {
            if (argc < 3) {
                std::cerr << "ker-summary <db-path>\n";
                return 1;
            }
            eco_cli::cmd_ker_summary(argv[2]);
        } else {
            eco_cli::usage();
            return 1;
        }
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }
    return 0;
}
