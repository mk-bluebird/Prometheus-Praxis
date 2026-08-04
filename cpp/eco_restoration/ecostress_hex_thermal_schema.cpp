// File: cpp/eco_restoration/ecostress_hex_thermal_schema.cpp

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

// Install schema for ECOSTRESS LST integration into hex thermal recovery.
void install_ecostress_hex_thermal_schema(sqlite3* db) {
    const std::string sql_ecostress_raw = R"SQL(
        CREATE TABLE IF NOT EXISTS ecostress_lst_raw (
            granule_id        TEXT NOT NULL,
            pixel_id          INTEGER NOT NULL,
            ts_utc            TEXT NOT NULL,
            lat               REAL NOT NULL,
            lon               REAL NOT NULL,
            lst_k             REAL NOT NULL,
            cloud_flag        INTEGER NOT NULL,
            quality_flag      INTEGER NOT NULL,
            PRIMARY KEY(granule_id, pixel_id)
        );
    )SQL";

    const std::string sql_ecostress_hex = R"SQL(
        CREATE TABLE IF NOT EXISTS ecostress_hex_lst (
            h3_index          TEXT NOT NULL,
            ts_utc            TEXT NOT NULL,
            lst_c             REAL NOT NULL,
            cloud_fraction    REAL NOT NULL,
            quality_score     REAL NOT NULL,
            PRIMARY KEY(h3_index, ts_utc)
        );

        CREATE INDEX IF NOT EXISTS idx_ecostress_hex_lst_h3_ts
            ON ecostress_hex_lst(h3_index, ts_utc);
    )SQL";

    const std::string sql_hex_thermal = R"SQL(
        CREATE TABLE IF NOT EXISTS hex_thermal_recovery (
            h3_index          TEXT PRIMARY KEY,
            lst_baseline_c    REAL NOT NULL,
            lst_recent_c      REAL NOT NULL,
            lst_drop_c        REAL NOT NULL,
            recovery_score    REAL,
            lst_quality_score REAL,
            lst_last_update   TEXT NOT NULL
        );
    )SQL";

    const std::string sql_check_view = R"SQL(
        CREATE VIEW IF NOT EXISTS hex_thermal_recovery_checked AS
        SELECT *
        FROM hex_thermal_recovery
        WHERE lst_quality_score >= 0.5;
    )SQL";

    const std::string sql_trigger_update_hex = R"SQL(
        CREATE TRIGGER IF NOT EXISTS trg_ecostress_hex_lst_upsert_hex_thermal
        AFTER INSERT ON ecostress_hex_lst
        BEGIN
            UPDATE hex_thermal_recovery
            SET lst_recent_c      = NEW.lst_c,
                lst_drop_c        = lst_baseline_c - NEW.lst_c,
                recovery_score    = CASE
                    WHEN lst_baseline_c > 0.0 THEN (lst_baseline_c - NEW.lst_c) / lst_baseline_c
                    ELSE NULL
                END,
                lst_quality_score = NEW.quality_score,
                lst_last_update   = NEW.ts_utc
            WHERE h3_index = NEW.h3_index;

            INSERT INTO hex_thermal_recovery(
                h3_index, lst_baseline_c, lst_recent_c, lst_drop_c,
                recovery_score, lst_quality_score, lst_last_update
            )
            SELECT
                NEW.h3_index,
                NEW.lst_c,
                NEW.lst_c,
                0.0,
                0.0,
                NEW.quality_score,
                NEW.ts_utc
            WHERE NOT EXISTS (
                SELECT 1 FROM hex_thermal_recovery h
                WHERE h.h3_index = NEW.h3_index
            );

            INSERT INTO hex_corridor_replan_queue(h3_index, ts_utc, lst_recent_c, lst_drop_c)
            SELECT
                NEW.h3_index,
                NEW.ts_utc,
                NEW.lst_c,
                (SELECT lst_baseline_c - NEW.lst_c
                 FROM hex_thermal_recovery h
                 WHERE h.h3_index = NEW.h3_index)
            WHERE ABS(
                (SELECT lst_baseline_c - NEW.lst_c
                 FROM hex_thermal_recovery h
                 WHERE h.h3_index = NEW.h3_index)
            ) >= 1.0;
        END;
    )SQL";

    const std::string sql_corridor_replan_queue = R"SQL(
        CREATE TABLE IF NOT EXISTS hex_corridor_replan_queue (
            id              INTEGER PRIMARY KEY AUTOINCREMENT,
            h3_index        TEXT NOT NULL,
            ts_utc          TEXT NOT NULL,
            lst_recent_c    REAL NOT NULL,
            lst_drop_c      REAL NOT NULL,
            processed       INTEGER NOT NULL DEFAULT 0
        );
    )SQL";

    exec_sql(db, sql_ecostress_raw);
    exec_sql(db, sql_ecostress_hex);
    exec_sql(db, sql_hex_thermal);
    exec_sql(db, sql_check_view);
    exec_sql(db, sql_corridor_replan_queue);
    exec_sql(db, sql_trigger_update_hex);
}

} // namespace eco_restoration
} // namespace prometheus_praxis

int main(int argc, char* argv[]) {
    using namespace prometheus_praxis::eco_restoration;

    std::string db_path = "ecostress_hex_thermal.db";
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
        install_ecostress_hex_thermal_schema(db);
        std::cout << "ECOSTRESS LST integration schema installed into "
                  << db_path << std::endl;

        std::cout << "\n-- Example query: ECOSTRESS hex LST --\n";
        std::cout << "SELECT * FROM ecostress_hex_lst ORDER BY ts_utc LIMIT 10;\n";

        std::cout << "\n-- Example query: hex thermal recovery checked --\n";
        std::cout << "SELECT * FROM hex_thermal_recovery_checked LIMIT 10;\n";

        std::cout << "\n-- Example query: corridor replan queue --\n";
        std::cout << "SELECT * FROM hex_corridor_replan_queue WHERE processed = 0 LIMIT 10;\n";
    } catch (const std::exception& ex) {
        std::cerr << "ECOSTRESS schema error: " << ex.what() << std::endl;
        sqlite3_close(db);
        return 1;
    }

    sqlite3_close(db);
    return 0;
}
