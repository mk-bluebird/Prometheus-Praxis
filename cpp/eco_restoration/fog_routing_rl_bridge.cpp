// File: cpp/eco_restoration/fog_routing_rl_bridge.cpp

#include <vector>
#include <string>
#include <stdexcept>
#include <sqlite3.h>

// Simple struct to hold state features for RL
struct FOGState {
    double dissolved_oxygen;
    double pfas_conc;
    double flow_rate;
    double temperature;
    double ker_e;
    double ker_r;
};

// Routing action probabilities from RL policy
struct RoutingProbabilities {
    double route_to_a;
    double route_to_b;
    double route_to_treatment;
};

// Lua fallback decision result
struct LuaRoutingDecision {
    std::string chosen_route;
};

class FOGRoutingRLBridge {
public:
    FOGRoutingRLBridge(sqlite3* db,
                       double confidence_threshold)
        : db_(db),
          confidence_threshold_(confidence_threshold) {
        if (!db_) {
            throw std::runtime_error("SQLite DB pointer must not be null");
        }
    }

    FOGState readCurrentState(const std::string& segment_id) {
        const char* sql =
            "SELECT dissolved_oxygen, pfas_conc, flow_rate, temperature, ker_e, ker_r "
            "FROM fog_segment_state "
            "WHERE segment_id = ? "
            "ORDER BY ts_utc DESC "
            "LIMIT 1;";

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare fog_segment_state query");
        }

        sqlite3_bind_text(stmt, 1, segment_id.c_str(), -1, SQLITE_TRANSIENT);

        FOGState s{};
        int rc = sqlite3_step(stmt);
        if (rc == SQLITE_ROW) {
            s.dissolved_oxygen = sqlite3_column_double(stmt, 0);
            s.pfas_conc        = sqlite3_column_double(stmt, 1);
            s.flow_rate        = sqlite3_column_double(stmt, 2);
            s.temperature      = sqlite3_column_double(stmt, 3);
            s.ker_e            = sqlite3_column_double(stmt, 4);
            s.ker_r            = sqlite3_column_double(stmt, 5);
        } else {
            s = FOGState{};
        }

        sqlite3_finalize(stmt);
        return s;
    }

    // Called from Kotlin/DL4J: pass in RL policy probabilities and confidence score.
    std::string blendDecision(const FOGState& state,
                              const RoutingProbabilities& rl_prob,
                              double rl_confidence,
                              const LuaRoutingDecision& lua_decision) {
        if (rl_confidence < confidence_threshold_) {
            // Fall back to Lua router decision
            return lua_decision.chosen_route;
        }

        // Confidence-weighted blending: choose route with highest RL probability,
        // but veto if it conflicts with Lua in high-risk PFAS conditions.
        std::string rl_choice = argmaxRoute(rl_prob);
        if (state.pfas_conc > pfas_high_threshold_) {
            // Prefer treatment route under high PFAS
            return "TREATMENT";
        }

        // If RL choice is treatment, keep it; otherwise ensure Lua route is not unsafe.
        if (rl_choice == "TREATMENT") {
            return rl_choice;
        }

        // Example: if Lua chooses treatment but RL is unsure, keep treatment.
        if (lua_decision.chosen_route == "TREATMENT") {
            return lua_decision.chosen_route;
        }

        return rl_choice;
    }

private:
    sqlite3* db_;
    double   confidence_threshold_;
    double   pfas_high_threshold_ = 0.0; // set from governance corridor elsewhere

    std::string argmaxRoute(const RoutingProbabilities& prob) const {
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
