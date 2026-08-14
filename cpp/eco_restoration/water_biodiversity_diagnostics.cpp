// File: cpp/eco_restoration/water_biodiversity_diagnostics.cpp
#include "water_biodiversity_diagnostics.hpp"

#include <cmath>
#include <cstdint>
#include <limits>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace prometheus_praxis::eco_restoration {
namespace {

bool IsNonNegative(std::int64_t value) noexcept {
    return value >= 0;
}

bool IsUnitInterval(double value) noexcept {
    return std::isfinite(value) && value >= 0.0 && value <= 1.0;
}

bool TryAddNonNegative(
    std::int64_t left,
    std::int64_t right,
    std::int64_t& sum) noexcept {
    if (!IsNonNegative(left) || !IsNonNegative(right) ||
        left > std::numeric_limits<std::int64_t>::max() - right) {
        return false;
    }

    sum = left + right;
    return true;
}

bool AreBudgetInputsStructurallyValid(
    std::int64_t available_water_ml,
    std::int64_t ecological_reserve_ml,
    std::int64_t allocated_water_ml) noexcept {
    return IsNonNegative(available_water_ml) &&
           IsNonNegative(ecological_reserve_ml) &&
           IsNonNegative(allocated_water_ml) &&
           ecological_reserve_ml <= available_water_ml;
}

bool IsStakeholderStructurallyValid(
    const WaterRightsStakeholderScenario& stakeholder) noexcept {
    return !stakeholder.stakeholder.empty() &&
           IsNonNegative(stakeholder.minimum_allocation_ml) &&
           IsNonNegative(stakeholder.maximum_allocation_ml) &&
           IsNonNegative(stakeholder.allocated_ml) &&
           stakeholder.minimum_allocation_ml <= stakeholder.maximum_allocation_ml &&
           stakeholder.allocated_ml >= stakeholder.minimum_allocation_ml &&
           stakeholder.allocated_ml <= stakeholder.maximum_allocation_ml &&
           IsUnitInterval(stakeholder.utility_weight);
}

void AddReason(
    WaterBiodiversityPolicyVerification& verification,
    std::string reason) {
    verification.reasons.push_back(std::move(reason));
}

bool SumStakeholderAllocations(
    const std::vector<WaterRightsStakeholderScenario>& stakeholders,
    std::int64_t& total) noexcept {
    total = 0;

    for (const WaterRightsStakeholderScenario& stakeholder : stakeholders) {
        std::int64_t next_total{};
        if (!TryAddNonNegative(total, stakeholder.allocated_ml, next_total)) {
            return false;
        }
        total = next_total;
    }

    return true;
}

bool HasUniqueStakeholders(
    const std::vector<WaterRightsStakeholderScenario>& stakeholders) {
    std::set<std::string> names;
    for (const WaterRightsStakeholderScenario& stakeholder : stakeholders) {
        if (!IsStakeholderStructurallyValid(stakeholder) ||
            !names.insert(stakeholder.stakeholder).second) {
            return false;
        }
    }
    return true;
}

}  // namespace

WaterAllocationBudget ComputeWaterAllocationBudget(
    std::int64_t available_water_ml,
    std::int64_t ecological_reserve_ml,
    std::int64_t allocated_water_ml) {
    WaterAllocationBudget budget{
        available_water_ml,
        ecological_reserve_ml,
        allocated_water_ml,
        0,
        false};

    if (!AreBudgetInputsStructurallyValid(
            available_water_ml,
            ecological_reserve_ml,
            allocated_water_ml)) {
        return budget;
    }

    std::int64_t protected_and_allocated{};
    if (!TryAddNonNegative(
            ecological_reserve_ml,
            allocated_water_ml,
            protected_and_allocated) ||
        protected_and_allocated > available_water_ml) {
        return budget;
    }

    budget.remaining_water_ml =
        available_water_ml - protected_and_allocated;
    budget.balanced = true;
    return budget;
}

