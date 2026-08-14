// File: cpp/eco_restoration/water_biodiversity_diagnostics.cpp
#include "water_biodiversity_diagnostics.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace {

bool IsFiniteUnitInterval(const double value) noexcept {
    return std::isfinite(value) && value >= 0.0 && value <= 1.0;
}

bool HasUniqueNonEmptyStakeholderNames(
    const std::vector<WaterRightsStakeholderScenario>& stakeholders) {
    for (std::size_t left = 0U; left < stakeholders.size(); ++left) {
        if (stakeholders[left].stakeholder.empty()) {
            return false;
        }

        for (std::size_t right = left + 1U; right < stakeholders.size();
             ++right) {
            if (stakeholders[left].stakeholder == stakeholders[right].stakeholder) {
                return false;
            }
        }
    }
    return true;
}

bool StakeholderBoundsAreValid(
    const WaterRightsStakeholderScenario& stakeholder) noexcept {
    return stakeholder.minimum_allocation_ml >= 0 &&
           stakeholder.maximum_allocation_ml >=
               stakeholder.minimum_allocation_ml &&
           stakeholder.allocated_ml >= stakeholder.minimum_allocation_ml &&
           stakeholder.allocated_ml <= stakeholder.maximum_allocation_ml &&
           std::isfinite(stakeholder.utility_weight) &&
           stakeholder.utility_weight > 0.0;
}

bool CheckedAdd(const std::int64_t left,
                const std::int64_t right,
                std::int64_t& result) noexcept {
    if ((right > 0 &&
         left > std::numeric_limits<std::int64_t>::max() - right) ||
        (right < 0 &&
         left < std::numeric_limits<std::int64_t>::min() - right)) {
        return false;
    }

    result = left + right;
    return true;
}

std::string FormatMillilitres(const std::int64_t value) {
    std::ostringstream output;
    output << value << " ml";
    return output.str();
}

}  // namespace

WaterBiodiversityPolicyVerification VerifyCrossShardWaterBiodiversityPolicy(
    const std::int64_t available_water_ml,
    const std::int64_t ecological_reserve_ml,
    const std::int64_t allocated_water_ml,
    const double biodiversity_index,
    const double minimum_biodiversity_index,
    const bool required_cross_shard_unsat) {
    WaterBiodiversityPolicyVerification result{};

    if (available_water_ml < 0 ||
        ecological_reserve_ml < 0 ||
        allocated_water_ml < 0) {
        result.reasons.emplace_back(
            "water quantities must be non-negative millilitre values");
    }

    if (ecological_reserve_ml > available_water_ml) {
        result.reasons.emplace_back(
            "ecological reserve exceeds available water");
    }

    if (!IsFiniteUnitInterval(biodiversity_index) ||
        !IsFiniteUnitInterval(minimum_biodiversity_index)) {
        result.reasons.emplace_back(
            "biodiversity indices must be finite values in [0,1]");
    }

    result.structurally_valid = result.reasons.empty();

    if (!result.structurally_valid) {
        result.water_compliant = false;
        result.biodiversity_compliant = false;
        result.invariant_holds = false;
        result.allowed = false;
        return result;
    }

    const std::int64_t distributable_water_ml =
        available_water_ml - ecological_reserve_ml;

    result.water_compliant = allocated_water_ml <= distributable_water_ml;
    if (!result.water_compliant) {
        result.reasons.emplace_back(
            "allocation exceeds water available after ecological reserve");
    }

    result.biodiversity_compliant =
        biodiversity_index >= minimum_biodiversity_index;
    if (!result.biodiversity_compliant) {
        result.reasons.emplace_back(
            "biodiversity index is below the required minimum");
    }

    result.invariant_holds = required_cross_shard_unsat;
    if (!result.invariant_holds) {
        result.reasons.emplace_back(
            "required_cross_shard_unsat invariant is not satisfied");
    }

    result.allowed = result.structurally_valid &&
                     result.water_compliant &&
                     result.biodiversity_compliant &&
                     result.invariant_holds;
    return result;
}

