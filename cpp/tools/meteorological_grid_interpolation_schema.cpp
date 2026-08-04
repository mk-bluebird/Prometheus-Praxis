// File: cpp/tools/meteorological_grid_interpolation_schema.cpp

#include <iostream>
#include <string>
#include <stdexcept>
#include <vector>
#include <cmath>
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

// Install SQL schema for meteorological grid, H3 centroids, LST forecast,
// and integration with hex_thermal_recovery.
void install_meteorological_grid_schema(sqlite3* db) {
    // Meteorological grid definition (static grid cells).
    const std::string sql_met_grid = R"SQL(
        CREATE TABLE IF NOT EXISTS met_grid (
            grid_id        INTEGER PRIMARY KEY,
            lat_center     REAL NOT NULL,
            lon_center     REAL NOT NULL,
            dx_km          REAL NOT NULL,  -- half-width in km (approximate)
            dy_km          REAL NOT NULL   -- half-height in km (approximate)
        );

        CREATE INDEX IF NOT EXISTS idx_met_grid_lat_lon
            ON met_grid(lat_center, lon_center);
    )SQL";

    // Meteorological forecasts per grid cell and timestamp.
    // Variables can include temperature, humidity, radiation, wind, etc.
    const std::string sql_met_forecast = R"SQL(
        CREATE TABLE IF NOT EXISTS met_forecast (
            grid_id        INTEGER NOT NULL,
            ts             TEXT NOT NULL,
            temp_2m_c      REAL NOT NULL,   -- 2m air temperature (C)
            rh_percent     REAL,           -- relative humidity (%)
            shortwave_wm2  REAL,           -- shortwave radiation (W/m^2)
            longwave_wm2   REAL,           -- longwave radiation (W/m^2)
            wind_speed_ms  REAL,           -- wind speed (m/s)
            PRIMARY KEY (grid_id, ts),
            FOREIGN KEY (grid_id) REFERENCES met_grid(grid_id) ON DELETE CASCADE
        );

        CREATE INDEX IF NOT EXISTS idx_met_forecast_ts
            ON met_forecast(ts);
    )SQL";

    // H3 centroids table: mapping from H3 index to centroid lat/lon.
    const std::string sql_h3_centroids = R"SQL(
        CREATE TABLE IF NOT EXISTS h3_centroids (
            h3_index   TEXT PRIMARY KEY,
            lat        REAL NOT NULL,
            lon        REAL NOT NULL
        );

        CREATE INDEX IF NOT EXISTS idx_h3_centroids_lat_lon
            ON h3_centroids(lat, lon);
    )SQL";

    // Table for interpolated meteorological conditions at H3 cells (per timestamp).
    // This stores nearest-neighbor or weighted interpolation results.
    const std::string sql_hex_met_interp = R"SQL(
        CREATE TABLE IF NOT EXISTS hex_met_interp (
            h3_index       TEXT NOT NULL,
            ts             TEXT NOT NULL,
            temp_2m_c      REAL NOT NULL,
            rh_percent     REAL,
            shortwave_wm2  REAL,
            longwave_wm2   REAL,
            wind_speed_ms  REAL,
            PRIMARY KEY (h3_index, ts),
            FOREIGN KEY (h3_index) REFERENCES h3_centroids(h3_index) ON DELETE CASCADE
        );

        CREATE INDEX IF NOT EXISTS idx_hex_met_interp_ts
            ON hex_met_interp(ts);
    )SQL";

    // LST forecast table based on simplified energy-balance model:
    //   LST_forecast = β0 + β1 * temp_2m_c + β2 * shortwave_wm2 + β3 * longwave_wm2
    //                + β4 * wind_speed_ms + β5 * green_fraction
    // where β-coefficients are calibrated externally.
    const std::string sql_hex_lst_forecast = R"SQL(
        CREATE TABLE IF NOT EXISTS hex_lst_forecast (
            h3_index        TEXT NOT NULL,
            ts              TEXT NOT NULL,
            lst_forecast_c  REAL NOT NULL,
            beta0           REAL NOT NULL,
            beta1           REAL NOT NULL,
            beta2           REAL NOT NULL,
            beta3           REAL NOT NULL,
            beta4           REAL NOT NULL,
            beta5           REAL NOT NULL,
            PRIMARY KEY (h3_index, ts),
            FOREIGN KEY (h3_index) REFERENCES h3_centroids(h3_index) ON DELETE CASCADE
        );

        CREATE INDEX IF NOT EXISTS idx_hex_lst_ts
            ON hex_lst_forecast(ts);
    )SQL";

    // Hex thermal recovery table assumed from earlier schema; ensure exists for join.
    const std::string sql_hex_thermal_recovery = R"SQL(
        CREATE TABLE IF NOT EXISTS hex_thermal_recovery (
            h3_index              TEXT PRIMARY KEY,
            lst_baseline_c        REAL NOT NULL,
            lst_recent_c          REAL NOT NULL,
            lst_drop_c            REAL NOT NULL,
            recovery_score        REAL,
            updated_at            TEXT NOT NULL
        );
    )SQL";

    // View joining interpolated met, LST forecast, and thermal recovery.
    const std::string sql_hex_met_lst_view = R"SQL(
        CREATE VIEW IF NOT EXISTS hex_met_lst_thermal AS
        SELECT
            hm.h3_index,
            hm.ts,
            hm.temp_2m_c,
            hm.rh_percent,
            hm.shortwave_wm2,
            hm.longwave_wm2,
            hm.wind_speed_ms,
            lf.lst_forecast_c,
            tr.lst_baseline_c,
            tr.lst_recent_c,
            tr.lst_drop_c,
            tr.recovery_score
        FROM hex_met_interp hm
        LEFT JOIN hex_lst_forecast lf
          ON hm.h3_index = lf.h3_index AND hm.ts = lf.ts
        LEFT JOIN hex_thermal_recovery tr
          ON hm.h3_index = tr.h3_index;
    )SQL";

    exec_sql(db, sql_met_grid);
    exec_sql(db, sql_met_forecast);
    exec_sql(db, sql_h3_centroids);
    exec_sql(db, sql_hex_met_interp);
    exec_sql(db, sql_hex_lst_forecast);
    exec_sql(db, sql_hex_thermal_recovery);
    exec_sql(db, sql_hex_met_lst_view);
}

