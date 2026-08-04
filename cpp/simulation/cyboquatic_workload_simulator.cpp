// File: cpp/simulation/cyboquatic_workload_simulator.cpp
// Destination: mk-bluebird/Prometheus-Praxis/cpp/simulation/cyboquatic_workload_simulator.cpp

#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <limits>
#include <chrono>
#include <random>
#include <iomanip>
#include <stdexcept>
#include <sqlite3.h>

namespace cyboquatic {

struct WorkloadSample {
    std::string basin_id;
    double timestamp_s;
    double flow_rate_m3_s;
    double head_m;
    double motor_efficiency;
    double aeration_factor;
    double energyreq_j;
    double delta_vt_m_s;
    std::string fog_route;
    double ker_k;
    double ker_e;
    double ker_r;
};

struct CanalNodeTelemetry {
    std::string node_id;
    double flow_rate_m3s;
    double head_loss_m;
    double pump_power_kw;
    double lift_height_m;
    double water_density_kgm3;
    double gravity_ms2;
    double eco_efficiency;
    double delta_v_t;
    double timestamp_seconds;
};

struct WorkloadResult {
    double energy_req_j;
    double eco_weighted_energy_j;
    double delta_v_t;
};

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

class CyboquaticWorkloadSimulator {
public:
    CyboquaticWorkloadSimulator(double corridor_alpha,
                                double corridor_beta,
                                double max_allowed_delta_v)
        : alpha_(corridor_alpha),
          beta_(corridor_beta),
          max_allowed_delta_v_(max_allowed_delta_v),
          last_timestamp_(std::numeric_limits<double>::quiet_NaN()),
          cumulative_energy_j_(0.0),
          cumulative_eco_energy_j_(0.0),
          cumulative_delta_v_t_(0.0) {}

    WorkloadResult step(const CanalNodeTelemetry& telemetry) {
        validate_telemetry(telemetry);
        double dt = compute_dt(telemetry.timestamp_seconds);

        double hydraulic_energy_j = telemetry.water_density_kgm3 *
                                    telemetry.gravity_ms2 *
                                    telemetry.flow_rate_m3s *
                                    telemetry.lift_height_m *
                                    dt;

        double electrical_energy_j = telemetry.pump_power_kw * 1000.0 * dt;
        double energy_req_j = hydraulic_energy_j + electrical_energy_j;

        double eco_factor = 1.0 + alpha_ * (1.0 - clamp01(telemetry.eco_efficiency));
        double eco_weighted_energy_j = energy_req_j * eco_factor;

        double delta_v_t = beta_ * eco_weighted_energy_j;
        if (delta_v_t > max_allowed_delta_v_) {
            delta_v_t = max_allowed_delta_v_;
        }

        cumulative_energy_j_ += energy_req_j;
        cumulative_eco_energy_j_ += eco_weighted_energy_j;
        cumulative_delta_v_t_ += delta_v_t;
        last_timestamp_ = telemetry.timestamp_seconds;

        WorkloadResult result;
        result.energy_req_j = energy_req_j;
        result.eco_weighted_energy_j = eco_weighted_energy_j;
        result.delta_v_t = delta_v_t;
        return result;
    }

    double cumulative_energy() const {
        return cumulative_energy_j_;
    }

    double cumulative_eco_energy() const {
        return cumulative_eco_energy_j_;
    }

    double cumulative_delta_v_t() const {
        return cumulative_delta_v_t_;
    }

    void reset() {
        last_timestamp_ = std::numeric_limits<double>::quiet_NaN();
        cumulative_energy_j_ = 0.0;
        cumulative_eco_energy_j_ = 0.0;
        cumulative_delta_v_t_ = 0.0;
    }

private:
    double alpha_;
    double beta_;
    double max_allowed_delta_v_;
    double last_timestamp_;
    double cumulative_energy_j_;
    double cumulative_eco_energy_j_;
    double cumulative_delta_v_t_;

    static double clamp01(double v) {
        if (v < 0.0) return 0.0;
        if (v > 1.0) return 1.0;
        return v;
    }

    static void validate_telemetry(const CanalNodeTelemetry& t) {
        if (t.flow_rate_m3s < 0.0) {
            throw std::invalid_argument("flow_rate_m3s must be non-negative");
        }
        if (t.lift_height_m < 0.0) {
            throw std::invalid_argument("lift_height_m must be non-negative");
        }
        if (t.pump_power_kw < 0.0) {
            throw std::invalid_argument("pump_power_kw must be non-negative");
        }
        if (t.water_density_kgm3 <= 0.0) {
            throw std::invalid_argument("water_density_kgm3 must be positive");
        }
        if (t.gravity_ms2 <= 0.0) {
            throw std::invalid_argument("gravity_ms2 must be positive");
        }
        if (t.eco_efficiency < 0.0 || t.eco_efficiency > 1.0) {
            throw std::invalid_argument("eco_efficiency must be in [0,1]");
        }
    }

