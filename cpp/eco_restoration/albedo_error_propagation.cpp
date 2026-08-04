// File: cpp/eco_restoration/albedo_error_propagation.cpp

#include <iostream>
#include <string>
#include <cmath>
#include <stdexcept>
#include <sqlite3.h>

namespace prometheus_praxis {
namespace eco_restoration {

struct AlbedoSensitivity {
    double alpha;
    double beta;
    double gamma;
};

struct AlbedoErrorParams {
    double systematic_error;
    double drift_per_year;
};

struct KerEBiasResult {
    double bias_now;
    double bias_after_years;
    int years_to_recalibration;
};

struct AlbedoChainParams {
    double alpha_lst;
    double beta_energy;
    double gamma_ker_e;
};

class AlbedoErrorPropagator {
public:
    explicit AlbedoErrorPropagator(const std::string& db_path)
        : db_path_(db_path), db_(nullptr) {
        open_db();
        install_schema();
    }

    ~AlbedoErrorPropagator() {
        if (db_) {
            sqlite3_close(db_);
        }
    }

    KerEBiasResult compute_bias_and_recalibration(int max_years,
                                                  double relative_error_limit) {
        AlbedoSensitivity sens = load_sensitivity();
        AlbedoErrorParams err  = load_error_params();
        double base_ker_e      = load_base_ker_e();

        double bias_now = propagate_bias(sens, err, 0.0);
        double bias_limit = relative_error_limit * base_ker_e;
        int years_to_recalibration = max_years;
        double bias_after_years = 0.0;

        for (int y = 1; y <= max_years; ++y) {
            double t = static_cast<double>(y);
            double bias_t = propagate_bias(sens, err, t);
            if (std::fabs(bias_t) > bias_limit) {
                years_to_recalibration = y;
                bias_after_years = bias_t;
                break;
            }
            bias_after_years = bias_t;
        }

        KerEBiasResult result;
        result.bias_now = bias_now;
        result.bias_after_years = bias_after_years;
        result.years_to_recalibration = years_to_recalibration;
        store_bias_result(result);
        return result;
    }

private:
    std::string db_path_;
    sqlite3* db_;

    void open_db() {
        int rc = sqlite3_open(db_path_.c_str(), &db_);
        if (rc != SQLITE_OK) {
            std::string msg = sqlite3_errmsg(db_);
            sqlite3_close(db_);
            db_ = nullptr;
            throw std::runtime_error("Cannot open DB: " + msg);
        }
    }

    void exec_sql(const std::string& sql) {
        char* errmsg = nullptr;
        int rc = sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &errmsg);
        if (rc != SQLITE_OK) {
            std::string msg = errmsg ? errmsg : "";
            sqlite3_free(errmsg);
            throw std::runtime_error("SQLite error: " + msg);
        }
    }

    void install_schema() {
        const char* sql_coeffs =
            "CREATE TABLE IF NOT EXISTS albedo_sensitivity_coeffs ("
            "  id INTEGER PRIMARY KEY,"
            "  alpha REAL NOT NULL,"
            "  beta REAL NOT NULL,"
            "  gamma REAL NOT NULL,"
            "  last_calibrated TEXT NOT NULL"
            ");";
        exec_sql(sql_coeffs);

        const char* sql_error =
            "CREATE TABLE IF NOT EXISTS albedo_error_stats ("
            "  id INTEGER PRIMARY KEY,"
            "  systematic_error REAL NOT NULL,"
            "  drift_per_year REAL NOT NULL,"
            "  last_updated TEXT NOT NULL"
            ");";
        exec_sql(sql_error);

        const char* sql_base_ker_e =
            "CREATE TABLE IF NOT EXISTS ker_e_baseline ("
            "  id INTEGER PRIMARY KEY,"
            "  ker_e_base REAL NOT NULL,"
            "  last_updated TEXT NOT NULL"
            ");";
        exec_sql(sql_base_ker_e);

        const char* sql_bias_log =
            "CREATE TABLE IF NOT EXISTS albedo_ker_e_bias_log ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  ts TEXT NOT NULL,"
            "  bias_now REAL NOT NULL,"
            "  bias_after_years REAL NOT NULL,"
            "  years_to_recalibration INTEGER NOT NULL"
            ");";
        exec_sql(sql_bias_log);
    }

    AlbedoSensitivity load_sensitivity() {
        const char* sql =
            "SELECT alpha, beta, gamma "
            "FROM albedo_sensitivity_coeffs "
            "ORDER BY last_calibrated DESC LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        AlbedoSensitivity s{0.5, 1.0, 1.0};
        if (rc == SQLITE_OK) {
            rc = sqlite3_step(stmt);
            if (rc == SQLITE_ROW) {
                s.alpha = sqlite3_column_double(stmt, 0);
                s.beta  = sqlite3_column_double(stmt, 1);
                s.gamma = sqlite3_column_double(stmt, 2);
            }
        }
        sqlite3_finalize(stmt);
        return s;
    }

