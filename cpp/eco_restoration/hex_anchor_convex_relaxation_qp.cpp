// File: cpp/eco_restoration/hex_anchor_convex_relaxation_qp.cpp

#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <stdexcept>
#include <sqlite3.h>
#include "osqp.h"   // OSQP C API header, assumed available

// Convex relaxation of integer tree-count hex-anchor optimization:
//   Decision variables x_h (relaxed tree counts per hex), originally integers.
// Objective:
//   minimize 0.5 * x^T L x + c^T x
// where L is a graph Laplacian (thermal/anchor smoothness) and c encodes
// per-hex costs (e.g., vulnerability-weighted penalties).
//
// This C++ module:
//   - Builds L and c from SQL adjacency and hex metadata.
//   - Solves the convex QP with OSQP to obtain relaxed x_h^*.
//   - Stores relaxed solution into SQL for Lua rounding heuristics and comparison.

namespace prometheus_praxis {
namespace eco_restoration {

struct HexNode {
    std::string h3_index;
    double cost;        // linear term c_h
    double min_count;   // lower bound (e.g., 0)
    double max_count;   // upper bound (e.g., budget per hex)
};

struct EdgeEntry {
    int i;
    int j;
    double weight;
};

class HexLaplacianQP {
public:
    HexLaplacianQP(const std::string& db_path)
        : db_path_(db_path), db_(nullptr), n_(0), m_(0),
          work_(nullptr), data_(nullptr), settings_(nullptr) {}

    ~HexLaplacianQP() {
        cleanup();
    }

    void solve_and_store_relaxed_plan() {
        open_db();
        load_hex_nodes();
        load_edges_and_build_laplacian();
        setup_qp();
        run_osqp();
        store_relaxed_solution();
        cleanup();
    }

private:
    std::string db_path_;
    sqlite3* db_;
    std::vector<HexNode> nodes_;
    std::unordered_map<std::string, int> node_index_;
    std::vector<EdgeEntry> edges_;
    std::size_t n_;
    std::size_t m_;

    OSQPWorkspace* work_;
    OSQPData* data_;
    OSQPSettings* settings_;

    void open_db() {
        int rc = sqlite3_open(db_path_.c_str(), &db_);
        if (rc != SQLITE_OK) {
            std::string msg = sqlite3_errmsg(db_);
            sqlite3_close(db_);
            db_ = nullptr;
            throw std::runtime_error("Cannot open DB: " + msg);
        }
    }

    void load_hex_nodes() {
        const char* sql =
            "SELECT h3_index, thermal_weight, carbon_potential "
            "FROM hex_restoration_commitment;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            throw std::runtime_error("Prepare hex_restoration_commitment failed: " +
                                     std::string(sqlite3_errmsg(db_)));
        }

        nodes_.clear();
        node_index_.clear();
        int idx = 0;
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            HexNode node{};
            const unsigned char* h3 = sqlite3_column_text(stmt, 0);
            node.h3_index = h3 ? reinterpret_cast<const char*>(h3) : "";

            double thermal_weight = sqlite3_column_double(stmt, 1);
            double carbon_potential = sqlite3_column_double(stmt, 2);

            // Linear cost c_h: penalize high counts in low-potential / low-thermal-weight hexes.
            node.cost = thermal_weight * carbon_potential;

            node.min_count = 0.0;
            node.max_count = 1000.0; // generic upper bound; can be refined by budget.

            node_index_[node.h3_index] = idx;
            nodes_.push_back(node);
            idx++;
        }
        sqlite3_finalize(stmt);

        if (nodes_.empty()) {
            throw std::runtime_error("No hex_restoration_commitment rows found");
        }