    double compute_dt(double current_timestamp) const {
        if (std::isnan(last_timestamp_)) {
            return 1.0;
        }
        double dt = current_timestamp - last_timestamp_;
        if (dt <= 0.0) {
            return 1.0;
        }
        return dt;
    }
};

WorkloadSample make_sample(const std::string& basin_id,
                           double t_s,
                           double base_flow,
                           double base_head,
                           std::mt19937_64& rng) {
    std::normal_distribution<double> flow_noise(0.0, 0.02 * base_flow);
    std::normal_distribution<double> head_noise(0.0, 0.05 * base_head);

    WorkloadSample s{};
    s.basin_id = basin_id;
    s.timestamp_s = t_s;
    s.flow_rate_m3_s = std::max(base_flow + flow_noise(rng), 0.0);
    s.head_m = std::max(base_head + head_noise(rng), 0.0);
    s.motor_efficiency = 0.80;
    s.aeration_factor = 0.6;

    double rho = 1000.0;
    double g = 9.80665;
    double power_W = rho * g * s.flow_rate_m3_s * s.head_m / s.motor_efficiency;
    s.energyreq_j = power_W;

    s.delta_vt_m_s = 0.05 * s.aeration_factor * std::sqrt(std::max(s.head_m, 0.0));

    if (s.flow_rate_m3_s < 0.1 && s.head_m < 3.0) {
        s.fog_route = "PRIMARY_CANAL";
    } else if (s.flow_rate_m3_s < 0.2) {
        s.fog_route = "SECONDARY_CANAL";
    } else {
        s.fog_route = "HOLD_TANK";
    }

    s.ker_k = 0.9;
    const double alpha = 1e-6;
    s.ker_e = -alpha * s.energyreq_j;
    s.ker_r = (s.fog_route == "HOLD_TANK") ? 0.5 : 0.2;

    return s;
}

} // namespace cyboquatic

int main() {
    using namespace cyboquatic;

    sqlite3* db = nullptr;
    int rc = sqlite3_open("cpp/simulation/data/cyboquatic_workload_dashboard.db", &db);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to open SQLite database\n";
        return 1;
    }

    try {
        exec_sql(db, "PRAGMA journal_mode=WAL;");
        exec_sql(db, "PRAGMA synchronous=NORMAL;");
        exec_sql(db, "PRAGMA foreign_keys=ON;");

        exec_sql(db,
            "CREATE TABLE IF NOT EXISTS cyboquatic_workload_telemetry ("
            "  telemetry_id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  basin_id TEXT NOT NULL,"
            "  timestamp_s REAL NOT NULL,"
            "  flow_rate_m3_s REAL NOT NULL,"
            "  head_m REAL NOT NULL,"
            "  motor_efficiency REAL NOT NULL,"
            "  aeration_factor REAL NOT NULL,"
            "  energyreq_j REAL NOT NULL,"
            "  delta_vt_m_s REAL NOT NULL,"
            "  fog_route TEXT NOT NULL,"
            "  ker_k REAL NOT NULL,"
            "  ker_e REAL NOT NULL,"
            "  ker_r REAL NOT NULL"
            ");"
        );
        exec_sql(db,
            "CREATE INDEX IF NOT EXISTS idx_dashboard_basin_time "
            "ON cyboquatic_workload_telemetry(basin_id, timestamp_s);"
        );
    } catch (const std::exception& e) {
        std::cerr << "Schema error: " << e.what() << "\n";
        sqlite3_close(db);
        return 1;
    }

    try {
        sqlite3_stmt* stmt = nullptr;
        const char* insertSql =
            "INSERT INTO cyboquatic_workload_telemetry ("
            "  basin_id, timestamp_s, flow_rate_m3_s, head_m, motor_efficiency,"
            "  aeration_factor, energyreq_j, delta_vt_m_s, fog_route,"
            "  ker_k, ker_e, ker_r"
            ") VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
        rc = sqlite3_prepare_v2(db, insertSql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare insert statement");
        }

        std::mt19937_64 rng(
            static_cast<unsigned long long>(
                std::chrono::high_resolution_clock::now().time_since_epoch().count()));

        const std::string basinA = "basin-A";
        double t = 0.0;
        const double dt = 1.0;

        CyboquaticWorkloadSimulator simulator(
            0.5,
            1e-6,
            0.05
        );

        auto now = std::chrono::steady_clock::now().time_since_epoch();
        double base_ts = std::chrono::duration<double>(now).count();

        for (int i = 0; i < 300; ++i) {
            WorkloadSample s = make_sample(basinA, t, 0.15, 4.0, rng);

            sqlite3_reset(stmt);
            sqlite3_bind_text(stmt, 1, s.basin_id.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_double(stmt, 2, s.timestamp_s);
            sqlite3_bind_double(stmt, 3, s.flow_rate_m3_s);
            sqlite3_bind_double(stmt, 4, s.head_m);
            sqlite3_bind_double(stmt, 5, s.motor_efficiency);
            sqlite3_bind_double(stmt, 6, s.aeration_factor);
            sqlite3_bind_double(stmt, 7, s.energyreq_j);
            sqlite3_bind_double(stmt, 8, s.delta_vt_m_s);
            sqlite3_bind_text(stmt, 9, s.fog_route.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_double(stmt, 10, s.ker_k);
            sqlite3_bind_double(stmt, 11, s.ker_e);
            sqlite3_bind_double(stmt, 12, s.ker_r);

            rc = sqlite3_step(stmt);
            if (rc != SQLITE_DONE) {
                throw std::runtime_error("Insert failed");
            }

            CanalNodeTelemetry tele{
                "node-A",
                0.4 + 0.002 * i,
                0.2,
                1.2,
                2.0,
                1000.0,
                9.81,
                0.9,
                0.0,
                base_ts + t
            };
            WorkloadResult r = simulator.step(tele);
            (void)r;

            t += dt;
        }

        sqlite3_finalize(stmt);

        std::cout << std::fixed << std::setprecision(3);
        std::cout << "Cumulative energy_req_j=" << simulator.cumulative_energy()
                  << " cumulative_eco_energy_j=" << simulator.cumulative_eco_energy()
                  << " cumulative_delta_v_t=" << simulator.cumulative_delta_v_t()
                  << "\n";
    } catch (const std::exception& e) {
        std::cerr << "Simulation error: " << e.what() << "\n";
        sqlite3_close(db);
        return 1;
    }

    sqlite3_close(db);
    return 0;
}
