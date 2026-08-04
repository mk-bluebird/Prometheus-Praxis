// File: cpp/eco_restoration/pfas_lyapunov_feedback_controller.cpp

#include <iostream>
#include <string>
#include <stdexcept>
#include <cmath>
#include <chrono>
#include <thread>
#include <atomic>

#include <sqlite3.h>

namespace prometheus_praxis {
namespace eco_restoration {

// Simple Lyapunov-based feedback controller for PFAS stabilization.
//
// State x: PFAS concentration at control location (e.g., µg/L or normalized units).
// Lyapunov residual λ_res: deviation of Lyapunov function V(x) from desired decay profile,
//                          computed upstream and stored in SQL.
//
// Control u: aeration intensity or flow control signal, constrained in [u_min, u_max].
//
// Control law (conceptual):
//   u = sat( u_ref + k_x * x + k_λ * λ_res )
// with saturation and boundedness enforced by clamping.
//
// The goal is to drive x toward a safe reference x_ref and ensure Lyapunov residual stays small,
// supporting mean-square boundedness under stochastic sorption noise.

struct PFASControlParams {
    double x_ref;   // desired PFAS reference (safe threshold)
    double k_x;     // gain on PFAS concentration
    double k_lambda;// gain on Lyapunov residual
    double u_min;   // minimum actuator command
    double u_max;   // maximum actuator command
};

struct LyapunovResidualSample {
    double x;        // latest PFAS concentration
    double lambda_res; // latest Lyapunov residual
    std::string ts;  // timestamp string
};

// Aeration actuator interface (placeholder for real hardware integration).
class AerationActuator {
public:
    explicit AerationActuator(const std::string& name)
        : name_(name) {}

    void apply(double u) {
        // In real deployment, this would send u to a PLC or field device.
        std::cout << "[actuator:" << name_ << "] command=" << u << std::endl;
    }

private:
    std::string name_;
};

// SQL adapter to fetch latest Lyapunov residual and PFAS concentration.
class LyapunovSqlAdapter {
public:
    explicit LyapunovSqlAdapter(const std::string& db_path)
        : db_path_(db_path) {}

    LyapunovResidualSample fetchLatest() const {
        sqlite3* db = nullptr;
        int rc = sqlite3_open(db_path_.c_str(), &db);
        if (rc != SQLITE_OK) {
            std::string msg = sqlite3_errmsg(db);
            sqlite3_close(db);
            throw std::runtime_error("Cannot open SQLite DB: " + msg);
        }

        // View definition (to be created separately, see helper below):
        // CREATE VIEW latest_pfashorizon_lyap AS
        //   SELECT x, lyap_residual, ts
        //   FROM ker_pfashorizon_lyap
        //   ORDER BY ts DESC
        //   LIMIT 1;
        const char* sql =
            "SELECT x, lyap_residual, ts "
            "FROM latest_pfashorizon_lyap "
            "LIMIT 1;";

        sqlite3_stmt* stmt = nullptr;
        rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            std::string msg = sqlite3_errmsg(db);
            sqlite3_close(db);
            throw std::runtime_error("Prepare latest Lyapunov query failed: " + msg);
        }

        LyapunovResidualSample sample{};
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            sample.x = sqlite3_column_double(stmt, 0);
            sample.lambda_res = sqlite3_column_double(stmt, 1);
            const unsigned char* ts = sqlite3_column_text(stmt, 2);
            sample.ts = ts ? reinterpret_cast<const char*>(ts) : "";
        } else {
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            throw std::runtime_error("No Lyapunov residual rows available");
        }

        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return sample;
    }

    // Helper to install the view if not present.
    void installView() const {
        sqlite3* db = nullptr;
        int rc = sqlite3_open(db_path_.c_str(), &db);
        if (rc != SQLITE_OK) {
            std::string msg = sqlite3_errmsg(db);
            sqlite3_close(db);
            throw std::runtime_error("Cannot open SQLite DB for view install: " + msg);
        }

        const char* sql =
            "CREATE VIEW IF NOT EXISTS latest_pfashorizon_lyap AS "
            "SELECT x, lyap_residual, ts "
            "FROM ker_pfashorizon_lyap "
            "ORDER BY ts DESC "
            "LIMIT 1;";

        char* errmsg = nullptr;
        rc = sqlite3_exec(db, sql, nullptr, nullptr, &errmsg);
        if (rc != SQLITE_OK) {
            std::string msg = errmsg ? errmsg : "";
            sqlite3_free(errmsg);
            sqlite3_close(db);
            throw std::runtime_error("Failed to create latest_pfashorizon_lyap view: " + msg);
        }

        sqlite3_close(db);
    }

private:
    std::string db_path_;
};

