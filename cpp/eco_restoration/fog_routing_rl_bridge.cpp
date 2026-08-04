// File: cpp/eco_restoration/fog_routing_rl_bridge.cpp

#include <iostream>
#include <string>
#include <stdexcept>
#include <sqlite3.h>

namespace prometheus_praxis {
namespace eco_restoration {

struct FOGState {
    double dissolved_oxygen_mgL;
    double pfas_ugL;
    double flow_m3h;
    double ker_e;
    double ker_r;
    int router_mode;
};

struct RLDecision {
    int action_id;
    double confidence;
};

struct SafetyLimits {
    double pfas_safe_thr;
    double bod_safe_thr;
    double lyap_max_increase;
};

struct RoutingProbabilities {
    double route_to_a;
    double route_to_b;
    double route_to_treatment;
};

struct LuaRoutingDecision {
    std::string chosen_route;
};

class FOGRoutingRLBridge {
public:
    explicit FOGRoutingRLBridge(const std::string& db_path)
        : db_path_(db_path), db_(nullptr),
          confidence_threshold_(0.7), pfas_high_threshold_(0.0) {
        open_db();
        install_schema();
    }

    FOGRoutingRLBridge(sqlite3* db, double confidence_threshold)
        : db_path_(), db_(db),
          confidence_threshold_(confidence_threshold), pfas_high_threshold_(0.0) {
        if (!db_) {
            throw std::runtime_error("SQLite DB pointer must not be null");
        }
        install_schema();
    }

    ~FOGRoutingRLBridge() {
        if (db_) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
    }

    int select_action(const FOGState& state) {
        SafetyLimits limits = load_safety_limits();
        RLDecision rl_decision = query_rl_policy(state);
        int lua_action = query_lua_fallback(state);

        if (!is_state_safe(state, limits)) {
            store_decision_event(state, rl_decision, lua_action, lua_action);
            return lua_action;
        }

        if (rl_decision.confidence < confidence_threshold_) {
            store_decision_event(state, rl_decision, lua_action, lua_action);
            return lua_action;
        }
        if (!respects_lyapunov(state, rl_decision.action_id, limits)) {
            store_decision_event(state, rl_decision, lua_action, lua_action);
            return lua_action;
        }

        int blended_action = rl_decision.action_id;
        store_decision_event(state, rl_decision, lua_action, blended_action);
        return blended_action;
    }

    FOGState read_current_state(const std::string& segment_id) {
        const char* sql =
            "SELECT dissolved_oxygen_mgL, pfas_ugL, flow_m3h, ker_e, ker_r, router_mode "
            "FROM fog_segment_state "
            "WHERE segment_id = ? "
            "ORDER BY ts_utc DESC "
            "LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            sqlite3_finalize(stmt);
            throw std::runtime_error("Failed to prepare fog_segment_state query");
        }

        rc = sqlite3_bind_text(stmt, 1, segment_id.c_str(), -1, SQLITE_TRANSIENT);
        if (rc != SQLITE_OK) {
            sqlite3_finalize(stmt);
            throw std::runtime_error("Failed to bind segment_id");
        }

        FOGState s{};
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            s.dissolved_oxygen_mgL = sqlite3_column_double(stmt, 0);
            s.pfas_ugL             = sqlite3_column_double(stmt, 1);
            s.flow_m3h             = sqlite3_column_double(stmt, 2);
            s.ker_e                = sqlite3_column_double(stmt, 3);
            s.ker_r                = sqlite3_column_double(stmt, 4);
            s.router_mode          = sqlite3_column_int(stmt, 5);
        }
        sqlite3_finalize(stmt);
        return s;
    }

    std::string blend_decision(const FOGState& state,
                               const RoutingProbabilities& rl_prob,
                               double rl_confidence,
                               const LuaRoutingDecision& lua_decision) {
        if (rl_confidence < confidence_threshold_) {
            return lua_decision.chosen_route;
        }

        std::string rl_choice = argmax_route(rl_prob);

        if (state.pfas_ugL > pfas_high_threshold_) {
            return "TREATMENT";
        }

        if (rl_choice == "TREATMENT") {
            return rl_choice;
        }

        if (lua_decision.chosen_route == "TREATMENT") {
            return lua_decision.chosen_route;
        }

        return rl_choice;
    }

private:
    std::string db_path_;
    sqlite3* db_;
    double confidence_threshold_;
    double pfas_high_threshold_;

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
        const char* sql_limits =
            "CREATE TABLE IF NOT EXISTS fog_rl_safety_limits ("
            "  id INTEGER PRIMARY KEY,"
            "  pfas_safe_thr REAL NOT NULL,"
            "  bod_safe_thr REAL NOT NULL,"
            "  lyap_max_increase REAL NOT NULL,"
            "  updated_at TEXT NOT NULL"
            ");";
        exec_sql(sql_limits);

