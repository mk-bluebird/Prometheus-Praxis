// File: cpp/eco_restoration/network_lyapunov_spectrum.cpp

#include <iostream>
#include <vector>
#include <string>
#include <stdexcept>
#include <sqlite3.h>
#include <cmath>

namespace prometheus_praxis {
namespace eco_restoration {

struct CrossCorrelationEntry {
    int from_segment;
    int to_segment;
    double corr;
};

struct LyapunovExponent {
    int index;
    double lambda;
};

void exec_sql(sqlite3* db, const std::string& sql) {
    char* errmsg = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errmsg);
    if (rc != SQLITE_OK) {
        std::string msg = errmsg ? errmsg : "";
        sqlite3_free(errmsg);
        throw std::runtime_error("SQLite error: " + msg);
    }
}

class NetworkLyapunovSpectrum {
public:
    explicit NetworkLyapunovSpectrum(const std::string& db_path)
        : db_path_(db_path), db_(nullptr) {
        open_db();
        install_schema();
    }

    ~NetworkLyapunovSpectrum() {
        if (db_) sqlite3_close(db_);
    }

    void compute_and_store_spectrum() {
        int n_segments = count_segments();
        if (n_segments <= 0) {
            throw std::runtime_error("No segments available for Lyapunov spectrum");
        }

        std::vector<double> A(n_segments * n_segments, 0.0);
        load_cross_correlation_matrix(n_segments, A);
        std::vector<double> eigenvalues = power_iteration_spectrum(A, n_segments);
        store_spectrum(eigenvalues);
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

    void install_schema() {
        const char* sql_corr =
            "CREATE TABLE IF NOT EXISTS cross_correlation_matrix ("
            "  from_segment INTEGER NOT NULL,"
            "  to_segment   INTEGER NOT NULL,"
            "  corr         REAL NOT NULL,"
            "  PRIMARY KEY(from_segment, to_segment)"
            ");";
        exec_sql(db_, sql_corr);

        const char* sql_spectrum =
            "CREATE TABLE IF NOT EXISTS network_lyapunov_spectrum ("
            "  idx          INTEGER PRIMARY KEY,"
            "  lambda       REAL NOT NULL,"
            "  computed_at  TEXT NOT NULL"
            ");";
        exec_sql(db_, sql_spectrum);
    }

    int count_segments() {
        const char* sql =
            "SELECT MAX(from_segment) AS max_seg FROM cross_correlation_matrix;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        int max_seg = 0;
        if (rc == SQLITE_OK) {
            rc = sqlite3_step(stmt);
            if (rc == SQLITE_ROW) {
                max_seg = sqlite3_column_int(stmt, 0);
            }
        }
        sqlite3_finalize(stmt);
        return max_seg + 1;
    }

    void load_cross_correlation_matrix(int n_segments, std::vector<double>& A) {
        const char* sql =
            "SELECT from_segment, to_segment, corr FROM cross_correlation_matrix;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            sqlite3_finalize(stmt);
            throw std::runtime_error("Prepare select cross_correlation_matrix failed: " +
                                     std::string(sqlite3_errmsg(db_)));
        }

        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            int i = sqlite3_column_int(stmt, 0);
            int j = sqlite3_column_int(stmt, 1);
            double c = sqlite3_column_double(stmt, 2);
            if (i >= 0 && i < n_segments && j >= 0 && j < n_segments) {
                A[i * n_segments + j] = c;
            }
        }
        sqlite3_finalize(stmt);
    }

    std::vector<double> power_iteration_spectrum(const std::vector<double>& A,
                                                int n) {
        std::vector<double> eigenvalues;
        std::vector<double> B = A;

        int k_max = std::min(n, 5);
        for (int k = 0; k < k_max; ++k) {
            std::vector<double> v(n, 1.0);
            double norm = 0.0;
            for (int iter = 0; iter < 50; ++iter) {
                std::vector<double> Av(n, 0.0);
                for (int i = 0; i < n; ++i) {
                    double sum = 0.0;
                    for (int j = 0; j < n; ++j) {
                        sum += B[i * n + j] * v[j];
                    }
                    Av[i] = sum;
                }
                norm = 0.0;
                for (int i = 0; i < n; ++i) {
                    norm += Av[i] * Av[i];
                }
                norm = std::sqrt(norm);
                if (norm <= 1e-12) break;
                for (int i = 0; i < n; ++i) {
                    v[i] = Av[i] / norm;
                }
            }
            double lambda = 0.0;
            for (int i = 0; i < n; ++i) {
                double sum = 0.0;
                for (int j = 0; j < n; ++j) {
                    sum += B[i * n + j] * v[j];
                }
                lambda += v[i] * sum;
            }
            eigenvalues.push_back(lambda);

            for (int i = 0; i < n; ++i) {
                for (int j = 0; j < n; ++j) {
                    B[i * n + j] -= lambda * v[i] * v[j];
                }
            }
        }

        std::vector<double> lyap;
        for (double lambda : eigenvalues) {
            double ly = std::log(std::fabs(lambda + 1e-12));
            lyap.push_back(ly);
        }
        return lyap;
    }

    void store_spectrum(const std::vector<double>& exponents) {
        exec_sql(db_, "DELETE FROM network_lyapunov_spectrum;");
        const char* sql =
            "INSERT INTO network_lyapunov_spectrum(idx, lambda, computed_at) "
            "VALUES(?, ?, datetime('now'));";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            sqlite3_finalize(stmt);
            return;
        }

        for (std::size_t i = 0; i < exponents.size(); ++i) {
            sqlite3_reset(stmt);
            sqlite3_clear_bindings(stmt);
            sqlite3_bind_int(stmt, 1, static_cast<int>(i));
            sqlite3_bind_double(stmt, 2, exponents[i]);
            sqlite3_step(stmt);
        }
        sqlite3_finalize(stmt);
    }
};

} // namespace eco_restoration
} // namespace prometheus_praxis

int main(int argc, char* argv[]) {
    using namespace prometheus_praxis::eco_restoration;

    std::string db_path = "pfas_network_lyapunov.db";
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
        NetworkLyapunovSpectrum spectrum(db_path);
        spectrum.compute_and_store_spectrum();
        std::cout << "Network Lyapunov spectrum computed and stored in "
                  << db_path << std::endl;

        std::cout << "\n-- Example query: network Lyapunov exponents --\n";
        std::cout << "SELECT * FROM network_lyapunov_spectrum ORDER BY idx;\n";
    } catch (const std::exception& ex) {
        std::cerr << "NetworkLyapunovSpectrum error: " << ex.what() << std::endl;
        sqlite3_close(db);
        return 1;
    }

    sqlite3_close(db);
    return 0;
}
