// filename: econet_mcp/src/mt6883_reward_bridge.cpp
// destination: EcoNet/econet_mcp/src/mt6883_reward_bridge.cpp
// purpose: non-actuating C++ bridge that reads EcoNet governance spine + reward index
//          and exposes a pure function for MCP servers to compute per-window reward scores.

#include <cstdint>
#include <vector>
#include <string>
#include <optional>
#include <stdexcept>
#include <sqlite3.h>

struct RewardPlaneUplift {
    std::string planecode;
    bool        nonoffsettable;
    double      r_start;
    double      r_end;
    double      delta_karma;
    double      delta_mass_kg;
    bool        plane_ok;
};

struct RewardWindowScore {
    std::int64_t reward_window_id;
    std::string  nodeid;
    std::string  region;
    std::string  family_code;
    double       k_avg;
    double       e_avg;
    double       r_avg;
    double       roh_est;
    double       aggregate_score;      // MCP-facing scalar, biocompatible reward index
    std::vector<RewardPlaneUplift> planes;
};

namespace econet_mcp {

class SqliteHandle {
public:
    explicit SqliteHandle(const std::string &path) {
        if (sqlite3_open_v2(path.c_str(), &db_, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
            throw std::runtime_error("Failed to open SQLite DB at " + path);
        }
    }

    ~SqliteHandle() {
        if (db_ != nullptr) {
            sqlite3_close(db_);
        }
    }

    sqlite3 *get() { return db_; }

private:
    sqlite3 *db_ = nullptr;
};

// Utility to prepare and finalize statements safely.
class Stmt {
public:
    Stmt(sqlite3 *db, const std::string &sql) {
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt_, nullptr) != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare SQL: " + sql);
        }
    }

    ~Stmt() {
        if (stmt_ != nullptr) {
            sqlite3_finalize(stmt_);
        }
    }

    sqlite3_stmt *get() { return stmt_; }

private:
    sqlite3_stmt *stmt_ = nullptr;
};

// Compute a scalar reward index from plane uplifts with KER-aligned semantics:
// - Nonoffsettable planes must have non-worsening risk (plane_ok true) or they zero rewards.
// - Score is sum(delta_karma) with soft boosting for positive delta_mass_kg.
// - This is read-only and non-actuating.
inline double compute_aggregate_reward_score(const std::vector<RewardPlaneUplift> &planes) {
    bool any_nonoffset_violation = false;
    double score = 0.0;

    for (const auto &p : planes) {
        if (p.nonoffsettable && !p.plane_ok) {
            any_nonoffset_violation = true;
        }
    }

    if (any_nonoffset_violation) {
        return 0.0;
    }

    for (const auto &p : planes) {
        double base = p.delta_karma;
        double mass_bonus = 0.0;
        if (p.delta_mass_kg > 0.0) {
            mass_bonus = 0.1 * p.delta_mass_kg;
        }
        score += base + mass_bonus;
    }

    if (score < 0.0) {
        score = 0.0;
    }

    return score;
}

// Load plane uplifts for a given reward_window_id.
inline std::vector<RewardPlaneUplift> load_plane_uplifts(sqlite3 *db, std::int64_t reward_window_id) {
    std::vector<RewardPlaneUplift> out;
    const std::string sql =
        "SELECT planecode, nonoffsettable, r_start, r_end, delta_karma, delta_mass_kg, plane_ok "
        "FROM v_reward_plane_uplift WHERE reward_window_id = ?1";

    Stmt stmt(db, sql);
    sqlite3_bind_int64(stmt.get(), 1, reward_window_id);

    while (true) {
        int rc = sqlite3_step(stmt.get());
        if (rc == SQLITE_ROW) {
            RewardPlaneUplift p;
            p.planecode = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 0));
            p.nonoffsettable = sqlite3_column_int(stmt.get(), 1) != 0;
            p.r_start = sqlite3_column_double(stmt.get(), 2);
            p.r_end = sqlite3_column_double(stmt.get(), 3);
            p.delta_karma = sqlite3_column_double(stmt.get(), 4);
            p.delta_mass_kg = sqlite3_column_double(stmt.get(), 5);
            p.plane_ok = sqlite3_column_int(stmt.get(), 6) != 0;
            out.push_back(p);
        } else if (rc == SQLITE_DONE) {
            break;
        } else {
            throw std::runtime_error("Error stepping v_reward_plane_uplift");
        }
    }

    return out;
}

// Public API: compute reward scores for all eligible windows in a region.
// MCP servers can call this in-process and expose results as MCP "resources".
inline std::vector<RewardWindowScore> compute_region_reward_scores(
    const std::string &db_path,
    const std::string &region_filter
) {
    SqliteHandle handle(db_path);
    sqlite3 *db = handle.get();

    const std::string sql =
        "SELECT reward_window_id, nodeid, region, family_code, k_avg, e_avg, r_avg, roh_est "
        "FROM v_reward_eligible_windows WHERE region = ?1";

    Stmt stmt(db, sql);
    sqlite3_bind_text(stmt.get(), 1, region_filter.c_str(), -1, SQLITE_TRANSIENT);

    std::vector<RewardWindowScore> results;

    while (true) {
        int rc = sqlite3_step(stmt.get());
        if (rc == SQLITE_ROW) {
            RewardWindowScore w;
            w.reward_window_id = sqlite3_column_int64(stmt.get(), 0);
            w.nodeid           = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 1));
            w.region           = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 2));
            w.family_code      = reinterpret_cast<const char*>(sqlite3_column_text(stmt.get(), 3));
            w.k_avg            = sqlite3_column_double(stmt.get(), 4);
            w.e_avg            = sqlite3_column_double(stmt.get(), 5);
            w.r_avg            = sqlite3_column_double(stmt.get(), 6);
            w.roh_est          = sqlite3_column_double(stmt.get(), 7);

            w.planes = load_plane_uplifts(db, w.reward_window_id);
            w.aggregate_score = compute_aggregate_reward_score(w.planes);

            results.push_back(std::move(w));
        } else if (rc == SQLITE_DONE) {
            break;
        } else {
            throw std::runtime_error("Error stepping v_reward_eligible_windows");
        }
    }

    return results;
}

} // namespace econet_mcp
