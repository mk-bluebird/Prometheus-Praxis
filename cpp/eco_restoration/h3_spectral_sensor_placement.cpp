// File: cpp/eco_restoration/h3_spectral_sensor_placement.cpp

#include <vector>
#include <string>
#include <stdexcept>
#include <unordered_map>
#include <cmath>
#include <sqlite3.h>

// Simple dense matrix and vector for small to medium graphs
struct Matrix {
    std::size_t n;
    std::vector<double> a; // row-major

    Matrix(std::size_t n_) : n(n_), a(n_ * n_, 0.0) {}

    double& operator()(std::size_t i, std::size_t j) {
        return a[i * n + j];
    }
    double operator()(std::size_t i, std::size_t j) const {
        return a[i * n + j];
    }
};

struct Vector {
    std::size_t n;
    std::vector<double> v;

    Vector(std::size_t n_) : n(n_), v(n_, 0.0) {}

    double& operator[](std::size_t i) { return v[i]; }
    double  operator[](std::size_t i) const { return v[i]; }
};

// Power iteration to approximate smallest non-zero eigenpairs of Laplacian via shift
// For production, a proper symmetric eigensolver (e.g. Eigen, LAPACK) should be used.
class LaplacianSpectralAnalyzer {
public:
    explicit LaplacianSpectralAnalyzer(sqlite3* db)
        : db_(db) {
        if (!db_) {
            throw std::runtime_error("SQLite DB pointer must not be null");
        }
    }

    void analyzeAndStoreSensorPlacement(std::size_t numSensors) {
        // Load H3 hex registry and neighbor graph from SQLite
        std::vector<std::string> hexIds;
        std::unordered_map<std::string, std::size_t> index;
        loadHexRegistry(hexIds, index);

        Matrix L = buildGraphLaplacian(hexIds, index);

        // Compute first few eigenvectors corresponding to smallest non-zero eigenvalues
        // Here we compute k eigenvectors and score nodes by combined magnitude.
        std::size_t k = std::min<std::size_t>(3, hexIds.size());
        std::vector<Vector> eigenvectors;
        computeSmallEigenvectors(L, k, eigenvectors);

        // Score each node by sum of squared components across eigenvectors
        std::vector<double> scores(hexIds.size(), 0.0);
        for (std::size_t i = 0; i < hexIds.size(); ++i) {
            double s = 0.0;
            for (const auto& ev : eigenvectors) {
                s += ev[i] * ev[i];
            }
            scores[i] = s;
        }

        // Select top numSensors nodes by score
        std::vector<std::size_t> chosen = selectTopIndices(scores, numSensors);

        // Store sensor placement in SQL
        writeSensorPlacement(hexIds, scores, chosen);
    }

private:
    sqlite3* db_;

    void loadHexRegistry(std::vector<std::string>& hexIds,
                         std::unordered_map<std::string, std::size_t>& index) {
        const char* sql =
            "SELECT h3_index "
            "FROM phoenix_hex_registry "
            "ORDER BY h3_index;";

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare phoenix_hex_registry query");
        }

