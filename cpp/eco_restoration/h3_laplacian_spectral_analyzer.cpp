// File: cpp/eco_restoration/h3_laplacian_spectral_analyzer.cpp

#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <cmath>
#include <sqlite3.h>

namespace prometheus_praxis {
namespace eco_restoration {

struct HexNode {
    int node_id;
    std::string h3_index;
};

struct EdgeEntry {
    int from;
    int to;
    double weight;
};

class LaplacianSpectralAnalyzer {
public:
    explicit LaplacianSpectralAnalyzer(const std::string& db_path,
                                       std::size_t max_power_iters = 200,
                                       double tol = 1e-6)
        : db_path_(db_path),
          db_(nullptr),
          max_power_iters_(max_power_iters),
          tol_(tol) {
        open_db();
    }

    ~LaplacianSpectralAnalyzer() {
        if (db_) {
            sqlite3_close(db_);
        }
    }

    void analyze_and_store_sensor_placement(std::size_t k_top) {
        load_nodes();
        load_edges();
        build_laplacian();
        compute_low_frequency_eigenvector();
        score_nodes();
        store_sensor_placement(k_top);
    }

private:
    std::string db_path_;
    sqlite3* db_;
    std::vector<HexNode> nodes_;
    std::unordered_map<int, std::size_t> node_index_;
    std::vector<EdgeEntry> edges_;
    std::size_t n_;
    std::vector<std::vector<double>> L_rows_;
    std::vector<double> eigenvector_;
    std::vector<double> scores_;
    std::size_t max_power_iters_;
    double tol_;

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

    void load_nodes() {
        const char* sql =
            "SELECT node_id, h3_index "
            "FROM cool_corridor_node;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            sqlite3_finalize(stmt);
            throw std::runtime_error("Prepare cool_corridor_node failed: " +
                                     std::string(sqlite3_errmsg(db_)));
        }

        nodes_.clear();
        node_index_.clear();
        std::size_t idx = 0;
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            HexNode node{};
            node.node_id = sqlite3_column_int(stmt, 0);
            const unsigned char* h3 = sqlite3_column_text(stmt, 1);
            node.h3_index = h3 ? reinterpret_cast<const char*>(h3) : "";
            nodes_.push_back(node);
            node_index_[node.node_id] = idx;
            idx++;
        }
        sqlite3_finalize(stmt);

        if (nodes_.empty()) {
            throw std::runtime_error("No nodes in cool_corridor_node");
        }
        n_ = nodes_.size();
    }

    void load_edges() {
        const char* sql =
            "SELECT from_node_id, to_node_id, edge_weight "
            "FROM cool_corridor_edge;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            sqlite3_finalize(stmt);
            throw std::runtime_error("Prepare cool_corridor_edge failed: " +
                                     std::string(sqlite3_errmsg(db_)));
        }

