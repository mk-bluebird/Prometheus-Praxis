// File: cpp/tools/pfas_change_point_detection_sql.cpp

#include <iostream>
#include <string>
#include <stdexcept>
#include <sqlite3.h>

// This file installs SQL schema and logic for PFAS change-point detection using:
//   - CUSUM via a recursive CTE.
//   - Moving window mean/std via window functions.
//   - Triggers on insert to update a corridor flag table and an alert table.
// It also sketches an MQTT publisher service hook (as a simple stdout placeholder).

namespace prometheus_praxis {
namespace tools {

void exec_sql(sqlite3* db, const std::string& sql) {
    char* errmsg = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        std::string msg = errmsg ? errmsg : "";
        sqlite3_free(errmsg);
        throw std::runtime_error("SQLite error: " + msg);
    }
}

// Install schema: telemetry, CUSUM params, alerts, corridor flags.
void install_schema(sqlite3* db) {
    const std::string sql_telemetry = R"SQL(
        CREATE TABLE IF NOT EXISTS pfas_telemetry (
            id            INTEGER PRIMARY KEY AUTOINCREMENT,
            ts            TEXT NOT NULL,
            segment_id    INTEGER NOT NULL,
            pfas_ugL      REAL NOT NULL
        );

        CREATE INDEX IF NOT EXISTS idx_pfas_telemetry_ts_segment
            ON pfas_telemetry(segment_id, ts);
    )SQL";

    const std::string sql_params = R"SQL(
        CREATE TABLE IF NOT EXISTS pfas_cusum_params (
            segment_id     INTEGER PRIMARY KEY,
            k              REAL NOT NULL,  -- reference value (drift sensitivity)
            h              REAL NOT NULL   -- decision threshold
        );
    )SQL";

    const std::string sql_alerts = R"SQL(
        CREATE TABLE IF NOT EXISTS pfas_change_alert (
            id            INTEGER PRIMARY KEY AUTOINCREMENT,
            ts            TEXT NOT NULL,
            segment_id    INTEGER NOT NULL,
            alert_type    TEXT NOT NULL,   -- e.g., 'CUSUM', 'BAYES_Z'
            pfas_ugL      REAL NOT NULL,
            cusum_value   REAL,
            z_score       REAL,
            handled       INTEGER NOT NULL DEFAULT 0
        );
    )SQL";

    const std::string sql_corridor_flag = R"SQL(
        CREATE TABLE IF NOT EXISTS pfas_corridor_flag (
            segment_id    INTEGER PRIMARY KEY,
            flagged       INTEGER NOT NULL DEFAULT 0,
            flagged_at    TEXT
        );
    )SQL";

    exec_sql(db, sql_telemetry);
    exec_sql(db, sql_params);
    exec_sql(db, sql_alerts);
    exec_sql(db, sql_corridor_flag);
}

// View: moving window mean and std dev for Bayesian-like Z-score approximation.
// Uses a window of last N samples per segment.
void install_moving_stats_view(sqlite3* db, int window_size) {
    std::string sql_view = R"SQL(
        CREATE VIEW IF NOT EXISTS pfas_moving_stats AS
        SELECT
            id,
            ts,
            segment_id,
            pfas_ugL,
            AVG(pfas_ugL) OVER (
                PARTITION BY segment_id
                ORDER BY ts
                ROWS BETWEEN %N PRECEDING AND CURRENT ROW
            ) AS mu_win,
            CASE
                WHEN COUNT(pfas_ugL) OVER (
                    PARTITION BY segment_id
                    ORDER BY ts
                    ROWS BETWEEN %N PRECEDING AND CURRENT ROW
                ) > 1
                THEN
                    STDDEV_SAMP(pfas_ugL) OVER (
                        PARTITION BY segment_id
                        ORDER BY ts
                        ROWS BETWEEN %N PRECEDING AND CURRENT ROW
                    )
                ELSE NULL
            END AS sigma_win
        FROM pfas_telemetry;
    )SQL";

    // Replace %N with window_size-1 because ROWS BETWEEN N PRECEDING uses number of rows.
    std::string n_str = std::to_string(window_size - 1);
    auto pos = sql_view.find("%N");
    while (pos != std::string::npos) {
        sql_view.replace(pos, 2, n_str);
        pos = sql_view.find("%N", pos + 1);
    }

    exec_sql(db, sql_view);
}

