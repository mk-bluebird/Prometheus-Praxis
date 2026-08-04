// File: cpp/tools/vfd_efficiency_calibration_loader.cpp

#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <filesystem>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <stdexcept>

#include <sqlite3.h>

namespace prometheus_praxis {
namespace tools {

struct VFDEfficiencyParams {
    // Parametric efficiency curve parameters loaded from SQLite:
    // Example model: eta(f, P) = a0 + a1*f + a2*P + a3*f*f + a4*P*P + a5*f*P
    double a0;
    double a1;
    double a2;
    double a3;
    double a4;
    double a5;
    double conf_int_a0;
    double conf_int_a1;
    double conf_int_a2;
    double conf_int_a3;
    double conf_int_a4;
    double conf_int_a5;
    std::string last_calibrated_at;
};

// Thread-safe cache of VFD efficiency parameters.
class VFDEfficiencyCache {
public:
    VFDEfficiencyCache() : has_params_(false) {}

    void update(const VFDEfficiencyParams& p) {
        std::lock_guard<std::mutex> lock(mutex_);
        params_ = p;
        has_params_ = true;
    }

    bool get(VFDEfficiencyParams& out) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!has_params_) return false;
        out = params_;
        return true;
    }

    // Evaluate efficiency curve given frequency (Hz) and power (kW).
    bool evaluate(double frequency_hz, double power_kw, double& efficiency_out) const {
        VFDEfficiencyParams p;
        if (!get(p)) return false;
        double f = frequency_hz;
        double P = power_kw;
        efficiency_out = p.a0
                         + p.a1 * f
                         + p.a2 * P
                         + p.a3 * f * f
                         + p.a4 * P * P
                         + p.a5 * f * P;
        return true;
    }

private:
    mutable std::mutex mutex_;
    VFDEfficiencyParams params_;
    bool has_params_;
};

// Periodic SQLite loader that reads calibration produced by Lua-scripted LM optimization.
// It watches a calibration table and optionally a "calibration_version" file to detect changes.
class VFDEfficiencyCalibrationLoader {
public:
    VFDEfficiencyCalibrationLoader(const std::string& db_path,
                                   VFDEfficiencyCache& cache,
                                   std::chrono::seconds interval)
        : db_path_(db_path),
          cache_(cache),
          interval_(interval),
          running_(false),
          last_version_token_("") {}

    ~VFDEfficiencyCalibrationLoader() {
        stop();
    }

    void start() {
        running_ = true;
        thread_ = std::thread(&VFDEfficiencyCalibrationLoader::loop, this);
    }

    void stop() {
        running_ = false;
        if (thread_.joinable()) {
            thread_.join();
        }
    }

private:
    std::string db_path_;
    VFDEfficiencyCache& cache_;
    std::chrono::seconds interval_;
    std::atomic<bool> running_;
    std::thread thread_;
    std::string last_version_token_;

    void loop() {
        while (running_) {
            try {
                std::string current_token = readVersionToken();
                if (current_token != last_version_token_) {
                    VFDEfficiencyParams params;
                    if (loadFromDatabase(params)) {
                        cache_.update(params);
                        last_version_token_ = current_token;
                        std::cerr << "[vfd_calibration] Parameters reloaded at token="
                                  << current_token << " last_calibrated_at="
                                  << params.last_calibrated_at << std::endl;
                    }
                }
            } catch (const std::exception& ex) {
                std::cerr << "[vfd_calibration] loader error: " << ex.what() << std::endl;
            }

            std::this_thread::sleep_for(interval_);
        }
    }

    // Optionally watch a file, e.g., "vfd_calibration.version" that Lua script updates after LM run.
    std::string readVersionToken() const {
        const std::string version_file = "vfd_calibration.version";
        if (!std::filesystem::exists(version_file)) {
            // Fallback to empty token; loader will still query at first run.
            return "";
        }
        std::ifstream in(version_file);
        if (!in) return "";
        std::string token;
        std::getline(in, token);
        return token;
    }

    bool loadFromDatabase(VFDEfficiencyParams& out) {
        sqlite3* db = nullptr;
        int rc = sqlite3_open(db_path_.c_str(), &db);
        if (rc != SQLITE_OK) {
            std::cerr << "[vfd_calibration] cannot open DB: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_close(db);
            return false;
        }

        const char* sql =
            "SELECT a0, a1, a2, a3, a4, a5, "
            "       conf_int_a0, conf_int_a1, conf_int_a2, conf_int_a3, conf_int_a4, conf_int_a5, "
            "       last_calibrated_at "
            "FROM vfd_calibration "
            "ORDER BY last_calibrated_at DESC "
            "LIMIT 1;";

        sqlite3_stmt* stmt = nullptr;
        rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "[vfd_calibration] prepare failed: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_close(db);
            return false;
        }

        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            out.a0 = sqlite3_column_double(stmt, 0);
            out.a1 = sqlite3_column_double(stmt, 1);
            out.a2 = sqlite3_column_double(stmt, 2);
            out.a3 = sqlite3_column_double(stmt, 3);
            out.a4 = sqlite3_column_double(stmt, 4);
            out.a5 = sqlite3_column_double(stmt, 5);

            out.conf_int_a0 = sqlite3_column_double(stmt, 6);
            out.conf_int_a1 = sqlite3_column_double(stmt, 7);
            out.conf_int_a2 = sqlite3_column_double(stmt, 8);
            out.conf_int_a3 = sqlite3_column_double(stmt, 9);
            out.conf_int_a4 = sqlite3_column_double(stmt, 10);
            out.conf_int_a5 = sqlite3_column_double(stmt, 11);

            const unsigned char* text = sqlite3_column_text(stmt, 12);
            out.last_calibrated_at = text ? reinterpret_cast<const char*>(text) : "";
        } else if (rc == SQLITE_DONE) {
            std::cerr << "[vfd_calibration] no calibration rows found" << std::endl;
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            return false;
        } else {
            std::cerr << "[vfd_calibration] step failed: " << sqlite3_errmsg(db) << std::endl;
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            return false;
        }

        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return true;
    }
};

} // namespace tools
} // namespace prometheus_praxis

int main(int argc, char* argv[]) {
    using namespace prometheus_praxis::tools;

    std::string db_path = "telemetry.db";
    if (argc > 1) {
        db_path = argv[1];
    }

    VFDEfficiencyCache cache;
    VFDEfficiencyCalibrationLoader loader(db_path, cache, std::chrono::seconds(5));
    loader.start();

    std::cout << "VFD efficiency calibration loader running. Press Ctrl+C to exit." << std::endl;

    // Example workload model: periodically query efficiency for synthetic operating points
    // without blocking telemetry ingestion; in a real system this would be integrated into
    // the pump/drive workload estimator.
    while (true) {
        VFDEfficiencyParams p{};
        if (cache.get(p)) {
            double freq = 45.0; // Hz
            double power = 8.0; // kW
            double eta = 0.0;
            if (cache.evaluate(freq, power, eta)) {
                std::cout << "[vfd_efficiency] freq=" << freq
                          << " power=" << power
                          << " efficiency=" << eta
                          << " last_calibrated_at=" << p.last_calibrated_at
                          << std::endl;
            }
        } else {
            std::cout << "[vfd_efficiency] no calibration yet" << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }

    loader.stop();
    return 0;
}
