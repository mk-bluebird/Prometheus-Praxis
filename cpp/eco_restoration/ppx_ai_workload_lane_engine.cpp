// File: cpp/eco_restoration/ppx_ai_workload_lane_engine.cpp
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

namespace ppx::eco_restoration {

enum class TelemetryDomain { WastewaterPump, Shredder, Hammermill, AiWorkload, Unknown };
enum class ActionLane { Research, Pilot, Production };
enum class LaneAction { Proceed, Derate, Halt };

struct MachineTelemetry {
    std::string machine_id;
    std::string station_id;
    TelemetryDomain domain{TelemetryDomain::Unknown};
    std::string timestamp_utc;
    double r_hydraulics{};
    double r_energy{};
    double r_uncertainty{};
    double r_reliability{};
    double r_extra_1{};
    double r_extra_2{};
    bool has_extra_1{};
    bool has_extra_2{};
    double risk_of_harm{};
    double vt_current{};
    double vt_next{};
};

struct KerCoordinates {
    double knowledge{};
    double eco_impact{};
    double risk{};
};

struct LaneConfig {
    ActionLane lane{ActionLane::Research};
    double risk_of_harm_ceiling{0.25};
    double max_delta_vt{0.02};
    double knowledge_min_research{0.50};
    double knowledge_min_pilot{0.65};
    double knowledge_min_production{0.80};
    double impact_min_research{0.50};
    double impact_min_pilot{0.65};
    double impact_min_production{0.80};
    double risk_max_research{0.35};
    double risk_max_pilot{0.25};
    double risk_max_production{0.15};
};

struct GovernanceGateConfig {
    bool corridor_available{false};
    bool allow_research_exploration{false};
};

struct LaneDecision {
    ActionLane lane{};
    LaneAction action{};
    std::string reason_code;
    double delta_vt{};
    double risk_of_harm{};
    KerCoordinates ker{};
};

class PpxAiWorkloadLaneEngine {
public:
    [[nodiscard]] LaneDecision decide(
        const MachineTelemetry& telemetry,
        const KerCoordinates& ker,
        const LaneConfig& config,
        const GovernanceGateConfig& gates) const {
        validate(telemetry, ker, config);
        const double delta = telemetry.vt_next - telemetry.vt_current;

        if (!gates.corridor_available) {
            return make(config.lane, LaneAction::Halt, "no_corridor_no_build", delta, telemetry, ker);
        }
        if (telemetry.risk_of_harm >= config.risk_of_harm_ceiling) {
            return make(config.lane, LaneAction::Halt, "risk_of_harm_ceiling_reached", delta, telemetry, ker);
        }
        if (delta > config.max_delta_vt) {
            const bool research_exception =
                config.lane == ActionLane::Research && gates.allow_research_exploration;
            return make(config.lane, research_exception ? LaneAction::Derate : LaneAction::Halt,
                        research_exception ? "residual_exceeds_research_band"
                                           : "residual_exceeds_lane_band",
                        delta, telemetry, ker);
        }

        const auto [minimum_knowledge, minimum_impact, maximum_risk] = thresholds(config);
        if (ker.knowledge < minimum_knowledge) {
            return make(config.lane, LaneAction::Derate, "knowledge_below_lane_min", delta, telemetry, ker);
        }
        if (ker.eco_impact < minimum_impact) {
            return make(config.lane, LaneAction::Derate, "eco_impact_below_lane_min", delta, telemetry, ker);
        }
        if (ker.risk > maximum_risk) {
            return make(config.lane, LaneAction::Derate, "risk_above_lane_max", delta, telemetry, ker);
        }
        return make(config.lane, LaneAction::Proceed, "all_governance_gates_passed", delta, telemetry, ker);
    }

private:
    static bool in_unit_interval(double value) {
        return std::isfinite(value) && value >= 0.0 && value <= 1.0;
    }

    static void validate(const MachineTelemetry& t, const KerCoordinates& k, const LaneConfig& c) {
        if (t.machine_id.empty() || t.station_id.empty() || t.timestamp_utc.empty()) {
            throw std::invalid_argument("machine, station, and timestamp are required");
        }
        for (const double value : {t.r_hydraulics, t.r_energy, t.r_uncertainty,
                                   t.r_reliability, t.risk_of_harm, k.knowledge,
                                   k.eco_impact, k.risk, c.risk_of_harm_ceiling}) {
            if (!in_unit_interval(value)) {
                throw std::invalid_argument("normalized KER and risk values must be within [0,1]");
            }
        }
        if ((t.has_extra_1 && !in_unit_interval(t.r_extra_1)) ||
            (t.has_extra_2 && !in_unit_interval(t.r_extra_2)) ||
            !std::isfinite(t.vt_current) || !std::isfinite(t.vt_next) ||
            !std::isfinite(c.max_delta_vt) || c.max_delta_vt < 0.0) {
            throw std::invalid_argument("invalid residual or optional risk coordinate");
        }
    }

    static std::tuple<double, double, double> thresholds(const LaneConfig& c) {
        switch (c.lane) {
            case ActionLane::Research:
                return {c.knowledge_min_research, c.impact_min_research, c.risk_max_research};
            case ActionLane::Pilot:
                return {c.knowledge_min_pilot, c.impact_min_pilot, c.risk_max_pilot};
            case ActionLane::Production:
                return {c.knowledge_min_production, c.impact_min_production, c.risk_max_production};
        }
        throw std::logic_error("unrecognized lane");
    }

    static LaneDecision make(ActionLane lane, LaneAction action, std::string reason,
                             double delta, const MachineTelemetry& telemetry,
                             const KerCoordinates& ker) {
        return {lane, action, std::move(reason), delta, telemetry.risk_of_harm, ker};
    }
};

extern "C" int ppx_ai_workload_decide(
    const char* machine_id, const char* station_id, const char* timestamp_utc,
    double r_hydraulics, double r_energy, double r_uncertainty, double r_reliability,
    double risk_of_harm, double vt_current, double vt_next, double knowledge,
    double eco_impact, double risk, int lane, int corridor_available,
    int allow_research_exploration, int* action, double* delta_vt) {
    try {
        if (action == nullptr || delta_vt == nullptr) return 2;
        const MachineTelemetry telemetry{
            machine_id == nullptr ? "" : machine_id,
            station_id == nullptr ? "" : station_id, TelemetryDomain::AiWorkload,
            timestamp_utc == nullptr ? "" : timestamp_utc, r_hydraulics, r_energy,
            r_uncertainty, r_reliability, 0.0, 0.0, false, false, risk_of_harm,
            vt_current, vt_next
        };
        const LaneConfig config{static_cast<ActionLane>(lane)};
        const LaneDecision decision = PpxAiWorkloadLaneEngine{}.decide(
            telemetry, {knowledge, eco_impact, risk}, config,
            {corridor_available != 0, allow_research_exploration != 0});
        *action = static_cast<int>(decision.action);
        *delta_vt = decision.delta_vt;
        return 0;
    } catch (...) {
        return 1;
    }
}

}  // namespace ppx::eco_restoration