// Lyapunov-based feedback controller.
class PFASLyapunovController {
public:
    PFASLyapunovController(const PFASControlParams& params,
                           LyapunovSqlAdapter& sql_adapter,
                           AerationActuator& actuator)
        : params_(params),
          sql_adapter_(sql_adapter),
          actuator_(actuator),
          running_(false) {}

    // Compute control law u = sat(u_ref + k_x * (x - x_ref) + k_lambda * lambda_res).
    double computeControl(const LyapunovResidualSample& sample) const {
        double x_err = sample.x - params_.x_ref;
        double u_ref = baselineControl(sample.x);
        double u_raw = u_ref + params_.k_x * x_err + params_.k_lambda * sample.lambda_res;
        return clamp(u_raw);
    }

    // Baseline control law that ensures safety even if Lyapunov residual is noisy.
    double baselineControl(double x) const {
        // Simple proportional term driving x toward x_ref.
        double k_p = 0.2; // baseline gain
        double u = k_p * (params_.x_ref - x);
        // Center around mid-range actuator level to avoid aggressive swings.
        double u_center = 0.5 * (params_.u_min + params_.u_max);
        u += u_center;
        return clamp(u);
    }

    // Start feedback loop with periodic polling from SQL.
    void start(std::chrono::seconds period) {
        running_ = true;
        loop_thread_ = std::thread(&PFASLyapunovController::loop, this, period);
    }

    void stop() {
        running_ = false;
        if (loop_thread_.joinable()) {
            loop_thread_.join();
        }
    }

private:
    PFASControlParams params_;
    LyapunovSqlAdapter& sql_adapter_;
    AerationActuator& actuator_;
    std::atomic<bool> running_;
    std::thread loop_thread_;

    double clamp(double u) const {
        if (u < params_.u_min) return params_.u_min;
        if (u > params_.u_max) return params_.u_max;
        return u;
    }

    void loop(std::chrono::seconds period) {
        while (running_) {
            try {
                // Fetch latest Lyapunov residual and PFAS concentration from SQL.
                LyapunovResidualSample sample = sql_adapter_.fetchLatest();

                double u = computeControl(sample);

                // Safety margin check: if x is already below reference with robust margin,
                // avoid increasing actuator beyond baseline to prevent overcorrection.
                double safety_margin = 0.1 * std::fabs(params_.x_ref);
                if (sample.x < params_.x_ref - safety_margin && u > baselineControl(sample.x)) {
                    // Governance: do not override baseline when safety margins hold.
                    u = baselineControl(sample.x);
                }

                actuator_.apply(u);

                std::cout << "[pfas_controller] ts=" << sample.ts
                          << " x=" << sample.x
                          << " lambda_res=" << sample.lambda_res
                          << " u=" << u << std::endl;
            } catch (const std::exception& ex) {
                std::cerr << "[pfas_controller] error: " << ex.what() << std::endl;
            }

            std::this_thread::sleep_for(period);
        }
    }
};

} // namespace eco_restoration
} // namespace prometheus_praxis

int main(int argc, char* argv[]) {
    using namespace prometheus_praxis::eco_restoration;

    std::string db_path = "telemetry.db";
    if (argc > 1) {
        db_path = argv[1];
    }

    PFASControlParams params;
    params.x_ref = 0.2;   // example safe PFAS concentration (normalized)
    params.k_x = -0.5;    // gain to reduce concentration when above reference
    params.k_lambda = -0.3; // gain to counteract positive Lyapunov residuals
    params.u_min = 0.0;   // minimum aeration
    params.u_max = 10.0;  // maximum aeration

    try {
        LyapunovSqlAdapter sql_adapter(db_path);
        sql_adapter.installView();

        AerationActuator actuator("pfas_aeration");

        PFASLyapunovController controller(params, sql_adapter, actuator);
        controller.start(std::chrono::seconds(5));

        std::cout << "PFAS Lyapunov-based feedback controller running. Press Ctrl+C to exit." << std::endl;

        // Run indefinitely; in production, integrate with proper signal handling.
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(30));
        }

        controller.stop();
    } catch (const std::exception& ex) {
        std::cerr << "PFAS Lyapunov controller error: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