// Recursive CTE for CUSUM per segment.
// We create a view computing CUSUM+ (upper) for each segment using segment-specific k and h.
//
// CUSUM definition:
//   Let X_t be pfas_ugL, μ0 baseline mean, k reference value.
//   S_t = max(0, S_{t-1} + (X_t - μ0 - k))
// We approximate μ0 via segment baseline (e.g., global mean) stored in params or treat it as k.
//
// Here we use:
//   S_t = max(0, S_{t-1} + X_t - k)
//
// Decision: S_t >= h → change detected.
void install_cusum_view(sqlite3* db) {
    const std::string sql_view = R"SQL(
        CREATE VIEW IF NOT EXISTS pfas_cusum AS
        WITH RECURSIVE cusum(seg, id, ts, pfas_ugL, k, h, S_t) AS (
            -- Seed: earliest telemetry per segment
            SELECT
                t.segment_id AS seg,
                t.id,
                t.ts,
                t.pfas_ugL,
                p.k,
                p.h,
                MAX(0.0, t.pfas_ugL - p.k) AS S_t
            FROM pfas_telemetry t
            JOIN pfas_cusum_params p ON p.segment_id = t.segment_id
            WHERE t.id = (
                SELECT MIN(id) FROM pfas_telemetry tt WHERE tt.segment_id = t.segment_id
            )

            UNION ALL

            SELECT
                t.segment_id AS seg,
                t.id,
                t.ts,
                t.pfas_ugL,
                p.k,
                p.h,
                MAX(0.0, c.S_t + t.pfas_ugL - p.k) AS S_t
            FROM cusum c
            JOIN pfas_telemetry t
              ON t.segment_id = c.seg AND t.id > c.id
            JOIN pfas_cusum_params p
              ON p.segment_id = c.seg
            WHERE t.id = (
                SELECT MIN(id)
                FROM pfas_telemetry tt
                WHERE tt.segment_id = c.seg AND tt.id > c.id
            )
        )
        SELECT seg AS segment_id, id, ts, pfas_ugL, k, h, S_t
        FROM cusum;
    )SQL";

    exec_sql(db, sql_view);
}

// Triggers on insert to update corridor flags and alerts.
// On insert into pfas_telemetry:
//   - Check CUSUM for this segment at latest id and insert alert if S_t >= h.
//   - Compute Z-score from moving stats and optionally insert alert if above threshold.
//   - Update corridor_flag.segment_id flagged=1 when alert is raised.
void install_triggers(sqlite3* db, double z_threshold) {
    // Trigger: CUSUM and Z-score alert for newly inserted row.
    // Note: SQLite triggers cannot easily reference recursive CTE, so we
    //       approximate by selecting from pfas_cusum/pfas_moving_stats views.
    std::string sql_trigger = R"SQL(
        CREATE TRIGGER IF NOT EXISTS trg_pfas_telemetry_insert
        AFTER INSERT ON pfas_telemetry
        BEGIN
            -- CUSUM check
            INSERT INTO pfas_change_alert(ts, segment_id, alert_type, pfas_ugL, cusum_value, z_score)
            SELECT
                NEW.ts,
                NEW.segment_id,
                'CUSUM',
                NEW.pfas_ugL,
                pc.S_t,
                NULL
            FROM pfas_cusum pc
            WHERE pc.segment_id = NEW.segment_id
              AND pc.id = NEW.id
              AND pc.S_t >= pc.h;

            -- Z-score check (Bayesian approximation)
            INSERT INTO pfas_change_alert(ts, segment_id, alert_type, pfas_ugL, cusum_value, z_score)
            SELECT
                ms.ts,
                ms.segment_id,
                'BAYES_Z',
                ms.pfas_ugL,
                NULL,
                CASE
                    WHEN ms.sigma_win IS NOT NULL AND ms.sigma_win > 0.0
                    THEN (ms.pfas_ugL - ms.mu_win) / ms.sigma_win
                    ELSE NULL
                END AS z_score
            FROM pfas_moving_stats ms
            WHERE ms.id = NEW.id
              AND ms.segment_id = NEW.segment_id
              AND ms.sigma_win IS NOT NULL
              AND ms.sigma_win > 0.0
              AND ABS((ms.pfas_ugL - ms.mu_win) / ms.sigma_win) >= %ZTHR;

            -- Corridor flagging when any alert is raised for this segment.
            UPDATE pfas_corridor_flag
            SET flagged = 1,
                flagged_at = NEW.ts
            WHERE segment_id = NEW.segment_id
              AND EXISTS (
                  SELECT 1 FROM pfas_change_alert a
                  WHERE a.segment_id = NEW.segment_id
                    AND a.ts = NEW.ts
              );

            -- Insert corridor_flag row if missing and alert raised.
            INSERT INTO pfas_corridor_flag(segment_id, flagged, flagged_at)
            SELECT
                NEW.segment_id,
                1,
                NEW.ts
            WHERE NOT EXISTS (
                SELECT 1 FROM pfas_corridor_flag cf
                WHERE cf.segment_id = NEW.segment_id
            )
            AND EXISTS (
                SELECT 1 FROM pfas_change_alert a
                WHERE a.segment_id = NEW.segment_id
                  AND a.ts = NEW.ts
            );
        END;
    )SQL";

    // Substitute Z threshold.
    std::string z_str = std::to_string(z_threshold);
    auto pos = sql_trigger.find("%ZTHR");
    if (pos != std::string::npos) {
        sql_trigger.replace(pos, 5, z_str);
    }

    exec_sql(db, sql_trigger);
}