        const char* sql_decisions =
            "CREATE TABLE IF NOT EXISTS fog_rl_decision_log ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  ts TEXT NOT NULL,"
            "  dissolved_oxygen_mgL REAL NOT NULL,"
            "  pfas_ugL REAL NOT NULL,"
            "  flow_m3h REAL NOT NULL,"
            "  ker_e REAL NOT NULL,"
            "  ker_r REAL NOT NULL,"
            "  router_mode INTEGER NOT NULL,"
            "  rl_action_id INTEGER NOT NULL,"
            "  rl_confidence REAL NOT NULL,"
            "  lua_action_id INTEGER NOT NULL,"
            "  blended_action_id INTEGER NOT NULL"
            ");";
        exec_sql(sql_decisions);
    }

    SafetyLimits load_safety_limits() {
        const char* sql =
            "SELECT pfas_safe_thr, bod_safe_thr, lyap_max_increase "
            "FROM fog_rl_safety_limits ORDER BY updated_at DESC LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        SafetyLimits limits{0.1, 5.0, 0.0};
        if (rc == SQLITE_OK) {
            rc = sqlite3_step(stmt);
            if (rc == SQLITE_ROW) {
                limits.pfas_safe_thr     = sqlite3_column_double(stmt, 0);
                limits.bod_safe_thr      = sqlite3_column_double(stmt, 1);
                limits.lyap_max_increase = sqlite3_column_double(stmt, 2);
            }
        }
        sqlite3_finalize(stmt);
        return limits;
    }

    RLDecision query_rl_policy(const FOGState& state) {
        const char* sql =
            "SELECT action_id, confidence "
            "FROM fog_rl_policy_state_action "
            "WHERE router_mode = ? "
            "ORDER BY confidence DESC LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        RLDecision dec{0, 0.0};
        if (rc != SQLITE_OK) {
            sqlite3_finalize(stmt);
            return dec;
        }
        rc = sqlite3_bind_int(stmt, 1, state.router_mode);
        if (rc != SQLITE_OK) {
            sqlite3_finalize(stmt);
            return dec;
        }
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            dec.action_id  = sqlite3_column_int(stmt, 0);
            dec.confidence = sqlite3_column_double(stmt, 1);
        }
        sqlite3_finalize(stmt);
        return dec;
    }

    int query_lua_fallback(const FOGState& state) {
        const char* sql =
            "SELECT action_id "
            "FROM fog_lua_router_rules "
            "WHERE router_mode = ? "
            "ORDER BY rule_priority ASC LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        int action_id = 0;
        if (rc != SQLITE_OK) {
            sqlite3_finalize(stmt);
            return action_id;
        }
        rc = sqlite3_bind_int(stmt, 1, state.router_mode);
        if (rc != SQLITE_OK) {
            sqlite3_finalize(stmt);
            return action_id;
        }
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            action_id = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
        return action_id;
    }

    bool is_state_safe(const FOGState& state, const SafetyLimits& limits) const {
        if (state.pfas_ugL > limits.pfas_safe_thr) return false;
        if (state.dissolved_oxygen_mgL < limits.bod_safe_thr) return false;
        return true;
    }

    bool respects_lyapunov(const FOGState& state,
                           int action_id,
                           const SafetyLimits& limits) const {
        double delta_lyap = 0.0;
        if (action_id == 2) {
            delta_lyap = 0.1;
        }
        return (delta_lyap <= limits.lyap_max_increase + 1e-9);
    }

    void store_decision_event(const FOGState& state,
                              const RLDecision& rl_decision,
                              int lua_action,
                              int blended_action) {
        const char* sql =
            "INSERT INTO fog_rl_decision_log("
            "  ts, dissolved_oxygen_mgL, pfas_ugL, flow_m3h, ker_e, ker_r, router_mode,"
            "  rl_action_id, rl_confidence, lua_action_id, blended_action_id"
            ") VALUES(datetime('now'), ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
        sqlite3_stmt* stmt = nullptr;
        int rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            sqlite3_finalize(stmt);
            return;
        }
        sqlite3_bind_double(stmt, 1, state.dissolved_oxygen_mgL);
        sqlite3_bind_double(stmt, 2, state.pfas_ugL);
        sqlite3_bind_double(stmt, 3, state.flow_m3h);
        sqlite3_bind_double(stmt, 4, state.ker_e);
        sqlite3_bind_double(stmt, 5, state.ker_r);
        sqlite3_bind_int(stmt, 6, state.router_mode);
        sqlite3_bind_int(stmt, 7, rl_decision.action_id);
        sqlite3_bind_double(stmt, 8, rl_decision.confidence);
        sqlite3_bind_int(stmt, 9, lua_action);
        sqlite3_bind_int(stmt, 10, blended_action);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    std::string argmax_route(const RoutingProbabilities& prob) const {
        double max_val = prob.route_to_a;
        std::string choice = "A";
        if (prob.route_to_b > max_val) {
            max_val = prob.route_to_b;
            choice = "B";
        }
        if (prob.route_to_treatment > max_val) {
            max_val = prob.route_to_treatment;
            choice = "TREATMENT";
        }
        return choice;
    }
};

} // namespace eco_restoration
} // namespace prometheus_praxis

int main(int argc, char* argv[]) {
    using namespace prometheus_praxis::eco_restoration;

    std::string db_path = "fog_routing_rl.db";
    if (argc > 1) {
        db_path = argv[1];
    }

    try {
        FOGRoutingRLBridge bridge(db_path);

        FOGState state;
        state.dissolved_oxygen_mgL = 7.0;
        state.pfas_ugL = 0.05;
        state.flow_m3h = 100.0;
        state.ker_e = 0.8;
        state.ker_r = 0.6;
        state.router_mode = 0;

        int action = bridge.select_action(state);
        std::cout << "Blended FOG routing action_id=" << action << std::endl;
    } catch (const std::exception& ex) {
        std::cerr << "FOG routing RL bridge error: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
