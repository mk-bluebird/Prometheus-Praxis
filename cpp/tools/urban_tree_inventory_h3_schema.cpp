// File: cpp/tools/urban_tree_inventory_h3_schema.cpp

#include <iostream>
#include <string>
#include <stdexcept>
#include <sqlite3.h>

namespace prometheus_praxis {
namespace tools {

// Helper to execute a SQL batch with error reporting.
void exec_sql(sqlite3* db, const std::string& sql) {
    char* errmsg = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        std::string msg = errmsg ? errmsg : "";
        sqlite3_free(errmsg);
        throw std::runtime_error("SQLite error: " + msg);
    }
}

// Install schema for urban tree inventory linked to H3 hex cells and thermal recovery metrics.
void install_urban_tree_inventory_schema(sqlite3* db) {
    // Core tree inventory table:
    // - h3_index stored as TEXT (H3 string or integer encoded as text)
    // - geometry stored in an R*Tree virtual table keyed by tree_id
    // - canopy_area_m2 is per-tree canopy; species, age, health for eco-restoration analytics.
    const std::string sql_tree_inventory = R"SQL(
        CREATE TABLE IF NOT EXISTS urban_tree_inventory (
            tree_id            INTEGER PRIMARY KEY,
            species            TEXT NOT NULL,
            planted_year       INTEGER,
            health_score       REAL,            -- normalized 0..1
            canopy_area_m2     REAL NOT NULL,   -- per-tree canopy area
            h3_index           TEXT NOT NULL,   -- H3 index string for coarse location
            lat                REAL NOT NULL,
            lon                REAL NOT NULL
        );

        CREATE INDEX IF NOT EXISTS idx_tree_h3_index
            ON urban_tree_inventory(h3_index);
    )SQL";

