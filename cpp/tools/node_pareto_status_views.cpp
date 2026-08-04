// File: cpp/tools/node_pareto_status_views.cpp

#include <stdexcept>
#include <string>
#include <sqlite3.h>

// Helper to install SQL views and node_pareto_status schema.
// Intended to be called once at service startup.
class NodeParetoSchemaInstaller {
public:
    explicit NodeParetoSchemaInstaller(sqlite3* db)
        : db_(db) {
        if (!db_) {
            throw std::runtime_error("SQLite DB pointer must not be null");
        }
    }

    void install() {
        createTelemetryBase();
        createViews();
        createParetoStatusTable();
        createParetoTrigger();
    }

private:
    sqlite3* db_;

    void execSql(const std::string& sql) {
        char* errMsg = nullptr;
        if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
            std::string msg = "SQLite error: ";
            if (errMsg) msg += errMsg;
            sqlite3_free(errMsg);
            throw std::runtime_error(msg);
        }
    }

    void createTelemetryBase() {
        // Base telemetry for latency, carbon intensity, and PFAS removal
        const char* sql =
            "CREATE TABLE IF NOT EXISTS cyboquatic_node_telemetry ("
            " node_id          TEXT NOT NULL,"
            " ts_utc           INTEGER NOT NULL,"
            " latency_ms       REAL NOT NULL,"
            " carbon_intensity REAL NOT NULL,"   // gCO2 per kWh
            " pfas_inlet       REAL NOT NULL,"   // PFAS concentration upstream
            " pfas_outlet      REAL NOT NULL,"   // PFAS concentration downstream"
            ");";
        execSql(sql);
    }

    void createViews() {
        // View: latest telemetry per node
        const char* v_latest =
            "CREATE VIEW IF NOT EXISTS v_node_latest AS "
            "SELECT node_id, "
            "       MAX(ts_utc) AS ts_utc "
            "FROM cyboquatic_node_telemetry "
            "GROUP BY node_id;";
        execSql(v_latest);

        // View: current performance metrics including PFAS removal rate
        const char* v_perf =
            "CREATE VIEW IF NOT EXISTS v_node_perf AS "
            "SELECT t.node_id, t.ts_utc, "
            "       t.latency_ms, "
            "       t.carbon_intensity, "
            "       CASE "
            "         WHEN t.pfas_inlet > 0.0 "
            "         THEN (t.pfas_inlet - t.pfas_outlet) / t.pfas_inlet "
            "         ELSE 0.0 "
            "       END AS pfas_removal_rate "
            "FROM cyboquatic_node_telemetry t "
            "JOIN v_node_latest l "
            "  ON t.node_id = l.node_id "
            " AND t.ts_utc  = l.ts_utc;";
        execSql(v_perf);

        // View: Pareto-optimal nodes (no other node strictly better in all three metrics)
        const char* v_pareto =
            "CREATE VIEW IF NOT EXISTS v_node_pareto AS "
            "SELECT p1.node_id, p1.ts_utc, "
            "       p1.latency_ms, "
            "       p1.carbon_intensity, "
            "       p1.pfas_removal_rate "
            "FROM v_node_perf p1 "
            "LEFT JOIN v_node_perf p2 "
            "  ON p2.node_id <> p1.node_id "
            " AND p2.latency_ms       <= p1.latency_ms "
            " AND p2.carbon_intensity <= p1.carbon_intensity "
            " AND p2.pfas_removal_rate >= p1.pfas_removal_rate "
            " AND (p2.latency_ms < p1.latency_ms "
            "      OR p2.carbon_intensity < p1.carbon_intensity "
            "      OR p2.pfas_removal_rate > p1.pfas_removal_rate) "
            "WHERE p2.node_id IS NULL;";
        execSql(v_pareto);
    }

    void createParetoStatusTable() {
        const char* sql =
            "CREATE TABLE IF NOT EXISTS node_pareto_status ("
            " node_id          TEXT PRIMARY KEY,"
            " ts_utc           INTEGER NOT NULL,"
            " latency_ms       REAL NOT NULL,"
            " carbon_intensity REAL NOT NULL,"
            " pfas_removal_rate REAL NOT NULL,"
            " pareto_rank      INTEGER NOT NULL,"  // rank in Pareto set (1..k)
            " updated_utc      INTEGER NOT NULL"
            ");";
        execSql(sql);
    }

    void createParetoTrigger() {
        // Trigger: whenever new telemetry is inserted, refresh node_pareto_status
        const char* trig =
            "CREATE TRIGGER IF NOT EXISTS node_pareto_refresh "
            "AFTER INSERT ON cyboquatic_node_telemetry "
            "BEGIN "
            "  DELETE FROM node_pareto_status; "
            "  INSERT INTO node_pareto_status "
            "    (node_id, ts_utc, latency_ms, carbon_intensity, "
            "     pfas_removal_rate, pareto_rank, updated_utc) "
            "  SELECT node_id, ts_utc, latency_ms, carbon_intensity, "
            "         pfas_removal_rate, "
            "         ROW_NUMBER() OVER (ORDER BY latency_ms ASC, "
            "                                   carbon_intensity ASC, "
            "                                   pfas_removal_rate DESC) AS pareto_rank, "
            "         strftime('%s','now') "
            "  FROM v_node_pareto; "
            "END;";
        execSql(trig);
    }
};
