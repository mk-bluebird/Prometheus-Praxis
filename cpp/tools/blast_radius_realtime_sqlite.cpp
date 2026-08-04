// File: cpp/tools/blast_radius_realtime_sqlite.cpp
#include <iostream>
#include <string>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <sqlite3.h>

/**
 * C++ RTU (remote terminal unit) pushing water-level and pressure readings
 * into an in-memory SQLite database using WAL mode. This is designed for
 * real-time blast-radius prediction by a Kotlin micro-service that queries
 * pre-computed tables within ~100 ms.
 *
 * Schema assumptions:
 *  - realtime_sensor_readings: latest water-level and pressure readings.
 *  - blast_radius_table: pre-computed mapping from ranges of level/pressure
 *    to predicted flood polygon id.
 */

static int exec_sql(sqlite3* db, const std::string& sql) {
    char* errMsg = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::string msg = errMsg ? errMsg : "Unknown error";
        sqlite3_free(errMsg);
        throw std::runtime_error("SQLite error: " + msg);
    }
    return rc;
}

int main() {
    sqlite3* db = nullptr;
    // Use in-memory database, shared via URI for external connections if needed.
    int rc = sqlite3_open_v2("file:blast_radius_db?mode=memory&cache=shared",
                             &db,
                             SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_URI,
                             nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to open in-memory SQLite database\n";
        return 1;
    }

    // Enable WAL mode for concurrent writes/reads.
    try {
        exec_sql(db, "PRAGMA journal_mode=WAL;");
        exec_sql(db, "PRAGMA synchronous=NORMAL;");
        exec_sql(db, "PRAGMA foreign_keys=ON;");
    } catch (const std::exception& e) {
        std::cerr << "PRAGMA error: " << e.what() << "\n";
        sqlite3_close(db);
        return 1;
    }

    // Create realtime sensor table.
    try {
        exec_sql(db,
            "CREATE TABLE IF NOT EXISTS realtime_sensor_readings ("
            "  reading_id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  timestamp_ms INTEGER NOT NULL,"
            "  canal_id TEXT NOT NULL,"
            "  water_level_m REAL NOT NULL,"
            "  pressure_kPa REAL NOT NULL"
            ");"
        );
        exec_sql(db,
            "CREATE TABLE IF NOT EXISTS blast_radius_table ("
            "  radius_id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  canal_id TEXT NOT NULL,"
            "  level_min_m REAL NOT NULL,"
            "  level_max_m REAL NOT NULL,"
            "  pressure_min_kPa REAL NOT NULL,"
            "  pressure_max_kPa REAL NOT NULL,"
            "  flood_polygon_id TEXT NOT NULL"
            ");"
        );
        exec_sql(db,
            "CREATE INDEX IF NOT EXISTS idx_blast_radius_range "
            "ON blast_radius_table(canal_id, level_min_m, level_max_m, pressure_min_kPa, pressure_max_kPa);"
        );
    } catch (const std::exception& e) {
        std::cerr << "Schema error: " << e.what() << "\n";
        sqlite3_close(db);
        return 1;
    }

    // Simulated RTU loop: push sensor readings periodically.
    try {
        sqlite3_stmt* stmt = nullptr;
        const char* sql_insert =
            "INSERT INTO realtime_sensor_readings (timestamp_ms, canal_id, water_level_m, pressure_kPa) "
            "VALUES (?, ?, ?, ?);";
        rc = sqlite3_prepare_v2(db, sql_insert, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare insert statement");
        }

        const std::string canalId = "canal-01";

        for (int i = 0; i < 1000; ++i) {
            auto now = std::chrono::system_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

            double waterLevel = 2.0 + 0.01 * i; // example ramp
            double pressure = 150.0 + 0.5 * i;  // example ramp

            sqlite3_reset(stmt);
            sqlite3_bind_int64(stmt, 1, ms);
            sqlite3_bind_text(stmt, 2, canalId.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_double(stmt, 3, waterLevel);
            sqlite3_bind_double(stmt, 4, pressure);

            rc = sqlite3_step(stmt);
            if (rc != SQLITE_DONE) {
                throw std::runtime_error("Failed to execute insert statement");
            }

            // Short sleep to simulate RTU sampling interval.
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        sqlite3_finalize(stmt);
    } catch (const std::exception& e) {
        std::cerr << "RTU loop error: " << e.what() << "\n";
        sqlite3_close(db);
        return 1;
    }

    sqlite3_close(db);
    return 0;
}
