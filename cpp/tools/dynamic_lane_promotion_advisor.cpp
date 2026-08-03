// File: cpp/tools/dynamic_lane_promotion_advisor.cpp
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>

// This tool reads bandit-style governance state for modules and recommends
// lane promotions (e.g., RESEARCH -> EXPPROD -> PROD). It writes recommendations
// into module_lane_history but does not auto-execute lane changes, preserving
// human-in-the-loop governance.

namespace eco {

enum class Lane {
    RESEARCH,
    EXPPROD,
    PROD
};

std::string lane_to_string(Lane lane) {
    switch (lane) {
        case Lane::RESEARCH: return "RESEARCH";
        case Lane::EXPPROD:  return "EXPPROD";
        case Lane::PROD:     return "PROD";
    }
    return "UNKNOWN";
}

struct ModuleBanditState {
    std::string module_id;
    Lane current_lane;
    double mean_reward;    // e.g., eco impact or KER-related reward
    double uncertainty;    // bandit confidence interval width
    double risk_score;     // lower is safer
};

struct LaneRecommendation {
    std::string module_id;
    Lane from_lane;
    Lane to_lane;
    std::string reason;
};

Lane next_lane(Lane lane) {
    if (lane == Lane::RESEARCH) return Lane::EXPPROD;
    if (lane == Lane::EXPPROD)  return Lane::PROD;
    return Lane::PROD;
}

// Decide if a module is a candidate for lane promotion based on bandit metrics.
bool should_promote(const ModuleBanditState& s, Lane& to_lane, std::string& reason) {
    // Simple UCB-style condition: high mean_reward, low uncertainty, low risk.
    double ucb = s.mean_reward + 0.5 * s.uncertainty;

    if (s.current_lane == Lane::PROD) {
        reason = "Already in PROD lane.";
        return false;
    }

    if (ucb < 0.5) {
        reason = "Insufficient expected reward (UCB below threshold).";
        return false;
    }
    if (s.risk_score > 0.3) {
        reason = "Risk score too high for promotion.";
        return false;
    }

    to_lane = next_lane(s.current_lane);
    reason = "Bandit metrics support lane promotion (high reward, low uncertainty, low risk).";
    return true;
}

// Build lane recommendations from module_bandit_state snapshot.
std::vector<LaneRecommendation> build_recommendations(
        const std::vector<ModuleBanditState>& states) {
    std::vector<LaneRecommendation> recs;
    for (const auto& s : states) {
        Lane to_lane;
        std::string reason;
        if (should_promote(s, to_lane, reason)) {
            LaneRecommendation r{};
            r.module_id = s.module_id;
            r.from_lane = s.current_lane;
            r.to_lane = to_lane;
            r.reason = reason;
            recs.push_back(r);
        }
    }
    return recs;
}

// Emit SQL for module_lane_history (recommendations only, no auto-execution).
void emit_lane_history_sql(const std::vector<LaneRecommendation>& recs,
                           const std::string& advisory_run_id) {
    for (const auto& r : recs) {
        std::cout << "INSERT INTO module_lane_history "
                  << "(advisory_run_id, module_id, from_lane, to_lane, reason, ts) VALUES ('"
                  << advisory_run_id << "', '"
                  << r.module_id << "', '"
                  << lane_to_string(r.from_lane) << "', '"
                  << lane_to_string(r.to_lane) << "', '"
                  << r.reason << "', "
                  << "CURRENT_TIMESTAMP);\n";
    }
}

} // namespace eco

int main() {
    using namespace eco;

    // Example module_bandit_state snapshot.
    std::vector<ModuleBanditState> states = {
        {"module_A", Lane::RESEARCH, 0.6, 0.1, 0.2},
        {"module_B", Lane::EXPPROD, 0.7, 0.15, 0.25},
        {"module_C", Lane::RESEARCH, 0.4, 0.2, 0.1},
        {"module_D", Lane::PROD,     0.8, 0.05, 0.2}
    };

    auto recs = build_recommendations(states);

    std::string advisory_run_id = "lane_advisor_2026_08_03";
    std::cout << "Dynamic lane promotion advisor recommendations:\n";
    for (const auto& r : recs) {
        std::cout << "  module " << r.module_id << " : "
                  << lane_to_string(r.from_lane) << " -> "
                  << lane_to_string(r.to_lane)
                  << " reason=" << r.reason << "\n";
    }
    std::cout << "\n";

    emit_lane_history_sql(recs, advisory_run_id);

    return 0;
}