// Simple MQTT publisher placeholder: in production, this would integrate with
// a real MQTT client library (e.g., Eclipse Paho). Here we just print alerts.
void publish_alerts(sqlite3* db) {
    const char* sql =
        "SELECT id, ts, segment_id, alert_type, pfas_ugL, cusum_value, z_score "
        "FROM pfas_change_alert WHERE handled = 0 ORDER BY ts ASC;";
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error("Prepare alert select failed: " + std::string(sqlite3_errmsg(db)));
    }

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        const unsigned char* ts = sqlite3_column_text(stmt, 1);
        int segment_id = sqlite3_column_int(stmt, 2);
        const unsigned char* alert_type = sqlite3_column_text(stmt, 3);
        double pfas_ugL = sqlite3_column_double(stmt, 4);
        double cusum_value = sqlite3_column_double(stmt, 5);
        double z_score = sqlite3_column_double(stmt, 6);

        std::string ts_str = ts ? reinterpret_cast<const char*>(ts) : "";
        std::string type_str = alert_type ? reinterpret_cast<const char*>(alert_type) : "";

        // Placeholder "publish" operation.
        std::cout << "[MQTT] PFAS alert id=" << id
                  << " ts=" << ts_str
                  << " segment=" << segment_id
                  << " type=" << type_str
                  << " pfas=" << pfas_ugL
                  << " cusum=" << cusum_value
                  << " z=" << z_score
                  << std::endl;

        // Mark as handled.
        char* errmsg = nullptr;
        std::string sql_update = "UPDATE pfas_change_alert SET handled = 1 WHERE id = " + std::to_string(id) + ";";
        rc = sqlite3_exec(db, sql_update.c_str(), nullptr, nullptr, &errmsg);
        if (rc != SQLITE_OK) {
            std::string msg = errmsg ? errmsg : "";
            sqlite3_free(errmsg);
            sqlite3_finalize(stmt);
            throw std::runtime_error("Update handled failed: " + msg);
        }
    }

    sqlite3_finalize(stmt);
}

} // namespace tools
} // namespace prometheus_praxis

int main(int argc, char* argv[]) {
    using namespace prometheus_praxis::tools;

    std::string db_path = "pfas_change_detection.db";
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
        install_schema(db);
        install_moving_stats_view(db, 20);  // moving window of 20 samples
        install_cusum_view(db);
        install_triggers(db, 3.0);          // Z-score threshold (e.g., 3σ)

        std::cout << "PFAS change-point detection schema and logic installed into "
                  << db_path << std::endl;

        // Example loop to publish alerts (would run in a service).
        publish_alerts(db);
    } catch (const std::exception& ex) {
        std::cerr << "PFAS change-point detection error: " << ex.what() << std::endl;
        sqlite3_close(db);
        return 1;
    }

    sqlite3_close(db);
    return 0;
}