        edges_.clear();
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            int from_id = sqlite3_column_int(stmt, 0);
            int to_id   = sqlite3_column_int(stmt, 1);
            double w    = sqlite3_column_double(stmt, 2);
            auto it_from = node_index_.find(from_id);
            auto it_to   = node_index_.find(to_id);
            if (it_from == node_index_.end() || it_to == node_index_.end()) continue;
            EdgeEntry e{};
            e.from = static_cast<int>(it_from->second);
            e.to   = static_cast<int>(it_to->second);
            e.weight = w;
            edges_.push_back(e);
        }
        sqlite3_finalize(stmt);

        if (edges_.empty()) {
            throw std::runtime_error("No edges in cool_corridor_edge");
        }
    }

    void build_laplacian() {
        L_rows_.assign(n_, std::vector<double>(n_, 0.0));
        std::vector<double> degree(n_, 0.0);

        for (const auto& e : edges_) {
            int i = e.from;
            int j = e.to;
            double w = e.weight;
            if (i == j) continue;
            degree[i] += w;
            degree[j] += w;
            L_rows_[i][j] -= w;
            L_rows_[j][i] -= w;
        }

        for (std::size_t i = 0; i < n_; ++i) {
            L_rows_[i][i] = degree[i];
        }
    }

    void compute_low_frequency_eigenvector() {
        eigenvector_.assign(n_, 0.0);
        std::vector<double> v(n_, 1.0);
        normalize(v);

        std::vector<double> Lv(n_, 0.0);

        for (std::size_t iter = 0; iter < max_power_iters_; ++iter) {
            multiply_L(v, Lv);
            double lambda = rayleigh_quotient(v, Lv);
            std::vector<double> w = Lv;
            normalize(w);

            double diff = 0.0;
            for (std::size_t i = 0; i < n_; ++i) {
                double d = w[i] - v[i];
                diff += d * d;
            }
            v = w;
            if (diff < tol_) break;
        }

        eigenvector_ = v;
    }

    void multiply_L(const std::vector<double>& v, std::vector<double>& result) {
        result.assign(n_, 0.0);
        for (std::size_t i = 0; i < n_; ++i) {
            double sum = 0.0;
            const auto& row = L_rows_[i];
            for (std::size_t j = 0; j < n_; ++j) {
                sum += row[j] * v[j];
            }
            result[i] = sum;
        }
    }

    double rayleigh_quotient(const std::vector<double>& v,
                             const std::vector<double>& Lv) const {
        double num = 0.0;
        double den = 0.0;
        for (std::size_t i = 0; i < n_; ++i) {
            num += v[i] * Lv[i];
            den += v[i] * v[i];
        }
        return (den > 0.0) ? num / den : 0.0;
    }

    void normalize(std::vector<double>& v) const {
        double norm = 0.0;
        for (double x : v) norm += x * x;
        norm = std::sqrt(norm);
        if (norm <= 0.0) return;
        for (double& x : v) x /= norm;
    }

    void score_nodes() {
        scores_.assign(n_, 0.0);
        for (std::size_t i = 0; i < n_; ++i) {
            scores_[i] = eigenvector_[i] * eigenvector_[i];
        }
    }

    void store_sensor_placement(std::size_t k_top) {
        const char* sql_schema =
            "CREATE TABLE IF NOT EXISTS sensor_placement ("
            "  node_id INTEGER PRIMARY KEY,"
            "  h3_index TEXT NOT NULL,"
            "  spectral_score REAL NOT NULL,"
            "  selected INTEGER NOT NULL,"
            "  created_at TEXT NOT NULL"
            ");";
        exec_sql(sql_schema);

        std::vector<std::size_t> indices(n_);
        for (std::size_t i = 0; i < n_; ++i) indices[i] = i;
        std::sort(indices.begin(), indices.end(),
                  [&](std::size_t a, std::size_t b) {
                      return scores_[a] > scores_[b];
                  });

        const char* sql_ins =
            "INSERT OR REPLACE INTO sensor_placement("
            "  node_id, h3_index, spectral_score, selected, created_at"
            ") VALUES (?, ?, ?, ?, datetime('now'));";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, sql_ins, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            sqlite3_finalize(stmt);
            throw std::runtime_error("Prepare insert sensor_placement failed: " +
                                     std::string(sqlite3_errmsg(db_)));
        }

        for (std::size_t i = 0; i < n_; ++i) {
            std::size_t idx = indices[i];
            int selected = (i < k_top) ? 1 : 0;

            sqlite3_reset(stmt);
            sqlite3_clear_bindings(stmt);
            sqlite3_bind_int(stmt, 1, nodes_[idx].node_id);
            sqlite3_bind_text(stmt, 2, nodes_[idx].h3_index.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_double(stmt, 3, scores_[idx]);
            sqlite3_bind_int(stmt, 4, selected);
            sqlite3_step(stmt);
        }

        sqlite3_finalize(stmt);

        std::cout << "Sensor placement updated; top " << k_top
                  << " nodes selected based on spectral scores." << std::endl;
    }
};

} // namespace eco_restoration
} // namespace prometheus_praxis

int main(int argc, char* argv[]) {
    using namespace prometheus_praxis::eco_restoration;

    std::string db_path = "cool_corridor.db";
    std::size_t k_top = 50;
    if (argc > 1) {
        db_path = argv[1];
    }
    if (argc > 2) {
        k_top = static_cast<std::size_t>(std::stoul(argv[2]));
    }

    try {
        LaplacianSpectralAnalyzer analyzer(db_path);
        analyzer.analyze_and_store_sensor_placement(k_top);
    } catch (const std::exception& ex) {
        std::cerr << "Laplacian spectral analyzer error: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