WaterBiodiversityPolicyVerification VerifyCrossShardWaterBiodiversityPolicy(
    std::int64_t available_water_ml,
    std::int64_t ecological_reserve_ml,
    std::int64_t allocated_water_ml,
    double biodiversity_index,
    double minimum_biodiversity_index,
    bool required_cross_shard_unsat) {
    WaterBiodiversityPolicyVerification verification;

    const bool valid_water = AreBudgetInputsStructurallyValid(
        available_water_ml,
        ecological_reserve_ml,
        allocated_water_ml);
    const bool valid_biodiversity =
        IsUnitInterval(biodiversity_index) &&
        IsUnitInterval(minimum_biodiversity_index);

    verification.structurally_valid = valid_water && valid_biodiversity;

    if (!valid_water) {
        AddReason(
            verification,
            "water values must be non-negative and ecological reserve must not exceed availability");
    }

    if (!valid_biodiversity) {
        AddReason(
            verification,
            "biodiversity indices must be finite and within [0,1]");
    }

    const WaterAllocationBudget budget = ComputeWaterAllocationBudget(
        available_water_ml,
        ecological_reserve_ml,
        allocated_water_ml);
    verification.water_compliant = budget.balanced;

    if (!verification.water_compliant) {
        AddReason(
            verification,
            "allocation consumes protected ecological reserve or exceeds water availability");
    }

    verification.biodiversity_compliant =
        valid_biodiversity && biodiversity_index >= minimum_biodiversity_index;
    if (!verification.biodiversity_compliant) {
        AddReason(
            verification,
            "biodiversity index is below the required minimum");
    }

    verification.invariant_holds = !required_cross_shard_unsat;
    if (!verification.invariant_holds) {
        AddReason(
            verification,
            "required cross-shard water and biodiversity invariant is unsatisfied");
    }

    verification.allowed =
        verification.structurally_valid &&
        verification.water_compliant &&
        verification.biodiversity_compliant &&
        verification.invariant_holds;
    return verification;
}

WaterBiodiversityPolicyVerification EvaluateEquitableWaterAllocationDiagnostic(
    std::int64_t available_water_ml,
    std::int64_t ecological_reserve_ml,
    const std::vector<WaterRightsStakeholderScenario>& stakeholders,
    double biodiversity_index,
    double minimum_biodiversity_index,
    bool required_cross_shard_unsat) {
    const bool stakeholders_valid = HasUniqueStakeholders(stakeholders);
    std::int64_t allocated_water_ml{};
    const bool allocation_sum_valid =
        stakeholders_valid &&
        SumStakeholderAllocations(stakeholders, allocated_water_ml);

    WaterBiodiversityPolicyVerification verification =
        VerifyCrossShardWaterBiodiversityPolicy(
            available_water_ml,
            ecological_reserve_ml,
            allocation_sum_valid ? allocated_water_ml : 0,
            biodiversity_index,
            minimum_biodiversity_index,
            required_cross_shard_unsat);

    if (!stakeholders_valid) {
        verification.structurally_valid = false;
        verification.allowed = false;
        AddReason(
            verification,
            "stakeholders must have unique non-empty names, valid allocation bounds, "
            "and finite utility weights in [0,1]");
    }

    if (!allocation_sum_valid && stakeholders_valid) {
        verification.structurally_valid = false;
        verification.water_compliant = false;
        verification.allowed = false;
        AddReason(
            verification,
            "total stakeholder allocation exceeds int64 millilitre capacity");
    }

    return verification;
}

std::vector<WaterRightsStakeholderScenario>
BuildDeterministicWaterRightsScenario() {
    return {
        WaterRightsStakeholderScenario{
            "riparian_habitat",
            450000,
            700000,
            600000,
            1.0},
        WaterRightsStakeholderScenario{
            "community_irrigation",
            250000,
            500000,
            350000,
            0.8},
        WaterRightsStakeholderScenario{
            "native_planting",
            150000,
            300000,
            200000,
            0.9}};
}

std::string ExplainWaterRightsStakeholderScenario(
    const std::vector<WaterRightsStakeholderScenario>& stakeholders) {
    std::ostringstream output;
    output << "water_rights_stakeholders; count=" << stakeholders.size();

    for (const WaterRightsStakeholderScenario& stakeholder : stakeholders) {
        output << "; stakeholder=" << stakeholder.stakeholder
               << "; minimum_ml=" << stakeholder.minimum_allocation_ml
               << "; maximum_ml=" << stakeholder.maximum_allocation_ml
               << "; allocated_ml=" << stakeholder.allocated_ml
               << "; utility_weight=" << stakeholder.utility_weight;
    }

    return output.str();
}

std::string ExplainWaterAllocationBudget(
    const WaterAllocationBudget& budget) {
    std::ostringstream output;
    output << "water_allocation_budget"
           << "; available_ml=" << budget.available_water_ml
           << "; ecological_reserve_ml=" << budget.ecological_reserve_ml
           << "; allocated_ml=" << budget.allocated_water_ml
           << "; remaining_ml=" << budget.remaining_water_ml
           << "; balanced=" << (budget.balanced ? "true" : "false");
    return output.str();
}