    // R*Tree for spatial indexing of tree point geometries (lat/lon bounding box).
    // We represent each tree as a very small bounding box around its point location.
    const std::string sql_tree_rtree = R"SQL(
        CREATE VIRTUAL TABLE IF NOT EXISTS urban_tree_inventory_rtree
        USING rtree(
            tree_id,
            min_lat, max_lat,
            min_lon, max_lon
        );
    )SQL";

    // Triggers to keep R*Tree in sync with main inventory table.
    const std::string sql_tree_triggers = R"SQL(
        CREATE TRIGGER IF NOT EXISTS trg_tree_inventory_insert
        AFTER INSERT ON urban_tree_inventory
        BEGIN
            INSERT INTO urban_tree_inventory_rtree(tree_id, min_lat, max_lat, min_lon, max_lon)
            VALUES (
                NEW.tree_id,
                NEW.lat, NEW.lat,
                NEW.lon, NEW.lon
            );
        END;

        CREATE TRIGGER IF NOT EXISTS trg_tree_inventory_update
        AFTER UPDATE ON urban_tree_inventory
        BEGIN
            UPDATE urban_tree_inventory_rtree
            SET min_lat = NEW.lat,
                max_lat = NEW.lat,
                min_lon = NEW.lon,
                max_lon = NEW.lon
            WHERE tree_id = NEW.tree_id;
        END;

        CREATE TRIGGER IF NOT EXISTS trg_tree_inventory_delete
        AFTER DELETE ON urban_tree_inventory
        BEGIN
            DELETE FROM urban_tree_inventory_rtree
            WHERE tree_id = OLD.tree_id;
        END;
    )SQL";

    // Hex-level canopy aggregation view:
    // - aggregates canopy_area_m2 per H3 hex index
    // - computes average health and tree count for eco-restoration planning.
    const std::string sql_canopy_agg_view = R"SQL(
        CREATE VIEW IF NOT EXISTS hex_canopy_aggregation AS
        SELECT
            h3_index,
            COUNT(*)                    AS tree_count,
            SUM(canopy_area_m2)         AS total_canopy_m2,
            AVG(health_score)           AS avg_health_score
        FROM urban_tree_inventory
        GROUP BY h3_index;
    )SQL";

    // Hex-level thermal recovery table:
    // - assumed pre-existing or created here for completeness.
    // - stores land surface temperature (LST) metrics and recovery indicators per H3 cell.
    const std::string sql_hex_thermal_recovery = R"SQL(
        CREATE TABLE IF NOT EXISTS hex_thermal_recovery (
            h3_index              TEXT PRIMARY KEY,
            lst_baseline_c        REAL NOT NULL,  -- pre-canopy or pre-intervention LST
            lst_recent_c          REAL NOT NULL,  -- current LST
            lst_drop_c            REAL NOT NULL,  -- lst_baseline_c - lst_recent_c
            recovery_score        REAL,           -- normalized 0..1
            updated_at            TEXT NOT NULL
        );
    )SQL";

    // View joining canopy aggregation with hex thermal recovery:
    // - enables correlation analysis between canopy change and LST drop at the SQL level.
    const std::string sql_canopy_thermal_view = R"SQL(
        CREATE VIEW IF NOT EXISTS hex_canopy_thermal AS
        SELECT
            c.h3_index,
            c.tree_count,
            c.total_canopy_m2,
            c.avg_health_score,
            t.lst_baseline_c,
            t.lst_recent_c,
            t.lst_drop_c,
            t.recovery_score
        FROM hex_canopy_aggregation c
        LEFT JOIN hex_thermal_recovery t
        ON c.h3_index = t.h3_index;
    )SQL";

    // Query pattern helper views:
    // 1. Recent high-impact hexes: large canopy and strong LST drop.
    const std::string sql_high_impact_hex_view = R"SQL(
        CREATE VIEW IF NOT EXISTS hex_canopy_thermal_high_impact AS
        SELECT *
        FROM hex_canopy_thermal
        WHERE total_canopy_m2 > 1000.0
          AND lst_drop_c > 2.0;
    )SQL";

    // 2. Hexes needing intervention: low canopy and minimal LST drop.
    const std::string sql_intervention_hex_view = R"SQL(
        CREATE VIEW IF NOT EXISTS hex_canopy_thermal_intervention AS
        SELECT *
        FROM hex_canopy_thermal
        WHERE total_canopy_m2 < 200.0
          AND lst_drop_c < 0.5;
    )SQL";

    // Execute all schema pieces.
    exec_sql(db, sql_tree_inventory);
    exec_sql(db, sql_tree_rtree);
    exec_sql(db, sql_tree_triggers);
    exec_sql(db, sql_canopy_agg_view);
    exec_sql(db, sql_hex_thermal_recovery);
    exec_sql(db, sql_canopy_thermal_view);
    exec_sql(db, sql_high_impact_hex_view);
    exec_sql(db, sql_intervention_hex_view);
}

} // namespace tools
} // namespace prometheus_praxis

int main(int argc, char* argv[]) {
    using namespace prometheus_praxis::tools;

    std::string db_path = "urban_trees.db";
    if (argc > 1) {
        db_path = argv[1];
    }

    sqlite3* db = nullptr;
    int rc = sqlite3_open(db_path.c_str(), &db);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to open DB: " << sqlite3_errmsg(db) << std::endl;
        return 1;
    }

    try {
        install_urban_tree_inventory_schema(db);
        std::cout << "Urban tree inventory + H3-linked schema installed into " << db_path << std::endl;

        // Example query patterns (printed as hints for integration/testing).
        std::cout << "\n-- Example query: canopy aggregation per hex --\n";
        std::cout << "SELECT * FROM hex_canopy_aggregation LIMIT 10;\n";

        std::cout << "\n-- Example query: join canopy with thermal recovery --\n";
        std::cout << "SELECT * FROM hex_canopy_thermal WHERE lst_drop_c IS NOT NULL LIMIT 10;\n";

        std::cout << "\n-- Example query: high-impact hexes --\n";
        std::cout << "SELECT * FROM hex_canopy_thermal_high_impact ORDER BY lst_drop_c DESC LIMIT 10;\n";

        std::cout << "\n-- Example query: intervention candidates --\n";
        std::cout << "SELECT * FROM hex_canopy_thermal_intervention ORDER BY total_canopy_m2 ASC LIMIT 10;\n";
    } catch (const std::exception& ex) {
        std::cerr << "Schema installation error: " << ex.what() << std::endl;
        sqlite3_close(db);
        return 1;
    }

    sqlite3_close(db);
    return 0;
}
