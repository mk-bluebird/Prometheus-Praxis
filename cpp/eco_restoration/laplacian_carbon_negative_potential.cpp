// File: cpp/eco_restoration/laplacian_carbon_negative_potential.cpp

#include <iostream>
#include <string>
#include <stdexcept>
#include <vector>
#include <unordered_map>
#include <cmath>
#include <sqlite3.h>

namespace prometheus_praxis {
namespace eco_restoration {

// Carbon-negative potential π_h for hex h:
//   π_h = w_c * ker_e_h + w_v * vulnerability_h
// where ker_e_h is a baseline eco-restoration energy/effort score and vulnerability_h
// is a heat-vulnerability metric (e.g., normalized 0..1).
//
// We then build a weighted Laplacian penalty:
//
//   L(alloc) = 0.5 * sum_{(h,i)∈E} w_{h,i} * (alloc_h - alloc_i)^2
//
// where weights w_{h,i} depend on π_h, π_i and vulnerability, so that smoothness
// does not force high allocation to low-carbon-potential / low-vulnerability cells.
//
// SQL schema assumptions:
//   hex_restoration_commitment(h3_index TEXT PRIMARY KEY,
//                              anchor_value REAL NOT NULL,
//                              vulnerability REAL NOT NULL,
//                              ker_e REAL NOT NULL,
//                              carbon_potential REAL,  -- π_h
//                              updated_at TEXT NOT NULL)
//
//   hex_neighbors(h3_index TEXT, neighbor_h3_index TEXT)
//
// This file:
//   - installs/updates SQL views to incorporate π_h.
//   - computes a discrete weighted Laplacian penalty over current allocations.
//   - implements a simple ALN-like invariant checker that rejects wasteful plans:
//       invariant: if π_h < PiThreshold and vulnerability_h < VulnThreshold,
//                  then anchor_value_h must not exceed SmoothBudgetFactor * neighbor_mean
//                  (no wasteful high allocation in low-need cells).

struct HexRecord {
    std::string h3_index;
    double anchor_value;
    double ker_e;
    double vulnerability;
    double carbon_potential; // π_h
};

struct LaplacianWeights {
    double w_c;              // weight on ker_e in π_h
    double w_v;              // weight on vulnerability in π_h
    double smooth_weight;    // base weight for Laplacian penalty
};

struct InvariantParams {
    double pi_threshold;          // PiThreshold: minimum π_h to justify high allocation
    double vuln_threshold;        // vulnerability threshold
    double smooth_budget_factor;  // factor times neighbor mean allowed in low π_h cells
};

class SqlCarbonPotentialUpdater {
public:
    explicit SqlCarbonPotentialUpdater(const std::string& db_path)
        : db_path_(db_path) {}

    void installViews() const {
        sqlite3* db = nullptr;
        int rc = sqlite3_open(db_path_.c_str(), &db);
        if (rc != SQLITE_OK) {
            std::string msg = sqlite3_errmsg(db);
            sqlite3_close(db);
            throw std::runtime_error("Cannot open DB for view install: " + msg);
        }

        const char* sql_add_column =
            "ALTER TABLE hex_restoration_commitment "
            "ADD COLUMN carbon_potential REAL;";
        // Best-effort: ignore error if column already exists.
        char* errmsg = nullptr;
        rc = sqlite3_exec(db, sql_add_column, nullptr, nullptr, &errmsg);
        if (rc != SQLITE_OK) {
            sqlite3_free(errmsg);
        }

        const char* sql_view =
            "CREATE VIEW IF NOT EXISTS hex_restoration_with_pi AS "
            "SELECT h3_index, anchor_value, vulnerability, ker_e, carbon_potential "
            "FROM hex_restoration_commitment;";

        rc = sqlite3_exec(db, sql_view, nullptr, nullptr, &errmsg);
        if (rc != SQLITE_OK) {
            std::string msg = errmsg ? errmsg : "";
            sqlite3_free(errmsg);
            sqlite3_close(db);
            throw std::runtime_error("Failed to create hex_restoration_with_pi view: " + msg);
        }

        sqlite3_close(db);
    }

