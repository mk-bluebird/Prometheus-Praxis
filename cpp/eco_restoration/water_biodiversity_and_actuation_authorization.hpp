// File: cpp/eco_restoration/water_biodiversity_and_actuation_authorization.hpp
#pragma once

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace eco_restoration {

/*
Fixed-point sorts:
- Water allocation uses nonnegative int64 millilitres over a declared interval.
- Biodiversity quality uses int32 units scaled by 1,000,000 over [0,1,000,000].
- RoH uses int32 units scaled by 1,000,000 over [0,1,000,000].

Required cross-shard invariant:
SAT(WaterCompliant and BiodiversityViolation and Allow)=unsat.
Allow is therefore constructed only from water compliance and biodiversity
compliance; a biodiversity violation cannot be accompanied by Allow.
*/
constexpr std::int32_t unit_interval_scale = 1'000'000;
constexpr std::int32_t risk_of_harm_limit_fixed = 300'000;

struct WaterAllocation {
    std::int64_t allocated_ml{};
    std::int64_t permitted_ml{};
    std::int64_t ecological_reserve_ml{};
};

struct BiodiversityIndex {
    std::int32_t quality_fixed{};
    std::int32_t required_minimum_fixed{};
};

struct CrossShardDecision {
    bool water_compliant{};
    bool biodiversity_compliant{};
    bool biodiversity_violation{};
    bool allow{};
    double knowledge_factor{};
    double eco_impact_value{};
};

inline CrossShardDecision evaluate_water_biodiversity(
    const WaterAllocation& water, const BiodiversityIndex& biodiversity) {

    if (water.allocated_ml < 0 || water.permitted_ml < 0 ||
        water.ecological_reserve_ml < 0 ||
        biodiversity.quality_fixed < 0 ||
        biodiversity.quality_fixed > unit_interval_scale ||
        biodiversity.required_minimum_fixed < 0 ||
        biodiversity.required_minimum_fixed > unit_interval_scale) {
        throw std::invalid_argument("fixed-point water or biodiversity input outside domain");
    }

    const bool water_compliant =
        water.allocated_ml <= water.permitted_ml &&
        water.permitted_ml - water.allocated_ml >= water.ecological_reserve_ml;
    const bool biodiversity_compliant =
        biodiversity.quality_fixed >= biodiversity.required_minimum_fixed;
    const bool biodiversity_violation = !biodiversity_compliant;
    const bool allow = water_compliant && biodiversity_compliant;

    const double reserve_ratio = water.permitted_ml > 0
        ? static_cast<double>(water.permitted_ml - water.allocated_ml) /
              static_cast<double>(water.permitted_ml)
        : 0.0;
    const double biodiversity_ratio =
        static_cast<double>(biodiversity.quality_fixed) / unit_interval_scale;
    const double knowledge = std::clamp(
        0.50 * biodiversity_ratio + 0.50 * std::clamp(reserve_ratio, 0.0, 1.0),
        0.0, 1.0);
    const double impact = allow
        ? std::clamp(0.50 * biodiversity_ratio +
                     0.50 * std::clamp(reserve_ratio, 0.0, 1.0), 0.0, 1.0)
        : 0.0;

    return {water_compliant, biodiversity_compliant, biodiversity_violation,
            allow, knowledge, impact};
}

inline bool required_cross_shard_unsat(const CrossShardDecision& decision) {
    return !(decision.water_compliant &&
             decision.biodiversity_violation &&
             decision.allow);
}

/*
Authorization separation property:

For every hardware-adapter transition Dispatch(action), there exists an
AuthorizationEvidence record accepted by ProofCheckedDispatcher for the same
action identifier, with:
- externally verified authorization evidence;
- risk_of_harm<=0.30;
- issue_time<=now<=expiry_time;
- exact policy identifier match;
- single-use sequence number.

No adapter exposes a public direct-dispatch method. The dispatcher is the sole
object that emits an approved actuation record. A real deployment additionally
requires process isolation and hardware capability controls, because C++ alone
cannot prevent unrelated privileged processes from accessing hardware.
*/
struct AuthorizationEvidence {
    std::string action_identifier;
    std::string policy_identifier;
    std::uint64_t issue_time_s{};
    std::uint64_t expiry_time_s{};
    std::uint64_t sequence{};
    std::int32_t risk_of_harm_fixed{};
    bool externally_verified{};
};

struct ApprovedActuation {
    std::string action_identifier;
    std::string policy_identifier;
    std::uint64_t sequence{};
    std::int32_t risk_of_harm_fixed{};
};

class ProofCheckedDispatcher {
public:
    explicit ProofCheckedDispatcher(std::string required_policy_identifier)
        : required_policy_identifier_(std::move(required_policy_identifier)) {
        if (required_policy_identifier_.empty()) {
            throw std::invalid_argument("policy identifier is required");
        }
    }

    bool accept(const AuthorizationEvidence& evidence, std::uint64_t now_s) {
        if (evidence.action_identifier.empty() ||
            evidence.policy_identifier != required_policy_identifier_ ||
            !evidence.externally_verified ||
            evidence.issue_time_s > now_s ||
            evidence.expiry_time_s < now_s ||
            evidence.expiry_time_s < evidence.issue_time_s ||
            evidence.risk_of_harm_fixed < 0 ||
            evidence.risk_of_harm_fixed > risk_of_harm_limit_fixed ||
            evidence.sequence <= last_accepted_sequence_) {
            return false;
        }

        last_accepted_sequence_ = evidence.sequence;
        latest_ = {evidence.action_identifier, evidence.policy_identifier,
                   evidence.sequence, evidence.risk_of_harm_fixed};
        has_latest_ = true;
        return true;
    }

    bool has_approved_actuation() const noexcept {
        return has_latest_;
    }

    const ApprovedActuation& latest_approved_actuation() const {
        if (!has_latest_) throw std::runtime_error("no proof-checked actuation exists");
        return latest_;
    }

private:
    std::string required_policy_identifier_;
    std::uint64_t last_accepted_sequence_{0};
    ApprovedActuation latest_;
    bool has_latest_{false};
};

}  // namespace eco_restoration