bool WaterBiodiversityDiagnosticsSelfTest() {
    const WaterBiodiversityPolicyVerification safe =
        VerifyCrossShardWaterBiodiversityPolicy(
            2000000,
            700000,
            1000000,
            0.82,
            0.70,
            false);
    if (!safe.structurally_valid ||
        !safe.water_compliant ||
        !safe.biodiversity_compliant ||
        !safe.invariant_holds ||
        !safe.allowed ||
        !safe.reasons.empty()) {
        return false;
    }

    const WaterAllocationBudget safe_budget =
        ComputeWaterAllocationBudget(2000000, 700000, 1000000);
    if (!safe_budget.balanced || safe_budget.remaining_water_ml != 300000) {
        return false;
    }

    const WaterBiodiversityPolicyVerification reserve_violation =
        VerifyCrossShardWaterBiodiversityPolicy(
            1000,
            800,
            300,
            0.80,
            0.70,
            false);
    if (reserve_violation.allowed || reserve_violation.water_compliant) {
        return false;
    }

    const WaterAllocationBudget overflow =
        ComputeWaterAllocationBudget(
            std::numeric_limits<std::int64_t>::max(),
            std::numeric_limits<std::int64_t>::max() - 10,
            11);
    if (overflow.balanced) {
        return false;
    }

    const WaterBiodiversityPolicyVerification low_biodiversity =
        VerifyCrossShardWaterBiodiversityPolicy(
            1000,
            300,
            500,
            0.40,
            0.70,
            false);
    if (low_biodiversity.allowed || low_biodiversity.biodiversity_compliant) {
        return false;
    }

    const WaterBiodiversityPolicyVerification invalid_water =
        VerifyCrossShardWaterBiodiversityPolicy(
            -1,
            0,
            0,
            0.90,
            0.70,
            false);
    if (invalid_water.structurally_valid || invalid_water.allowed) {
        return false;
    }

    std::vector<WaterRightsStakeholderScenario> duplicate =
        BuildDeterministicWaterRightsScenario();
    duplicate.push_back(duplicate.front());
    const WaterBiodiversityPolicyVerification duplicate_result =
        EvaluateEquitableWaterAllocationDiagnostic(
            2500000,
            700000,
            duplicate,
            0.85,
            0.70,
            false);
    if (duplicate_result.structurally_valid || duplicate_result.allowed) {
        return false;
    }

    const WaterBiodiversityPolicyVerification missing_invariant =
        VerifyCrossShardWaterBiodiversityPolicy(
            2000000,
            700000,
            1000000,
            0.82,
            0.70,
            true);
    if (missing_invariant.invariant_holds || missing_invariant.allowed) {
        return false;
    }

    const std::vector<WaterRightsStakeholderScenario> stakeholders =
        BuildDeterministicWaterRightsScenario();
    const WaterBiodiversityPolicyVerification equitable =
        EvaluateEquitableWaterAllocationDiagnostic(
            2000000,
            700000,
            stakeholders,
            0.82,
            0.70,
            false);
    if (!equitable.allowed) {
        return false;
    }

    std::vector<WaterRightsStakeholderScenario> allocation_overflow{
        WaterRightsStakeholderScenario{
            "river_restoration",
            0,
            std::numeric_limits<std::int64_t>::max(),
            std::numeric_limits<std::int64_t>::max(),
            1.0},
        WaterRightsStakeholderScenario{
            "wetland_recovery",
            0,
            1,
            1,
            1.0}};
    const WaterBiodiversityPolicyVerification overflow_result =
        EvaluateEquitableWaterAllocationDiagnostic(
            std::numeric_limits<std::int64_t>::max(),
            0,
            allocation_overflow,
            0.90,
            0.70,
            false);
    if (overflow_result.structurally_valid ||
        overflow_result.water_compliant ||
        overflow_result.allowed) {
        return false;
    }

    return ExplainWaterAllocationBudget(safe_budget) ==
               "water_allocation_budget; available_ml=2000000; "
               "ecological_reserve_ml=700000; allocated_ml=1000000; "
               "remaining_ml=300000; balanced=true" &&
           ExplainWaterRightsStakeholderScenario(stakeholders).find(
               "stakeholder=riparian_habitat") != std::string::npos;
}

}  // namespace prometheus_praxis::eco_restoration