    void updateCarbonPotential(const LaplacianWeights& lw) const {
        sqlite3* db = nullptr;
        int rc = sqlite3_open(db_path_.c_str(), &db);
        if (rc != SQLITE_OK) {
            std::string msg = sqlite3_errmsg(db);
            sqlite3_close(db);
            throw std::runtime_error("Cannot open DB for π_h update: " + msg);
        }

        char* errmsg = nullptr;
        rc = sqlite3_exec(db, "BEGIN IMMEDIATE TRANSACTION;", nullptr, nullptr, &errmsg);
        if (rc != SQLITE_OK) {
            std::string msg = errmsg ? errmsg : "";
            sqlite3_free(errmsg);
            sqlite3_close(db);
            throw std::runtime_error("BEGIN TRANSACTION failed: " + msg);
        }

        const char* sql_sel =
            "SELECT h3_index, ker_e, vulnerability "
            "FROM hex_restoration_commitment;";

        sqlite3_stmt* stmt_sel = nullptr;
        rc = sqlite3_prepare_v2(db, sql_sel, -1, &stmt_sel, nullptr);
        if (rc != SQLITE_OK) {
            std::string msg = sqlite3_errmsg(db);
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            sqlite3_close(db);
            throw std::runtime_error("Prepare select failed: " + msg);
        }

        const char* sql_upd =
            "UPDATE hex_restoration_commitment "
            "SET carbon_potential = ?, updated_at = datetime('now') "
            "WHERE h3_index = ?;";

        sqlite3_stmt* stmt_upd = nullptr;
        rc = sqlite3_prepare_v2(db, sql_upd, -1, &stmt_upd, nullptr);
        if (rc != SQLITE_OK) {
            std::string msg = sqlite3_errmsg(db);
            sqlite3_finalize(stmt_sel);
            sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
            sqlite3_close(db);
            throw std::runtime_error("Prepare update failed: " + msg);
        }

        while ((rc = sqlite3_step(stmt_sel)) == SQLITE_ROW) {
            const unsigned char* h3 = sqlite3_column_text(stmt_sel, 0);
            double ker_e = sqlite3_column_double(stmt_sel, 1);
            double vuln = sqlite3_column_double(stmt_sel, 2);
            std::string h3_index = h3 ? reinterpret_cast<const char*>(h3) : "";

            double pi = lw.w_c * ker_e + lw.w_v * vuln;

            sqlite3_reset(stmt_upd);
            sqlite3_clear_bindings(stmt_upd);
            rc = sqlite3_bind_double(stmt_upd, 1, pi);
            if (rc != SQLITE_OK) goto bind_error;
            rc = sqlite3_bind_text(stmt_upd, 2, h3_index.c_str(), -1, SQLITE_TRANSIENT);
            if (rc != SQLITE_OK) goto bind_error;

            rc = sqlite3_step(stmt_upd);
            if (rc != SQLITE_DONE) {
                std::string msg = sqlite3_errmsg(db);
                sqlite3_finalize(stmt_sel);
                sqlite3_finalize(stmt_upd);
                sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
                sqlite3_close(db);
                throw std::runtime_error("Update step failed: " + msg);
            }
            continue;

        bind_error:
            {
                std::string msg = sqlite3_errmsg(db);
                sqlite3_finalize(stmt_sel);
                sqlite3_finalize(stmt_upd);
                sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
                sqlite3_close(db);
                throw std::runtime_error("Bind error: " + msg);
            }
        }

        sqlite3_finalize(stmt_sel);
        sqlite3_finalize(stmt_upd);
        rc = sqlite3_exec(db, "COMMIT;", nullptr, nullptr, &errmsg);
        if (rc != SQLITE_OK) {
            std::string msg = errmsg ? errmsg : "";
            sqlite3_free(errmsg);
            sqlite3_close(db);
            throw std::runtime_error("COMMIT failed: " + msg);
        }

        sqlite3_close(db);
    }

private:
    std::string db_path_;
};

class LaplacianPenaltyComputer {
public:
    LaplacianPenaltyComputer(const std::string& db_path,
                             const LaplacianWeights& lw)
        : db_path_(db_path), lw_(lw) {}