        n_ = nodes_.size();
    }

    void load_edges_and_build_laplacian() {
        const char* sql =
            "SELECT h3_index, neighbor_h3_index "
            "FROM hex_neighbors;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            throw std::runtime_error("Prepare hex_neighbors failed: " +
                                     std::string(sqlite3_errmsg(db_)));
        }

        edges_.clear();
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            const unsigned char* h3_a = sqlite3_column_text(stmt, 0);
            const unsigned char* h3_b = sqlite3_column_text(stmt, 1);
            std::string a = h3_a ? reinterpret_cast<const char*>(h3_a) : "";
            std::string b = h3_b ? reinterpret_cast<const char*>(h3_b) : "";
            auto it_a = node_index_.find(a);
            auto it_b = node_index_.find(b);
            if (it_a == node_index_.end() || it_b == node_index_.end()) continue;

            int i = it_a->second;
            int j = it_b->second;

            // Weight: base smoothness weight scaled by mean carbon_potential.
            double pi_mean = 0.5 * (
                nodes_[i].cost + nodes_[j].cost
            );
            double w = 1.0 + std::fabs(pi_mean);
            EdgeEntry e{i, j, w};
            edges_.push_back(e);
        }
        sqlite3_finalize(stmt);

        m_ = edges_.size();
        if (m_ == 0) {
            throw std::runtime_error("No edges found in hex_neighbors; Laplacian is empty");
        }
    }

    void setup_qp() {
        // Construct Laplacian matrix L in sparse CSC form.
        // For each edge (i,j) with weight w:
        //   L_ii += w, L_jj += w, L_ij -= w, L_ji -= w.
        std::vector<double> L_diag(n_, 0.0);
        std::vector<std::unordered_map<int, double>> L_off(n_);

        for (const auto& e : edges_) {
            int i = e.i;
            int j = e.j;
            double w = e.weight;
            L_diag[i] += w;
            L_diag[j] += w;
            L_off[i][j] -= w;
            L_off[j][i] -= w;
        }

        // Build P: since objective is 0.5 * x^T L x, P = L.
        std::vector<c_int> P_i;
        std::vector<c_int> P_p(n_ + 1);
        std::vector<c_float> P_x;

        int nnz = 0;
        for (std::size_t j = 0; j < n_; ++j) {
            P_p[j] = nnz;
            // diagonal
            if (std::fabs(L_diag[j]) > 1e-12) {
                P_i.push_back(static_cast<c_int>(j));
                P_x.push_back(static_cast<c_float>(L_diag[j]));
                nnz++;
            }
            // off-diagonals
            for (const auto& kv : L_off[j]) {
                int row = kv.first;
                double val = kv.second;
                if (std::fabs(val) < 1e-12) continue;
                P_i.push_back(static_cast<c_int>(row));
                P_x.push_back(static_cast<c_float>(val));
                nnz++;
            }
        }
        P_p[n_] = nnz;

        // Linear term q: c_h from nodes_.
        std::vector<c_float> q(n_);
        for (std::size_t i = 0; i < n_; ++i) {
            q[i] = static_cast<c_float>(nodes_[i].cost);
        }

        // Constraints: box constraints on x_h (min_count <= x_h <= max_count).
        // A = identity.
        std::vector<c_int> A_i(n_);
        std::vector<c_int> A_p(n_ + 1);
        std::vector<c_float> A_x(n_);
        for (std::size_t j = 0; j < n_; ++j) {
            A_p[j] = j;
            A_i[j] = j;
            A_x[j] = 1.0;
        }
        A_p[n_] = n_;

        std::vector<c_float> l(n_);
        std::vector<c_float> u(n_);
        for (std::size_t i = 0; i < n_; ++i) {
            l[i] = static_cast<c_float>(nodes_[i].min_count);
            u[i] = static_cast<c_float>(nodes_[i].max_count);
        }

        // Allocate OSQP data.
        data_ = (OSQPData*)c_malloc(sizeof(OSQPData));
        data_->n = n_;
        data_->m = n_;
        data_->P = csc_matrix(data_->n, data_->n,
                              (c_int)P_x.size(),
                              P_x.data(), P_i.data(), P_p.data());
        data_->q = q.data();
        data_->A = csc_matrix(data_->m, data_->n,
                              (c_int)A_x.size(),
                              A_x.data(), A_i.data(), A_p.data());
        data_->l = l.data();
        data_->u = u.data();

        // Settings.
        settings_ = (OSQPSettings*)c_malloc(sizeof(OSQPSettings));
        osqp_set_default_settings(settings_);
        settings_->alpha = 1.6;
        settings_->eps_abs = 1e-4;
        settings_->eps_rel = 1e-4;
        settings_->verbose = 0;
        settings_->max_iter = 4000;

        // Setup.
        c_int exitflag = osqp_setup(&work_, data_, settings_);
        if (exitflag != 0) {
            throw std::runtime_error("OSQP setup failed");
        }
    }

    void run_osqp() {
        osqp_solve(work_);
        if (work_->info->status_val != OSQP_SOLVED) {
            throw std::runtime_error("OSQP QP did not solve to optimality; status=" +
                                     std::string(work_->info->status));
        }
    }

    void store_relaxed_solution() {
        const c_float* x = work_->solution->x;
        // Schema for relaxed vs rounded plans:
        //   CREATE TABLE IF NOT EXISTS hex_anchor_relaxed (
        //       h3_index TEXT PRIMARY KEY,
        //       relaxed_count REAL NOT NULL,
        //       rounded_count INTEGER,
        //       laplacian_cost REAL,
        //       created_at TEXT NOT NULL
        //   );
        const char* sql_schema =
            "CREATE TABLE IF NOT EXISTS hex_anchor_relaxed ("
            "  h3_index TEXT PRIMARY KEY,"
            "  relaxed_count REAL NOT NULL,"
            "  rounded_count INTEGER,"
            "  laplacian_cost REAL,"
            "  created_at TEXT NOT NULL"
            ");";
        exec_sql(sql_schema);

        const char* sql_ins =
            "INSERT OR REPLACE INTO hex_anchor_relaxed("
            "  h3_index, relaxed_count, rounded_count, laplacian_cost, created_at"
            ") VALUES (?, ?, NULL, ?, datetime('now'));";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, sql_ins, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            throw std::runtime_error("Prepare insert relaxed solution failed: " +
                                     std::string(sqlite3_errmsg(db_)));
        }

        // Compute Laplacian cost 0.5 * x^T L x for reporting; we reuse L_diag and L_off.
        // For simplicity, recompute here.
        std::vector<double> x_vec(n_);
        for (std::size_t i = 0; i < n_; ++i) {
            x_vec[i] = static_cast<double>(x[i]);
        }

        for (std::size_t i = 0; i < n_; ++i) {
            sqlite3_reset(stmt);
            sqlite3_clear_bindings(stmt);
            rc = sqlite3_bind_text(stmt, 1, nodes_[i].h3_index.c_str(), -1, SQLITE_TRANSIENT);
            if (rc != SQLITE_OK) goto bind_error;
            rc = sqlite3_bind_double(stmt, 2, x_vec[i]);
            if (rc != SQLITE_OK) goto bind_error;
            // Individual contribution to Laplacian cost is deferred; store 0.0 as placeholder.
            rc = sqlite3_bind_double(stmt, 3, 0.0);
            if (rc != SQLITE_OK) goto bind_error;

            rc = sqlite3_step(stmt);
            if (rc != SQLITE_DONE) {
                sqlite3_finalize(stmt);
                throw std::runtime_error("Insert relaxed solution step failed: " +
                                         std::string(sqlite3_errmsg(db_)));
            }
        }

        sqlite3_finalize(stmt);
        std::cout << "Relaxed hex-anchor QP solution stored in hex_anchor_relaxed." << std::endl;
        return;

    bind_error:
        sqlite3_finalize(stmt);
        throw std::runtime_error("Bind error inserting relaxed solution: " +
                                 std::string(sqlite3_errmsg(db_)));
    }

    void exec_sql(const char* sql) {
        char* errmsg = nullptr;
        int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &errmsg);
        if (rc != SQLITE_OK) {
            std::string msg = errmsg ? errmsg : "";
            sqlite3_free(errmsg);
            throw std::runtime_error("SQLite exec error: " + msg);
        }
    }

    void cleanup() {
        if (work_) {
            osqp_cleanup(work_);
            work_ = nullptr;
        }
        if (data_) {
            c_free(data_);
            data_ = nullptr;
        }
        if (settings_) {
            c_free(settings_);
            settings_ = nullptr;
        }
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
    }
};

} // namespace eco_restoration
} // namespace prometheus_praxis

int main(int argc, char* argv[]) {
    using namespace prometheus_praxis::eco_restoration;

    std::string db_path = "hex_restoration.db";
    if (argc > 1) {
        db_path = argv[1];
    }

    try {
        HexLaplacianQP qp(db_path);
        qp.solve_and_store_relaxed_plan();
    } catch (const std::exception& ex) {
        std::cerr << "Hex-anchor convex relaxation error: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
