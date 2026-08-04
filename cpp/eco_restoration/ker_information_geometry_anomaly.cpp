// File: cpp/eco_restoration/ker_information_geometry_anomaly.cpp

#include <vector>
#include <string>
#include <cmath>
#include <stdexcept>
#include <sqlite3.h>

struct KerTriple {
    double ker_k;
    double ker_e;
    double ker_r;
};

// Parameters of log-normal inflow and KER manifold
struct KerManifoldParams {
    double mu_k;    // mean of ker_k
    double mu_e;    // mean of ker_e
    double mu_r;    // mean of ker_r
    double sigma2_k;
    double sigma2_e;
    double sigma2_r;
};

// Online anomaly score based on Fisher-Rao distance
class KerInfoGeometryAnomaly {
public:
    explicit KerInfoGeometryAnomaly(sqlite3* db)
        : db_(db) {
        if (!db_) throw std::runtime_error("SQLite DB pointer must not be null");
    }

    KerManifoldParams loadParams(const std::string& node_id) {
        const char* sql =
            "SELECT mu_k, mu_e, mu_r, sigma2_k, sigma2_e, sigma2_r "
            "FROM ker_manifold_params "
            "WHERE node_id = ? "
            "ORDER BY updated_utc DESC "
            "LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare ker_manifold_params query");
        }
        sqlite3_bind_text(stmt, 1, node_id.c_str(), -1, SQLITE_TRANSIENT);

        KerManifoldParams p{};
        int rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            p.mu_k     = sqlite3_column_double(stmt, 0);
            p.mu_e     = sqlite3_column_double(stmt, 1);
            p.mu_r     = sqlite3_column_double(stmt, 2);
            p.sigma2_k = sqlite3_column_double(stmt, 3);
            p.sigma2_e = sqlite3_column_double(stmt, 4);
            p.sigma2_r = sqlite3_column_double(stmt, 5);
        }
        sqlite3_finalize(stmt);
        return p;
    }

    // Fisher-Rao distance for diagonal metric (approximate)
    double fisherRaoDistance(const KerManifoldParams& p,
                             const KerTriple& sample) const {
        // For independent coordinates with variances sigma2_*,
        // Fisher information metric is diag(1/sigma2_k, 1/sigma2_e, 1/sigma2_r).
        // Geodesic distance approximation reduces to weighted Euclidean:
        double dk = sample.ker_k - p.mu_k;
        double de = sample.ker_e - p.mu_e;
        double dr = sample.ker_r - p.mu_r;

        double gkk = (p.sigma2_k > 0.0) ? 1.0 / p.sigma2_k : 0.0;
        double gee = (p.sigma2_e > 0.0) ? 1.0 / p.sigma2_e : 0.0;
        double grr = (p.sigma2_r > 0.0) ? 1.0 / p.sigma2_r : 0.0;

        double dist2 = gkk * dk * dk + gee * de * de + grr * dr * dr;
        return std::sqrt(dist2);
    }

    // Online anomaly detection: compute distance and compare to threshold.
    bool isAnomalous(const std::string& node_id,
                     const KerTriple& sample,
                     double threshold) {
        KerManifoldParams p = loadParams(node_id);
        double d = fisherRaoDistance(p, sample);
        return (d > threshold);
    }

private:
    sqlite3* db_;
};