    double computePenalty() const {
        sqlite3* db = nullptr;
        int rc = sqlite3_open(db_path_.c_str(), &db);
        if (rc != SQLITE_OK) {
            std::string msg = sqlite3_errmsg(db);
            sqlite3_close(db);
            throw std::runtime_error("Cannot open DB for Laplacian penalty: " + msg);
        }

        // Load hex records.
        const char* sql_hex =
            "SELECT h3_index, anchor_value, ker_e, vulnerability, carbon_potential "
            "FROM hex_restoration_with_pi;";
        sqlite3_stmt* stmt_hex = nullptr;
        rc = sqlite3_prepare_v2(db, sql_hex, -1, &stmt_hex, nullptr);
        if (rc != SQLITE_OK) {
            std::string msg = sqlite3_errmsg(db);
            sqlite3_close(db);
            throw std::runtime_error("Prepare hex select failed: " + msg);
        }

        std::unordered_map<std::string, HexRecord> hexes;
        while ((rc = sqlite3_step(stmt_hex)) == SQLITE_ROW) {
            HexRecord h{};
            const unsigned char* h3 = sqlite3_column_text(stmt_hex, 0);
            h.h3_index = h3 ? reinterpret_cast<const char*>(h3) : "";
            h.anchor_value = sqlite3_column_double(stmt_hex, 1);
            h.ker_e = sqlite3_column_double(stmt_hex, 2);
            h.vulnerability = sqlite3_column_double(stmt_hex, 3);
            h.carbon_potential = sqlite3_column_double(stmt_hex, 4);
            hexes[h.h3_index] = h;
        }
        sqlite3_finalize(stmt_hex);

        // Load neighbor edges.
        const char* sql_nb =
            "SELECT h3_index, neighbor_h3_index "
            "FROM hex_neighbors;";
        sqlite3_stmt* stmt_nb = nullptr;
        rc = sqlite3_prepare_v2(db, sql_nb, -1, &stmt_nb, nullptr);
        if (rc != SQLITE_OK) {
            std::string msg = sqlite3_errmsg(db);
            sqlite3_close(db);
            throw std::runtime_error("Prepare neighbor select failed: " + msg);
        }

        double penalty = 0.0;
        while ((rc = sqlite3_step(stmt_nb)) == SQLITE_ROW) {
            const unsigned char* h3_a = sqlite3_column_text(stmt_nb, 0);
            const unsigned char* h3_b = sqlite3_column_text(stmt_nb, 1);
            std::string a = h3_a ? reinterpret_cast<const char*>(h3_a) : "";
            std::string b = h3_b ? reinterpret_cast<const char*>(h3_b) : "";

            auto it_a = hexes.find(a);
            auto it_b = hexes.find(b);
            if (it_a == hexes.end() || it_b == hexes.end()) continue;

            const HexRecord& ha = it_a->second;
            const HexRecord& hb = it_b->second;

            double diff = ha.anchor_value - hb.anchor_value;

            // Weighted edge: emphasize smoothness where both π_h are high,
            // reduce smoothing pressure where π_h is low (to avoid wasting budget).
            double pi_mean = 0.5 * (ha.carbon_potential + hb.carbon_potential);
            double w_edge = lw_.smooth_weight * (1.0 + pi_mean);

            penalty += 0.5 * w_edge * diff * diff;
        }
        sqlite3_finalize(stmt_nb);
        sqlite3_close(db);
        return penalty;
    }

private:
    std::string db_path_;
    LaplacianWeights lw_;
};

// ALN v2 invariant formalization and checker.
// Invariant (conceptual):
//   For all hex h,
//     if carbon_potential_h < pi_threshold AND vulnerability_h < vuln_threshold,
//       then anchor_value_h <= smooth_budget_factor * mean(anchor_value_neighbors)
//
// This prevents wasteful high allocations in low-carbon-potential / low-vulnerability cells.
class ALNInvariantChecker {
public:
    ALNInvariantChecker(const std::string& db_path,
                        const InvariantParams& ip)
        : db_path_(db_path), ip_(ip) {}

