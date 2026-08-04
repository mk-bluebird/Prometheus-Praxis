// File: cpp/eco_restoration/node_pareto_views.cpp

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

// Install schema and views for real-time Pareto front of cyboquatic nodes.
void install_node_pareto_schema(sqlite3* db) {
    // Telemetry table schema assumption:
    // node_telemetry(node_id, ts, latency_ms, carbon_intensity_gco2, pfas_removal_rate, energy_efficiency)
    const std::string sql_telemetry = R"SQL(
        CREATE TABLE IF NOT EXISTS node_telemetry (
            node_id                  TEXT NOT NULL,
            ts                       TEXT NOT NULL,
            latency_ms               REAL NOT NULL,
            carbon_intensity_gco2    REAL NOT NULL,
            pfas_removal_rate        REAL NOT NULL,
            energy_efficiency        REAL,
            PRIMARY KEY(node_id, ts)
        );
    )SQL";

    // Latest telemetry per node.
    const std::string sql_v_latest = R"SQL(
        CREATE VIEW IF NOT EXISTS v_node_latest AS
        SELECT nt.node_id,
               nt.ts,
               nt.latency_ms,
               nt.carbon_intensity_gco2,
               nt.pfas_removal_rate,
               nt.energy_efficiency
        FROM node_telemetry nt
        JOIN (
            SELECT node_id, MAX(ts) AS ts_max
            FROM node_telemetry
            GROUP BY node_id
        ) AS last
        ON nt.node_id = last.node_id AND nt.ts = last.ts_max;
    )SQL";

    // Performance view: copy latest metrics, optionally normalize or scale if needed.
    const std::string sql_v_perf = R"SQL(
        CREATE VIEW IF NOT EXISTS v_node_perf AS
        SELECT
            node_id,
            ts,
            latency_ms,
            carbon_intensity_gco2,
            pfas_removal_rate,
            energy_efficiency
        FROM v_node_latest;
    )SQL";

    // Pareto front view:
    // A node i is Pareto-optimal if there is no other node j such that:
    //   latency_ms_j <= latency_ms_i
    //   AND carbon_intensity_gco2_j <= carbon_intensity_gco2_i
    //   AND pfas_removal_rate_j >= pfas_removal_rate_i
    //   AND (strict inequality in at least one dimension).
    const std::string sql_v_pareto = R"SQL(
        CREATE VIEW IF NOT EXISTS v_node_pareto AS
        SELECT p.node_id,
               p.ts,
               p.latency_ms,
               p.carbon_intensity_gco2,
               p.pfas_removal_rate,
               p.energy_efficiency
        FROM v_node_perf p
        WHERE NOT EXISTS (
            SELECT 1
            FROM v_node_perf q
            WHERE q.node_id <> p.node_id
              AND q.latency_ms <= p.latency_ms
              AND q.carbon_intensity_gco2 <= p.carbon_intensity_gco2
              AND q.pfas_removal_rate >= p.pfas_removal_rate
              AND (
                   q.latency_ms < p.latency_ms
                OR q.carbon_intensity_gco2 < p.carbon_intensity_gco2
                OR q.pfas_removal_rate > p.pfas_removal_rate
              )
        );
    )SQL";

    // Status table: cached Pareto status per node for quick dashboard queries.
    const std::string sql_status = R"SQL(
        CREATE TABLE IF NOT EXISTS node_pareto_status (
            node_id                  TEXT PRIMARY KEY,
            ts                       TEXT NOT NULL,
            is_pareto_optimal        INTEGER NOT NULL,
            latency_ms               REAL NOT NULL,
            carbon_intensity_gco2    REAL NOT NULL,
            pfas_removal_rate        REAL NOT NULL,
            energy_efficiency        REAL,
            updated_at               TEXT NOT NULL
        );
    )SQL";

    // Trigger: refresh Pareto status on new telemetry insert.
    const std::string sql_trigger = R"SQL(
        CREATE TRIGGER IF NOT EXISTS trg_node_telemetry_insert_pareto
        AFTER INSERT ON node_telemetry
        BEGIN
            DELETE FROM node_pareto_status;

            INSERT INTO node_pareto_status(
                node_id, ts, is_pareto_optimal,
                latency_ms, carbon_intensity_gco2,
                pfas_removal_rate, energy_efficiency, updated_at
            )
            SELECT
                v.node_id,
                v.ts,
                CASE
                    WHEN v.node_id IN (
                        SELECT node_id FROM v_node_pareto
                    ) THEN 1
                    ELSE 0
                END AS is_pareto_optimal,
                v.latency_ms,
                v.carbon_intensity_gco2,
                v.pfas_removal_rate,
                v.energy_efficiency,
                datetime('now')
            FROM v_node_perf v;
        END;
    )SQL";

    exec_sql(db, sql_telemetry);
    exec_sql(db, sql_v_latest);
    exec_sql(db, sql_v_perf);
    exec_sql(db, sql_v_pareto);
    exec_sql(db, sql_status);
    exec_sql(db, sql_trigger);
}

} // namespace eco_restoration
} // namespace prometheus_praxis

int main(int argc, char* argv[]) {
    using namespace prometheus_praxis::eco_restoration;

    std::string db_path = "cyboquatic_nodes.db";
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
        install_node_pareto_schema(db);
        std::cout << "Cyboquatic node Pareto views installed into " << db_path << std::endl;

        std::cout << "\n-- Example query: latest node performance --\n";
        std::cout << "SELECT * FROM v_node_perf;\n";

        std::cout << "\n-- Example query: Pareto-optimal nodes --\n";
        std::cout << "SELECT * FROM v_node_pareto;\n";

        std::cout << "\n-- Example query: node Pareto status cache --\n";
        std::cout << "SELECT * FROM node_pareto_status ORDER BY is_pareto_optimal DESC;\n";
    } catch (const std::exception& ex) {
        std::cerr << "Node Pareto schema error: " << ex.what() << std::endl;
        sqlite3_close(db);
        return 1;
    }

    sqlite3_close(db);
    return 0;
}