WaterBiodiversityPolicyVerification EvaluateEquitableWaterAllocationDiagnostic(
    const std::int64_t available_water_ml,
    const std::int64_t ecological_reserve_ml,
    const std::vector<WaterRightsStakeholderScenario>& stakeholders,
    const double biodiversity_index,
    const double minimum_biodiversity_index,
    const bool required_cross_shard_unsat) {
    std::int64_t allocated_water_ml = 0;
    bool scenario_valid = HasUniqueNonEmptyStakeholderNames(stakeholders);

    if (!scenario_valid) {
        WaterBiodiversityPolicyVerification result{};
        result.reasons.emplace_back(
            "stakeholder names must be non-empty and unique");
        return result;
    }

    for (const WaterRightsStakeholderScenario& stakeholder : stakeholders) {
        if (!StakeholderBoundsAreValid(stakeholder)) {
            WaterBiodiversityPolicyVerification result{};
            result.reasons.emplace_back(
                "stakeholder allocation bounds or utility weight are invalid");
            return result;
        }

        std::int64_t next_total = 0;
        if (!CheckedAdd(
                allocated_water_ml,
                stakeholder.allocated_ml,
                next_total)) {
            WaterBiodiversityPolicyVerification result{};
            result.reasons.emplace_back(
                "total stakeholder allocation overflows int64 millilitres");
            return result;
        }
        allocated_water_ml = next_total;
    }

    WaterBiodiversityPolicyVerification result =
        VerifyCrossShardWaterBiodiversityPolicy(
            available_water_ml,
            ecological_reserve_ml,
            allocated_water_ml,
            biodiversity_index,
            minimum_biodiversity_index,
            required_cross_shard_unsat);

    if (!scenario_valid) {
        result.structurally_valid = false;
        result.allowed = false;
    }

    return result;
}

std::vector<WaterRightsStakeholderScenario>
BuildDeterministicWaterRightsScenario() {
    return {
        WaterRightsStakeholderScenario{
            "riparian_habitat",
            300000,
            500000,
            400000,
            1.50},
        WaterRightsStakeholderScenario{
            "community_garden",
            120000,
            250000,
            180000,
            1.00},
        WaterRightsStakeholderScenario{
            "native_tree_restoration",
            150000,
            300000,
            220000,
            1.25}};
}

std::string ExplainWaterRightsStakeholderScenario(
    const std::vector<WaterRightsStakeholderScenario>& stakeholders) {
    std::ostringstream output;
    output << "water_rights_stakeholders=" << stakeholders.size();

    for (const WaterRightsStakeholderScenario& stakeholder : stakeholders) {
        output << '\n'
               << stakeholder.stakeholder
               << ": minimum=" << FormatMillilitres(
                   stakeholder.minimum_allocation_ml)
               << ", maximum=" << FormatMillilitres(
                   stakeholder.maximum_allocation_ml)
               << ", allocated=" << FormatMillilitres(
                   stakeholder.allocated_ml)
               << ", utility_weight=" << stakeholder.utility_weight;
    }

    return output.str();
}

bool WaterBiodiversityDiagnosticsSelfTest() {
    const std::vector<WaterRightsStakeholderScenario> safe_scenario =
        BuildDeterministicWaterRightsScenario();

    const WaterBiodiversityPolicyVerification safe =
        EvaluateEquitableWaterAllocationDiagnostic(
            1500000,
            500000,
            safe_scenario,
            0.82,
            0.70,
            true);

    if (!safe.structurally_valid ||
        !safe.water_compliant ||
        !safe.biodiversity_compliant ||
        !safe.invariant_holds ||
        !safe.allowed) {
        return false;
    }

    const WaterBiodiversityPolicyVerification reserve_violation =
        EvaluateEquitableWaterAllocationDiagnostic(
            1000000,
            500000,
            safe_scenario,
            0.82,
            0.70,
            true);

    if (!reserve_violation.structurally_valid ||
        reserve_violation.water_compliant ||
        reserve_violation.allowed) {
        return false;
    }

    const WaterBiodiversityPolicyVerification low_biodiversity =
        EvaluateEquitableWaterAllocationDiagnostic(
            1500000,
            500000,
            safe_scenario,
            0.60,
            0.70,
            true);

    if (!low_biodiversity.structurally_valid ||
        low_biodiversity.biodiversity_compliant ||
        low_biodiversity.allowed) {
        return false;
    }

    const WaterBiodiversityPolicyVerification invalid_water =
        VerifyCrossShardWaterBiodiversityPolicy(
            -1,
            0,
            0,
            0.80,
            0.70,
            true);

    if (invalid_water.structurally_valid || invalid_water.allowed) {
        return false;
    }

    std::vector<WaterRightsStakeholderScenario> duplicates = safe_scenario;
    duplicates.push_back(
        WaterRightsStakeholderScenario{
            "community_garden",
            1000,
            2000,
            1500,
            1.00});

    const WaterBiodiversityPolicyVerification duplicate_stakeholders =
        EvaluateEquitableWaterAllocationDiagnostic(
            1500000,
            500000,
            duplicates,
            0.82,
            0.70,
            true);

    if (duplicate_stakeholders.structurally_valid ||
        duplicate_stakeholders.allowed) {
        return false;
    }

    const WaterBiodiversityPolicyVerification missing_invariant =
        EvaluateEquitableWaterAllocationDiagnostic(
            1500000,
            500000,
            safe_scenario,
            0.82,
            0.70,
            false);

    if (missing_invariant.invariant_holds || missing_invariant.allowed) {
        return false;
    }

    return ExplainWaterRightsStakeholderScenario(safe_scenario).find(
               "water_rights_stakeholders=3") == 0U;
}
