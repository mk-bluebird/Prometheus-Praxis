// filename: src/cpp/cyboquatic_guard/laneguard.cpp
// repo: mk-bluebird/eco_restoration_shard

#include "laneguard.hpp"

namespace cyboquatic_guard {

LaneGuard::LaneGuard(const LaneGuardConfig& config)
    : config_(config) {}

LaneGuardResult LaneGuard::check_lane_admissible(const LaneContext& ctx) const {
    LaneGuardResult result;
    result.admissible = true;
    result.reason.clear();

    if (ctx.k < config_.min_k) {
        result.admissible = false;
        result.reason = "K score below minimum";
        return result;
    }
    if (ctx.e < config_.min_e) {
        result.admissible = false;
        result.reason = "E score below minimum";
        return result;
    }
    if (ctx.r > config_.max_r) {
        result.admissible = false;
        result.reason = "R score above maximum";
        return result;
    }
    if (ctx.roh > config_.max_roh) {
        result.admissible = false;
        result.reason = "RoH above ceiling";
        return result;
    }
    if (!ctx.carbon_negative_ok) {
        result.admissible = false;
        result.reason = "Carbon-negative condition not satisfied";
        return result;
    }
    if (!ctx.restoration_ok) {
        result.admissible = false;
        result.reason = "Restoration condition not satisfied";
        return result;
    }
    if (!ctx.blast_safe) {
        result.admissible = false;
        result.reason = "Blast radius constraints not satisfied";
        return result;
    }

    result.reason = "Lane admissible";
    return result;
}

} // namespace cyboquatic_guard
