// File: cpp/eco_restoration/hex_action_log_schema.cpp

#include <iostream>
#include <string>
#include <stdexcept>
#include <sqlite3.h>

namespace prometheus_praxis {
namespace eco_restoration {

void exec_sql(sqlite3* db, const std::string& sql) {
    char* errmsg = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        std::string msg = errmsg ? errmsg : "";
        sqlite3_free(errmsg);
        throw std::runtime_error("SQLite error: " + msg);
    }
}

// Install schema for hex action log, trigger-based aggregation, and cumulative carbon cost view.
void install_hex_action_log_schema(sqlite3* db) {
    // Log of actions applied to each hex cell.
    const std::string sql_hex_action_log = R"SQL(
        CREATE TABLE IF NOT EXISTS hex_action_log (
            id                  INTEGER PRIMARY KEY AUTOINCREMENT,
            h3_index            TEXT NOT NULL,
            ts                  TEXT NOT NULL,
            action_type         TEXT NOT NULL,   -- e.g., 'PLANT_TREE', 'REMOVE_ASPHALT', 'ADD_CANOPY'
            carbon_cost_kgco2   REAL NOT NULL,   -- positive = emission, negative = reduction
            notes               TEXT
        );

        CREATE INDEX IF NOT EXISTS idx_hex_action_log_h3_ts
            ON hex_action_log(h3_index, ts);
    )SQL";

    // Extend hex_restoration_commitment with progress and cumulative carbon fields if needed.
    const std::string sql_commitment_extend = R"SQL(
        ALTER TABLE hex_restoration_commitment
        ADD COLUMN action_count INTEGER DEFAULT 0;
    )SQL";
    char* errmsg = nullptr;
    int rc = sqlite3_exec(db, sql_commitment_extend.c_str(), nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        sqlite3_free(errmsg);
    }

    const std::string sql_commitment_extend_carbon = R"SQL(
        ALTER TABLE hex_restoration_commitment
        ADD COLUMN cumulative_carbon_cost_kgco2 REAL DEFAULT 0.0;
    )SQL";
    rc = sqlite3_exec(db, sql_commitment_extend_carbon.c_str(), nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        sqlite3_free(errmsg);
    }

    // Trigger: whenever an action is logged, update per-hex progress and cumulative carbon cost.
    const std::string sql_trigger_update_commitment = R"SQL(
        CREATE TRIGGER IF NOT EXISTS trg_hex_action_log_insert_update_commitment
        AFTER INSERT ON hex_action_log
        BEGIN
            UPDATE hex_restoration_commitment
            SET action_count = COALESCE(action_count, 0) + 1,
                cumulative_carbon_cost_kgco2 = COALESCE(cumulative_carbon_cost_kgco2, 0.0) + NEW.carbon_cost_kgco2
            WHERE h3_index = NEW.h3_index;
        END;
    )SQL";

    // Recursive CTE view: cumulative carbon cost per hex over time.
    // For each hex, order actions by ts and compute running sum.
    const std::string sql_cumulative_view = R"SQL(
        CREATE VIEW IF NOT EXISTS hex_cumulative_carbon_cost AS
        WITH ordered_actions AS (
            SELECT
                h3_index,
                ts,
                carbon_cost_kgco2,
                ROW_NUMBER() OVER (PARTITION BY h3_index ORDER BY ts ASC) AS n
            FROM hex_action_log
        ),
        cumulative AS (
            SELECT
                h3_index,
                ts,
                n,
                carbon_cost_kgco2 AS cumulative_cost_kgco2
            FROM ordered_actions
            WHERE n = 1

            UNION ALL

            SELECT
                oa.h3_index,
                oa.ts,
                oa.n,
                cumulative.cumulative_cost_kgco2 + oa.carbon_cost_kgco2 AS cumulative_cost_kgco2
            FROM ordered_actions oa
            JOIN cumulative
              ON oa.h3_index = cumulative.h3_index
             AND oa.n = cumulative.n + 1
        )
        SELECT
            h3_index,
            ts,
            cumulative_cost_kgco2
        FROM cumulative;
    )SQL";

    exec_sql(db, sql_hex_action_log);
    exec_sql(db, sql_trigger_update_commitment);
    exec_sql(db, sql_cumulative_view);
}

} // namespace eco_restoration
} // namespace prometheus_praxis

int main(int argc, char* argv[]) {
    using namespace prometheus_praxis::eco_restoration;

    std::string db_path = "hex_actions.db";
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
        install_hex_action_log_schema(db);
        std::cout << "Hex action log schema and cumulative carbon cost view installed into "
                  << db_path << std::endl;

        std::cout << "\n-- Example query: latest actions per hex --\n";
        std::cout << "SELECT * FROM hex_action_log ORDER BY ts DESC LIMIT 10;\n";

        std::cout << "\n-- Example query: cumulative carbon cost per hex over time --\n";
        std::cout << "SELECT * FROM hex_cumulative_carbon_cost ORDER BY h3_index, ts LIMIT 20;\n";

        std::cout << "\n-- Example query: per-hex progress and cumulative carbon cost --\n";
        std::cout << "SELECT h3_index, action_count, cumulative_carbon_cost_kgco2 "
                  << "FROM hex_restoration_commitment LIMIT 10;\n";
    } catch (const std::exception& ex) {
        std::cerr << "Hex action log schema error: " << ex.what() << std::endl;
        sqlite3_close(db);
        return 1;
    }

    sqlite3_close(db);
    return 0;
}