        std::size_t i = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char* txt = sqlite3_column_text(stmt, 0);
            std::string h3 = txt ? reinterpret_cast<const char*>(txt) : "";
            index[h3] = i;
            hexIds.push_back(h3);
            ++i;
        }

        sqlite3_finalize(stmt);
    }

    Matrix buildGraphLaplacian(const std::vector<std::string>& hexIds,
                               const std::unordered_map<std::string, std::size_t>& index) {
        std::size_t n = hexIds.size();
        Matrix L(n);

        // Adjacency from neighbor cache
        const char* sql =
            "SELECT src_h3, dst_h3 "
            "FROM hex_neighbor_edges;";

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare hex_neighbor_edges query");
        }

        std::vector<int> degree(n, 0);

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const unsigned char* srcTxt = sqlite3_column_text(stmt, 0);
            const unsigned char* dstTxt = sqlite3_column_text(stmt, 1);
            std::string src = srcTxt ? reinterpret_cast<const char*>(srcTxt) : "";
            std::string dst = dstTxt ? reinterpret_cast<const char*>(dstTxt) : "";

            auto itSrc = index.find(src);
            auto itDst = index.find(dst);
            if (itSrc == index.end() || itDst == index.end()) continue;

            std::size_t i = itSrc->second;
            std::size_t j = itDst->second;

            if (i == j) continue;

            L(i, j) -= 1.0;
            L(j, i) -= 1.0;
            degree[i] += 1;
            degree[j] += 1;
        }

        sqlite3_finalize(stmt);

        for (std::size_t i = 0; i < n; ++i) {
            L(i, i) = static_cast<double>(degree[i]);
        }

        return L;
    }

    void computeSmallEigenvectors(const Matrix& L,
                                  std::size_t k,
                                  std::vector<Vector>& eigenvectors) {
        std::size_t n = L.n;
        // Simple power iterations with orthogonalization to approximate low-frequency modes
        // Note: This is illustrative; for accurate spectrum use a robust library.

        // Start with random initial vectors
        eigenvectors.clear();
        for (std::size_t m = 0; m < k; ++m) {
            Vector v(n);
            for (std::size_t i = 0; i < n; ++i) {
                v[i] = static_cast<double>(std::rand()) / RAND_MAX;
            }
            normalize(v);
            // Power iteration on (L + shift*I)^-1 or inverse iteration; here approximate low modes
            for (int iter = 0; iter < 200; ++iter) {
                Vector w = multiply(L, v);
                // Orthogonalize against previous eigenvectors to avoid duplicates
                for (const auto& prev : eigenvectors) {
                    double dot = dotProduct(w, prev);
                    for (std::size_t i = 0; i < n; ++i) {
                        w[i] -= dot * prev[i];
                    }
                }
                normalize(w);
                v = w;
            }
            eigenvectors.push_back(v);
        }
    }

    Vector multiply(const Matrix& M, const Vector& v) {
        std::size_t n = M.n;
        Vector res(n);
        for (std::size_t i = 0; i < n; ++i) {
            double sum = 0.0;
            for (std::size_t j = 0; j < n; ++j) {
                sum += M(i, j) * v[j];
            }
            res[i] = sum;
        }
        return res;
    }

    void normalize(Vector& v) {
        double norm = 0.0;
        for (double x : v.v) norm += x * x;
        norm = std::sqrt(norm);
        if (norm > 0.0) {
            for (double& x : v.v) x /= norm;
        }
    }

    double dotProduct(const Vector& a, const Vector& b) {
        double sum = 0.0;
        for (std::size_t i = 0; i < a.n; ++i) {
            sum += a[i] * b[i];
        }
        return sum;
    }

    std::vector<std::size_t> selectTopIndices(const std::vector<double>& scores,
                                              std::size_t numSensors) {
        std::vector<std::size_t> idx(scores.size());
        for (std::size_t i = 0; i < scores.size(); ++i) idx[i] = i;
        std::sort(idx.begin(), idx.end(),
                  [&](std::size_t a, std::size_t b) { return scores[a] > scores[b]; });
        if (numSensors > idx.size()) numSensors = idx.size();
        idx.resize(numSensors);
        return idx;
    }

    void writeSensorPlacement(const std::vector<std::string>& hexIds,
                              const std::vector<double>& scores,
                              const std::vector<std::size_t>& chosen) {
        const char* sql_create =
            "CREATE TABLE IF NOT EXISTS sensor_placement ("
            " h3_index       TEXT PRIMARY KEY,"
            " spectral_score REAL NOT NULL,"
            " selected       INTEGER NOT NULL,"
            " updated_utc    INTEGER NOT NULL"
            ");";

        char* errMsg = nullptr;
        if (sqlite3_exec(db_, sql_create, nullptr, nullptr, &errMsg) != SQLITE_OK) {
            std::string msg = "Failed to create sensor_placement: ";
            if (errMsg) msg += errMsg;
            sqlite3_free(errMsg);
            throw std::runtime_error(msg);
        }

        std::vector<int> isChosen(hexIds.size(), 0);
        for (std::size_t i : chosen) {
            if (i < isChosen.size()) isChosen[i] = 1;
        }

        const char* sql_insert =
            "INSERT OR REPLACE INTO sensor_placement "
            " (h3_index, spectral_score, selected, updated_utc) "
            "VALUES (?, ?, ?, strftime('%s','now'));";

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql_insert, -1, &stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare sensor_placement insert");
        }

        for (std::size_t i = 0; i < hexIds.size(); ++i) {
            sqlite3_bind_text(stmt, 1, hexIds[i].c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_double(stmt, 2, scores[i]);
            sqlite3_bind_int(stmt,   3, isChosen[i]);
            if (sqlite3_step(stmt) != SQLITE_DONE) {
                sqlite3_reset(stmt);
                continue;
            }
            sqlite3_reset(stmt);
        }

        sqlite3_finalize(stmt);
    }
};