    AlbedoErrorParams load_error_params() {
        const char* sql =
            "SELECT systematic_error, drift_per_year "
            "FROM albedo_error_stats "
            "ORDER BY last_updated DESC LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        AlbedoErrorParams e{0.02, 0.001};
        if (rc == SQLITE_OK) {
            rc = sqlite3_step(stmt);
            if (rc == SQLITE_ROW) {
                e.systematic_error = sqlite3_column_double(stmt, 0);
                e.drift_per_year   = sqlite3_column_double(stmt, 1);
            }
        }
        sqlite3_finalize(stmt);
        return e;
    }

    double load_base_ker_e() {
        const char* sql =
            "SELECT ker_e_base "
            "FROM ker_e_baseline "
            "ORDER BY last_updated DESC LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        double ker_e_base = 1.0;
        if (rc == SQLITE_OK) {
            rc = sqlite3_step(stmt);
            if (rc == SQLITE_ROW) {
                ker_e_base = sqlite3_column_double(stmt, 0);
            }
        }
        sqlite3_finalize(stmt);
        return ker_e_base;
    }

    double propagate_bias(const AlbedoSensitivity& sens,
                          const AlbedoErrorParams& err,
                          double years) const {
        double delta_albedo = err.systematic_error + err.drift_per_year * years;
        double delta_lst    = sens.alpha * delta_albedo;
        double delta_energy = sens.beta * delta_lst;
        double delta_ker_e  = sens.gamma * delta_energy;
        return delta_ker_e;
    }

    void store_bias_result(const KerEBiasResult& result) {
        const char* sql =
            "INSERT INTO albedo_ker_e_bias_log("
            "  ts, bias_now, bias_after_years, years_to_recalibration"
            ") VALUES(datetime('now'), ?, ?, ?);";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            sqlite3_finalize(stmt);
            return;
        }
        sqlite3_bind_double(stmt, 1, result.bias_now);
        sqlite3_bind_double(stmt, 2, result.bias_after_years);
        sqlite3_bind_int(stmt, 3, result.years_to_recalibration);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
};

double kerEBiasOverYears(double albedo_error,
                         const AlbedoChainParams& params,
                         double baseline_daily_energy,
                         int years) {
    double dKerE_per_day = params.gamma_ker_e
                           * params.beta_energy
                           * params.alpha_lst
                           * albedo_error;
    double days = 365.0 * static_cast<double>(years);
    double cumulative_bias = dKerE_per_day * days;
    return cumulative_bias;
}

double requiredRecalibrationYears(double albedo_error,
                                  const AlbedoChainParams& params,
                                  double baseline_daily_energy,
                                  double epsilon,
                                  int totalYears) {
    double baseline_kerE_per_day =
        params.gamma_ker_e * baseline_daily_energy;
    double baseline_cumulative =
        baseline_kerE_per_day * 365.0 * static_cast<double>(totalYears);
    double allowed_bias = epsilon * baseline_cumulative;
    double bias_per_year = kerEBiasOverYears(albedo_error, params, baseline_daily_energy, 1);
    if (bias_per_year <= 0.0) return static_cast<double>(totalYears);
    double years_between_recalibration = allowed_bias / bias_per_year;
    if (years_between_recalibration > totalYears) years_between_recalibration = static_cast<double>(totalYears);
    return years_between_recalibration;
}

} // namespace eco_restoration
} // namespace prometheus_praxis

int main(int argc, char* argv[]) {
    using namespace prometheus_praxis::eco_restoration;

    std::string db_path = "albedo_ker_e.db";
    if (argc > 1) {
        db_path = argv[1];
    }

    try {
        AlbedoErrorPropagator propagator(db_path);
        KerEBiasResult result = propagator.compute_bias_and_recalibration(10, 0.05);

        std::cout << "Albedo-induced ker_e bias now=" << result.bias_now
                  << " bias_after_years=" << result.bias_after_years
                  << " years_to_recalibration=" << result.years_to_recalibration
                  << std::endl;

        AlbedoChainParams params;
        params.alpha_lst   = -10.0;
        params.beta_energy = 0.5;
        params.gamma_ker_e = 1.0;

        double albedo_error = 0.02;
        double baseline_daily_energy = 100.0;
        int years = 10;

        double bias10 = kerEBiasOverYears(albedo_error, params, baseline_daily_energy, years);
        double recYears = requiredRecalibrationYears(albedo_error, params, baseline_daily_energy, 0.05, years);

        std::cout << "Cumulative ker_e bias over " << years << " years: " << bias10 << std::endl;
        std::cout << "Recalibration interval (years) to keep error <5%: " << recYears << std::endl;
    } catch (const std::exception& ex) {
        std::cerr << "Albedo error propagation error: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
