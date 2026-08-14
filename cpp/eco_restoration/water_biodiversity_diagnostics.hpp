// File: cpp/eco_restoration/water_biodiversity_diagnostics.hpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace prometheus_praxis::eco_restoration {

struct WaterBiodiversityPolicyVerification {
    bool structurally_valid{};
    bool water_compliant{};
    bool biodiversity_compliant{};
    bool invariant_holds{};
    bool allowed{};
    std::vector<std::string> reasons;
};

struct WaterRightsStakeholderScenario {
    std::string stakeholder;
    std::int64_t minimum_allocation_ml{};
    std::int64_t maximum_allocation_ml{};
    std::int64_t allocated_ml{};
    double utility_weight{};
};

struct WaterAllocationBudget {
    std::int64_t available_water_ml{};
    std::int64_t ecological_reserve_ml{};
    std::int64_t allocated_water_ml{};
    std::int64_t remaining_water_ml{};
    bool balanced{};
};

[[nodiscard]] WaterBiodiversityPolicyVerification
VerifyCrossShardWaterBiodiversityPolicy(
    std::int64_t available_water_ml,
    std::int64_t ecological_reserve_ml,
    std::int64_t allocated_water_ml,
    double biodiversity_index,
    double minimum_biodiversity_index,
    bool required_cross_shard_unsat);

[[nodiscard]] WaterAllocationBudget ComputeWaterAllocationBudget(
    std::int64_t available_water_ml,
    std::int64_t ecological_reserve_ml,
    std::int64_t allocated_water_ml);

[[nodiscard]] WaterBiodiversityPolicyVerification
EvaluateEquitableWaterAllocationDiagnostic(
    std::int64_t available_water_ml,
    std::int64_t ecological_reserve_ml,
    const std::vector<WaterRightsStakeholderScenario>& stakeholders,
    double biodiversity_index,
    double minimum_biodiversity_index,
    bool required_cross_shard_unsat);

[[nodiscard]] std::vector<WaterRightsStakeholderScenario>
BuildDeterministicWaterRightsScenario();

[[nodiscard]] std::string ExplainWaterRightsStakeholderScenario(
    const std::vector<WaterRightsStakeholderScenario>& stakeholders);

[[nodiscard]] std::string ExplainWaterAllocationBudget(
    const WaterAllocationBudget& budget);

[[nodiscard]] bool WaterBiodiversityDiagnosticsSelfTest();

}  // namespace prometheus_praxis::eco_restoration