// Nearest neighbor interpolation helper in C++.
// For simplicity, we use great-circle distance approximation in km.
struct MetPoint {
    int grid_id;
    double lat;
    double lon;
    double temp_2m_c;
    double rh_percent;
    double shortwave_wm2;
    double longwave_wm2;
    double wind_speed_ms;
};

struct H3Point {
    std::string h3_index;
    double lat;
    double lon;
};

double haversine_km(double lat1, double lon1, double lat2, double lon2) {
    const double R = 6371.0; // Earth radius in km
    double dlat = (lat2 - lat1) * M_PI / 180.0;
    double dlon = (lon2 - lon1) * M_PI / 180.0;
    double a = std::sin(dlat / 2.0) * std::sin(dlat / 2.0) +
               std::cos(lat1 * M_PI / 180.0) * std::cos(lat2 * M_PI / 180.0) *
               std::sin(dlon / 2.0) * std::sin(dlon / 2.0);
    double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
    return R * c;
}

class MetInterpolationInterface {
public:
    explicit MetInterpolationInterface(const std::string& db_path)
        : db_path_(db_path) {}

    // Interpolate meteorological fields at all H3 centroids for a given timestamp
    // using nearest neighbor from met_forecast; write into hex_met_interp.
    void interpolate_ts(const std::string& ts) const {
        sqlite3* db = nullptr;
        int rc = sqlite3_open(db_path_.c_str(), &db);
        if (rc != SQLITE_OK) {
            std::string msg = sqlite3_errmsg(db);
            sqlite3_close(db);
            throw std::runtime_error("Cannot open DB for interpolation: " + msg);
        }

        // Load met points at ts.
        std::vector<MetPoint> met_points;
        {
            const char* sql =
                "SELECT mg.grid_id, mg.lat_center, mg.lon_center, "
                "       mf.temp_2m_c, mf.rh_percent, mf.shortwave_wm2, "
                "       mf.longwave_wm2, mf.wind_speed_ms "
                "FROM met_grid mg "
                "JOIN met_forecast mf ON mg.grid_id = mf.grid_id "
                "WHERE mf.ts = ?;";
            sqlite3_stmt* stmt = nullptr;
            rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                std::string msg = sqlite3_errmsg(db);
                sqlite3_close(db);
                throw std::runtime_error("Prepare met query failed: " + msg);
            }
            rc = sqlite3_bind_text(stmt, 1, ts.c_str(), -1, SQLITE_TRANSIENT);
            if (rc != SQLITE_OK) {
                sqlite3_finalize(stmt);
                sqlite3_close(db);
                throw std::runtime_error("Bind ts failed: " + std::string(sqlite3_errmsg(db)));
            }

            while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
                MetPoint p{};
                p.grid_id = sqlite3_column_int(stmt, 0);
                p.lat = sqlite3_column_double(stmt, 1);
                p.lon = sqlite3_column_double(stmt, 2);
                p.temp_2m_c = sqlite3_column_double(stmt, 3);
                p.rh_percent = sqlite3_column_double(stmt, 4);
                p.shortwave_wm2 = sqlite3_column_double(stmt, 5);
                p.longwave_wm2 = sqlite3_column_double(stmt, 6);
                p.wind_speed_ms = sqlite3_column_double(stmt, 7);
                met_points.push_back(p);
            }
            sqlite3_finalize(stmt);
        }

        if (met_points.empty()) {
            sqlite3_close(db);
            throw std::runtime_error("No met_forecast rows at ts=" + ts);
        }

        // Load H3 centroids.
        std::vector<H3Point> h3_points;
        {
            const char* sql = "SELECT h3_index, lat, lon FROM h3_centroids;";
            sqlite3_stmt* stmt = nullptr;
            rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                std::string msg = sqlite3_errmsg(db);
                sqlite3_close(db);
                throw std::runtime_error("Prepare h3_centroids query failed: " + msg);
            }
            while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
                H3Point hp{};
                const unsigned char* h3 = sqlite3_column_text(stmt, 0);
                hp.h3_index = h3 ? reinterpret_cast<const char*>(h3) : "";
                hp.lat = sqlite3_column_double(stmt, 1);
                hp.lon = sqlite3_column_double(stmt, 2);
                h3_points.push_back(hp);
            }
            sqlite3_finalize(stmt);
        }

        // Insert interpolated values.
        {
            char* errmsg = nullptr;
            rc = sqlite3_exec(db, "BEGIN IMMEDIATE TRANSACTION;", nullptr, nullptr, &errmsg);
            if (rc != SQLITE_OK) {
                std::string msg = errmsg ? errmsg : "";
                sqlite3_free(errmsg);
                sqlite3_close(db);
                throw std::runtime_error("BEGIN TRANSACTION failed: " + msg);
            }

            const char* sql_ins =
                "INSERT OR REPLACE INTO hex_met_interp("
                "  h3_index, ts, temp_2m_c, rh_percent, shortwave_wm2, longwave_wm2, wind_speed_ms"
                ") VALUES (?, ?, ?, ?, ?, ?, ?);";
            sqlite3_stmt* stmt = nullptr;
            rc = sqlite3_prepare_v2(db, sql_ins, -1, &stmt, nullptr);
            if (rc != SQLITE_OK) {
                std::string msg = sqlite3_errmsg(db);
                sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
                sqlite3_close(db);
                throw std::runtime_error("Prepare insert failed: " + msg);
            }

            for (const auto& hp : h3_points) {
                // Nearest neighbor.
                const MetPoint* best = nullptr;
                double best_dist = std::numeric_limits<double>::max();
                for (const auto& mp : met_points) {
                    double d = haversine_km(hp.lat, hp.lon, mp.lat, mp.lon);
                    if (d < best_dist) {
                        best_dist = d;
                        best = &mp;
                    }
                }
                if (!best) continue;

                sqlite3_reset(stmt);
                sqlite3_clear_bindings(stmt);
                rc = sqlite3_bind_text(stmt, 1, hp.h3_index.c_str(), -1, SQLITE_TRANSIENT);
                if (rc != SQLITE_OK) goto bind_error;
                rc = sqlite3_bind_text(stmt, 2, ts.c_str(), -1, SQLITE_TRANSIENT);
                if (rc != SQLITE_OK) goto bind_error;
                rc = sqlite3_bind_double(stmt, 3, best->temp_2m_c);
                if (rc != SQLITE_OK) goto bind_error;
                rc = sqlite3_bind_double(stmt, 4, best->rh_percent);
                if (rc != SQLITE_OK) goto bind_error;
                rc = sqlite3_bind_double(stmt, 5, best->shortwave_wm2);
                if (rc != SQLITE_OK) goto bind_error;
                rc = sqlite3_bind_double(stmt, 6, best->longwave_wm2);
                if (rc != SQLITE_OK) goto bind_error;
                rc = sqlite3_bind_double(stmt, 7, best->wind_speed_ms);
                if (rc != SQLITE_OK) goto bind_error;

                rc = sqlite3_step(stmt);
                if (rc != SQLITE_DONE) {
                    std::string msg = sqlite3_errmsg(db);
                    sqlite3_finalize(stmt);
                    sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
                    sqlite3_close(db);
                    throw std::runtime_error("Insert step failed: " + msg);
                }
                continue;

            bind_error:
                {
                    std::string msg = sqlite3_errmsg(db);
                    sqlite3_finalize(stmt);
                    sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
                    sqlite3_close(db);
                    throw std::runtime_error("Bind error: " + msg);
                }
            }

            sqlite3_finalize(stmt);
            rc = sqlite3_exec(db, "COMMIT;", nullptr, nullptr, &errmsg);
            if (rc != SQLITE_OK) {
                std::string msg = errmsg ? errmsg : "";
                sqlite3_free(errmsg);
                sqlite3_close(db);
                throw std::runtime_error("COMMIT failed: " + msg);
            }
        }

        sqlite3_close(db);
    }

    // Compute LST forecasts for all hex_met_interp rows at a given timestamp,
    // using supplied β coefficients and a green_fraction (if known via another table).
    void compute_lst_forecast_ts(const std::string& ts,
                                 double beta0,
                                 double beta1,
                                 double beta2,
                                 double beta3,
                                 double beta4,
                                 double beta5) const {
        sqlite3* db = nullptr;
        int rc = sqlite3_open(db_path_.c_str(), &db);
        if (rc != SQLITE_OK) {
            std::string msg = sqlite3_errmsg(db);
            sqlite3_close(db);
            throw std::runtime_error("Cannot open DB for LST forecast: " + msg);
        }

        // Load interpolated met + optional green_fraction from hex_canopy_aggregation.
        const char* sql_sel =
            "SELECT hm.h3_index, hm.temp_2m_c, hm.shortwave_wm2, hm.longwave_wm2, hm.wind_speed_ms, "
            "       COALESCE(c.total_canopy_m2, 0.0) AS total_canopy_m2 "
            "FROM hex_met_interp hm "
            "LEFT JOIN hex_canopy_aggregation c ON hm.h3_index = c.h3_index "
            "WHERE hm.ts = ?;";

        sqlite3_stmt* stmt_sel = nullptr;
        rc = sqlite3_prepare_v2(db, sql_sel, -1, &stmt_sel, nullptr);
        if (rc != SQLITE_OK) {
            std::string msg = sqlite3_errmsg(db);
            sqlite3_close(db);
            throw std::runtime_error("Prepare LST select failed: " + msg);
        }
        rc = sqlite3_bind_text(stmt_sel, 1, ts.c_str(), -1, SQLITE_TRANSIENT);
        if (rc != SQLITE_OK) {
            sqlite3_finalize(stmt_sel);
            sqlite3_close(db);
            throw std::runtime_error("Bind ts failed: " + std::string(sqlite3_errmsg(db)));
        }

        char* errmsg = nullptr;
        rc = sqlite3_exec(db, "BEGIN IMMEDIATE TRANSACTION;", nullptr, nullptr, &errmsg);
        if (rc != SQLITE_OK) {
            std::string msg = errmsg ? errmsg : "";
            sqlite3_free(errmsg);
            sqlite3_finalize(stmt_sel);
            sqlite3_close(db);
            throw std::runtime_error("BEGIN TRANSACTION failed: " + msg);
        }

        const char* sql_ins =
            "INSERT OR REPLACE INTO hex_lst_forecast("
            "  h3_index, ts, lst_forecast_c, beta0, beta1, beta2, beta3, beta4, beta5"
            ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);";

        sqlite3_stmt* stmt_ins = nullptr;
        rc = sqlite3_prepare_v2(db, sql_ins, -1, &stmt_ins, nullptr);
        if (rc != SQLITE_OK) {
            std::string msg = sqlite3_errmsg(db);
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            sqlite3_finalize(stmt_sel);
            sqlite3_close(db);
            throw std::runtime_error("Prepare LST insert failed: " + msg);
        }

        while ((rc = sqlite3_step(stmt_sel)) == SQLITE_ROW) {
            const unsigned char* h3 = sqlite3_column_text(stmt_sel, 0);
            std::string h3_index = h3 ? reinterpret_cast<const char*>(h3) : "";
            double temp = sqlite3_column_double(stmt_sel, 1);
            double sw = sqlite3_column_double(stmt_sel, 2);
            double lw = sqlite3_column_double(stmt_sel, 3);
            double wind = sqlite3_column_double(stmt_sel, 4);
            double canopy_m2 = sqlite3_column_double(stmt_sel, 5);

            // Simple proxy for green_fraction: normalize canopy area by a scale factor.
            double green_fraction = std::min(1.0, canopy_m2 / 2000.0);

            double lst = beta0
                         + beta1 * temp
                         + beta2 * sw
                         + beta3 * lw
                         + beta4 * wind
                         + beta5 * green_fraction;

            sqlite3_reset(stmt_ins);
            sqlite3_clear_bindings(stmt_ins);
            rc = sqlite3_bind_text(stmt_ins, 1, h3_index.c_str(), -1, SQLITE_TRANSIENT);
            if (rc != SQLITE_OK) goto bind_error;
            rc = sqlite3_bind_text(stmt_ins, 2, ts.c_str(), -1, SQLITE_TRANSIENT);
            if (rc != SQLITE_OK) goto bind_error;
            rc = sqlite3_bind_double(stmt_ins, 3, lst);
            if (rc != SQLITE_OK) goto bind_error;
            rc = sqlite3_bind_double(stmt_ins, 4, beta0);
            if (rc != SQLITE_OK) goto bind_error;
            rc = sqlite3_bind_double(stmt_ins, 5, beta1);
            if (rc != SQLITE_OK) goto bind_error;
            rc = sqlite3_bind_double(stmt_ins, 6, beta2);
            if (rc != SQLITE_OK) goto bind_error;
            rc = sqlite3_bind_double(stmt_ins, 7, beta3);
            if (rc != SQLITE_OK) goto bind_error;
            rc = sqlite3_bind_double(stmt_ins, 8, beta4);
            if (rc != SQLITE_OK) goto bind_error;
            rc = sqlite3_bind_double(stmt_ins, 9, beta5);
            if (rc != SQLITE_OK) goto bind_error;

            rc = sqlite3_step(stmt_ins);
            if (rc != SQLITE_DONE) {
                std::string msg = sqlite3_errmsg(db);
                sqlite3_finalize(stmt_ins);
                sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
                sqlite3_finalize(stmt_sel);
                sqlite3_close(db);
                throw std::runtime_error("LST insert step failed: " + msg);
            }
            continue;

        bind_error:
            {
                std::string msg = sqlite3_errmsg(db);
                sqlite3_finalize(stmt_ins);
                sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
                sqlite3_finalize(stmt_sel);
                sqlite3_close(db);
                throw std::runtime_error("Bind error: " + msg);
            }
        }

        sqlite3_finalize(stmt_ins);
        sqlite3_finalize(stmt_sel);
        rc = sqlite3_exec(db, "COMMIT;", nullptr, nullptr, &errmsg);
        if (rc != SQLITE_OK) {
            std::string msg = errmsg ? errmsg : "";
            sqlite3_free(errmsg);
            sqlite3_close(db);
            throw std::runtime_error("COMMIT failed: " + msg);
        }

        sqlite3_close(db);
    }