    bool check() const {
        sqlite3* db = nullptr;
        int rc = sqlite3_open(db_path_.c_str(), &db);
        if (rc != SQLITE_OK) {
            std::string msg = sqlite3_errmsg(db);
            sqlite3_close(db);
            throw std::runtime_error("Cannot open DB for invariant check: " + msg);
        }

        // Load hex records.
        const char* sql_hex =
            "SELECT h3_index, anchor_value, vulnerability, carbon_potential "
            "FROM hex_restoration_with_pi;";
        sqlite3_stmt* stmt_hex = nullptr;
        rc = sqlite3_prepare_v2(db, sql_hex, -1, &stmt_hex, nullptr);
        if (rc != SQLITE_OK) {
            std::string msg = sqlite3_errmsg(db);
            sqlite3_close(db);
            throw std::runtime_error("Prepare hex select failed: " + msg);
        }

        std::unordered_map<std::string, HexRecord> hexes;
        while ((rc = sqlite3_step(stmt_hex)) == SQLITE_ROW) {
            HexRecord h{};
            const unsigned char* h3 = sqlite3_column_text(stmt_hex, 0);
            h.h3_index = h3 ? reinterpret_cast<const char*>(h3) : "";
            h.anchor_value = sqlite3_column_double(stmt_hex, 1);
            h.vulnerability = sqlite3_column_double(stmt_hex, 2);
            h.carbon_potential = sqlite3_column_double(stmt_hex, 3);
            hexes[h.h3_index] = h;
        }
        sqlite3_finalize(stmt_hex);

        // Load neighbors.
        const char* sql_nb =
            "SELECT h3_index, neighbor_h3_index "
            "FROM hex_neighbors;";
        sqlite3_stmt* stmt_nb = nullptr;
        rc = sqlite3_prepare_v2(db, sql_nb, -1, &stmt_nb, nullptr);
        if (rc != SQLITE_OK) {
            std::string msg = sqlite3_errmsg(db);
            sqlite3_close(db);
            throw std::runtime_error("Prepare neighbor select failed: " + msg);
        }

        std::unordered_map<std::string, std::vector<std::string>> neighbors;
        while ((rc = sqlite3_step(stmt_nb)) == SQLITE_ROW) {
            const unsigned char* h3_a = sqlite3_column_text(stmt_nb, 0);
            const unsigned char* h3_b = sqlite3_column_text(stmt_nb, 1);
            std::string a = h3_a ? reinterpret_cast<const char*>(h3_a) : "";
            std::string b = h3_b ? reinterpret_cast<const char*>(h3_b) : "";
            neighbors[a].push_back(b);
        }
        sqlite3_finalize(stmt_nb);
        sqlite3_close(db);

        // Check invariant.
        for (const auto& kv : hexes) {
            const std::string& h3 = kv.first;
            const HexRecord& h = kv.second;

            if (h.carbon_potential >= ip_.pi_threshold ||
                h.vulnerability >= ip_.vuln_threshold) {
                continue; // invariant applies only to low π_h and low vulnerability cells
            }

            auto it_nb = neighbors.find(h3);
            if (it_nb == neighbors.end() || it_nb->second.empty()) {
                continue; // no neighbors, skip
            }

            double sum_neighbor_anchor = 0.0;
            std::size_t count = 0;
            for (const auto& nb_h3 : it_nb->second) {
                auto it = hexes.find(nb_h3);
                if (it == hexes.end()) continue;
                sum_neighbor_anchor += it->second.anchor_value;
                count++;
            }
            if (count == 0) continue;
            double mean_neighbor_anchor = sum_neighbor_anchor / static_cast<double>(count);

            double allowed_max = ip_.smooth_budget_factor * mean_neighbor_anchor;
            if (h.anchor_value > allowed_max + 1e-9) {
                std::cerr << "[ALN invariant] violation at " << h3
                          << " anchor_value=" << h.anchor_value
                          << " allowed_max=" << allowed_max
                          << " π_h=" << h.carbon_potential
                          << " vuln=" << h.vulnerability << std::endl;
                return false;
            }
        }

        return true;
    }

private:
    std::string db_path_;
    InvariantParams ip_;
};

} // namespace eco_restoration
} // namespace prometheus_praxis

int main(int argc, char* argv[]) {
    using namespace prometheus_praxis::eco_restoration;

    std::string db_path = "hex_restoration.db";
    if (argc > 1) {
        db_path = argv[1];
    }

    LaplacianWeights lw;
    lw.w_c = 0.7;
    lw.w_v = 0.3;
    lw.smooth_weight = 1.0;

    InvariantParams ip;
    ip.pi_threshold = 0.4;          // example PiThreshold
    ip.vuln_threshold = 0.3;        // low vulnerability threshold
    ip.smooth_budget_factor = 1.2;  // allow up to 20% above neighbor mean

    try {
        SqlCarbonPotentialUpdater updater(db_path);
        updater.installViews();
        updater.updateCarbonPotential(lw);

        LaplacianPenaltyComputer penalty_comp(db_path, lw);
        double lap_penalty = penalty_comp.computePenalty();
        std::cout << "Current weighted Laplacian penalty: " << lap_penalty << std::endl;

        ALNInvariantChecker checker(db_path, ip);
        bool ok = checker.check();
        if (!ok) {
            std::cerr << "ALN v2 invariant violated: plan is wasteful; reject." << std::endl;
            return 2;
        } else {
            std::cout << "ALN v2 invariant satisfied: plan is non-wasteful." << std::endl;
        }
    } catch (const std::exception& ex) {
        std::cerr << "Laplacian carbon-negative potential error: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
