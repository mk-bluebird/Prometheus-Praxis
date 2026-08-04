// File: cpp/tools/ou_parameter_update_service.cpp

#include <cmath>
#include <vector>
#include <stdexcept>
#include <string>
#include <sqlite3.h>

// Maximum likelihood OU fit for scalar series X_t
// Here we fit theta and sigma in discrete-time OU: X_{k+1} = exp(-theta dt) X_k + noise.
struct OUFitResult {
    double theta;
    double sigma;
};

class OUParameterFitter {
public:
    explicit OUParameterFitter(sqlite3* db, double dt_seconds)
        : db_(db), dt_(dt_seconds) {
        if (!db_) {
            throw std::runtime_error("SQLite DB pointer must not be null");
        }
        if (dt_ <= 0.0) {
            throw std::runtime_error("dt_seconds must be positive");
        }
    }

    OUFitResult fitFromTelemetry(int max_samples) {
        std::vector<double> xs = loadRecentPFAS(max_samples);
        if (xs.size() < 2) {
            return OUFitResult{0.0, 0.0};
        }

        double sum_xx = 0.0;
        double sum_xy = 0.0;
        double sum_x  = 0.0;
        double sum_y  = 0.0;
        int    n      = static_cast<int>(xs.size()) - 1;

        for (int k = 0; k < n; ++k) {
            double x = xs[k];
            double y = xs[k + 1];
            sum_xx += x * x;
            sum_xy += x * y;
            sum_x  += x;
            sum_y  += y;
        }

        // Least squares estimate of Lambda in y ≈ Lambda x
        double denom = sum_xx;
        double lambda_hat = (denom > 0.0) ? (sum_xy / denom) : 1.0;

        // Map Lambda to theta: Lambda = exp(-theta dt)
        double theta_hat = (lambda_hat > 0.0 && lambda_hat < 1.0)
                               ? (-std::log(lambda_hat) / dt_)
                               : 0.0;

        // Estimate sigma from residual variance
        double sum_res2 = 0.0;
        for (int k = 0; k < n; ++k) {
            double x = xs[k];
            double y = xs[k + 1];
            double res = y - lambda_hat * x;
            sum_res2 += res * res;
        }
        double var_res = sum_res2 / static_cast<double>(n);
        // For OU, noise variance relates to sigma and dt; treat sigma as sqrt(var_res/dt) here.
        double sigma_hat = (dt_ > 0.0) ? std::sqrt(var_res / dt_) : 0.0;

        return OUFitResult{theta_hat, sigma_hat};
    }

    void writeOUParamsToSqlite(const OUFitResult& res) {
        const char* sql_create =
            "CREATE TABLE IF NOT EXISTS ou_params ("
            " theta REAL NOT NULL,"
            " sigma REAL NOT NULL,"
            " updated_utc INTEGER NOT NULL"
            ");";

        char* errMsg = nullptr;
        if (sqlite3_exec(db_, sql_create, nullptr, nullptr, &errMsg) != SQLITE_OK) {
            std::string msg = "Failed to create ou_params: ";
            if (errMsg) msg += errMsg;
            sqlite3_free(errMsg);
            throw std::runtime_error(msg);
        }

        const char* sql_insert =
            "INSERT INTO ou_params (theta, sigma, updated_utc) "
            "VALUES (?, ?, strftime('%s','now'));";

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql_insert, -1, &stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare ou_params insert");
        }

        sqlite3_bind_double(stmt, 1, res.theta);
        sqlite3_bind_double(stmt, 2, res.sigma);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            throw std::runtime_error("Failed to insert ou_params");
        }

        sqlite3_finalize(stmt);
    }

private:
    sqlite3* db_;
    double   dt_;

    std::vector<double> loadRecentPFAS(int max_samples) {
        std::vector<double> xs;
        const char* sql =
            "SELECT pfas_conc "
            "FROM pfas_telemetry "
            "ORDER BY ts_utc DESC "
            "LIMIT ?;";

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare PFAS telemetry query");
        }

        sqlite3_bind_int(stmt, 1, max_samples);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            double x = sqlite3_column_double(stmt, 0);
            xs.push_back(x);
        }

        sqlite3_finalize(stmt);

        // reverse to chronological order
        std::reverse(xs.begin(), xs.end());
        return xs;
    }
};