private:
    std::string db_path_;
};

} // namespace tools
} // namespace prometheus_praxis

int main(int argc, char* argv[]) {
    using namespace prometheus_praxis::tools;

    std::string db_path = "met_hex_thermal.db";
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
        install_meteorological_grid_schema(db);
        sqlite3_close(db);

        MetInterpolationInterface interp(db_path);

        // Example timestamp; in production this would be looped over all forecast horizons.
        std::string ts = "2026-08-04T12:00:00Z";

        // Run nearest-neighbor interpolation and LST forecast.
        interp.interpolate_ts(ts);

        // Example β coefficients for simplified energy-balance model; these would be calibrated.
        double beta0 = 0.0;
        double beta1 = 0.8;    // air temperature contribution
        double beta2 = 0.0005; // shortwave radiation contribution
        double beta3 = 0.0002; // longwave radiation contribution
        double beta4 = -0.1;   // wind cooling effect
        double beta5 = -5.0;   // green fraction cooling effect

        interp.compute_lst_forecast_ts(ts, beta0, beta1, beta2, beta3, beta4, beta5);

        std::cout << "Meteorological grid interpolation + LST forecast completed for ts=" << ts << std::endl;
        std::cout << "Use view hex_met_lst_thermal to join met, LST forecast, and thermal recovery for MPC or optimization." << std::endl;
    } catch (const std::exception& ex) {
        std::cerr << "Meteorological interpolation error: " << ex.what() << std::endl;
        sqlite3_close(db);
        return 1;
    }

    return 0;
}
