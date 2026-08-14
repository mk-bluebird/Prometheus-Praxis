// File: cpp/tools/deterministic_scenario_library.cpp
#include "deterministic_scenario_library.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace {

constexpr std::uint64_t kMultiplier = 6364136223846793005ULL;
constexpr std::uint64_t kIncrement = 1442695040888963407ULL;
constexpr double kInverseTwoToFiftyThree = 1.0 / 9007199254740992.0;

bool IsFiniteNonNegative(const double value) noexcept {
    return std::isfinite(value) && value >= 0.0;
}

bool IsFiniteRange(const double minimum, const double maximum) noexcept {
    return std::isfinite(minimum) &&
           std::isfinite(maximum) &&
           minimum <= maximum;
}

bool EqualSequences(const std::vector<double>& left,
                    const std::vector<double>& right) noexcept {
    if (left.size() != right.size()) {
        return false;
    }

    for (std::size_t index = 0U; index < left.size(); ++index) {
        if (left[index] != right[index]) {
            return false;
        }
    }

    return true;
}

bool EqualScenarios(
    const std::vector<eco_restoration::RainfallScenario>& left,
    const std::vector<eco_restoration::RainfallScenario>& right) noexcept {
    if (left.size() != right.size()) {
        return false;
    }

    for (std::size_t scenario_index = 0U;
         scenario_index < left.size();
         ++scenario_index) {
        if (left[scenario_index].probability !=
                right[scenario_index].probability ||
            !EqualSequences(
                left[scenario_index].rainfall_by_horizon,
                right[scenario_index].rainfall_by_horizon)) {
            return false;
        }
    }

    return true;
}

}  // namespace

DeterministicScenarioGenerator::DeterministicScenarioGenerator(
    const std::uint64_t seed) noexcept
    : state_(seed) {}

std::uint64_t DeterministicScenarioGenerator::NextUnsigned() noexcept {
    state_ = state_ * kMultiplier + kIncrement;
    return state_;
}

double DeterministicScenarioGenerator::NextUnitInterval() noexcept {
    const std::uint64_t random_bits = NextUnsigned() >> 11U;
    return static_cast<double>(random_bits) * kInverseTwoToFiftyThree;
}

double DeterministicScenarioGenerator::NextInRange(
    const double minimum,
    const double maximum) noexcept {
    if (!IsFiniteRange(minimum, maximum)) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    if (minimum == maximum) {
        return minimum;
    }

    return minimum + (maximum - minimum) * NextUnitInterval();
}

std::vector<double> DeterministicScenarioGenerator::GenerateUnitIntervalValues(
    const std::size_t count) {
    std::vector<double> values;
    values.reserve(count);

    for (std::size_t index = 0U; index < count; ++index) {
        values.push_back(NextUnitInterval());
    }

    return values;
}

std::vector<eco_restoration::RainfallScenario>
DeterministicScenarioGenerator::GenerateRainfallScenarios(
    const std::size_t scenario_count,
    const std::size_t horizon,
    const double maximum_rain) {
    if (scenario_count == 0U ||
        horizon == 0U ||
        !IsFiniteNonNegative(maximum_rain)) {
        return {};
    }

    std::vector<eco_restoration::RainfallScenario> scenarios;
    scenarios.reserve(scenario_count);

    const double probability =
        1.0 / static_cast<double>(scenario_count);

    double probability_assigned = 0.0;

    for (std::size_t scenario_index = 0U;
         scenario_index < scenario_count;
         ++scenario_index) {
        eco_restoration::RainfallScenario scenario{};
        scenario.probability =
            scenario_index + 1U == scenario_count
                ? 1.0 - probability_assigned
                : probability;

        probability_assigned += scenario.probability;
        scenario.rainfall_by_horizon.reserve(horizon);

        for (std::size_t horizon_index = 0U;
             horizon_index < horizon;
             ++horizon_index) {
            scenario.rainfall_by_horizon.push_back(
                NextInRange(0.0, maximum_rain));
        }

        scenarios.push_back(std::move(scenario));
    }

    return scenarios;
}

bool DeterministicScenarioLibrarySelfTest() {
    DeterministicScenarioGenerator first(0x123456789abcdef0ULL);
    DeterministicScenarioGenerator second(0x123456789abcdef0ULL);
    DeterministicScenarioGenerator different(0x0fedcba987654321ULL);

    const std::vector<double> first_values =
        first.GenerateUnitIntervalValues(12U);
    const std::vector<double> second_values =
        second.GenerateUnitIntervalValues(12U);
    const std::vector<double> different_values =
        different.GenerateUnitIntervalValues(12U);

    if (!EqualSequences(first_values, second_values) ||
        EqualSequences(first_values, different_values)) {
        return false;
    }

    for (const double value : first_values) {
        if (!std::isfinite(value) || value < 0.0 || value >= 1.0) {
            return false;
        }
    }

    DeterministicScenarioGenerator range_generator(42U);
    const double fixed_value = range_generator.NextInRange(0.75, 0.75);
    const double invalid_value = range_generator.NextInRange(1.0, 0.0);

    if (fixed_value != 0.75 || !std::isnan(invalid_value)) {
        return false;
    }

    DeterministicScenarioGenerator scenario_left(777U);
    DeterministicScenarioGenerator scenario_right(777U);

    const std::vector<eco_restoration::RainfallScenario> generated_left =
        scenario_left.GenerateRainfallScenarios(3U, 4U, 0.20);
    const std::vector<eco_restoration::RainfallScenario> generated_right =
        scenario_right.GenerateRainfallScenarios(3U, 4U, 0.20);

    if (generated_left.size() != 3U ||
        !EqualScenarios(generated_left, generated_right)) {
        return false;
    }

    double probability_sum = 0.0;
    for (const eco_restoration::RainfallScenario& scenario : generated_left) {
        if (scenario.rainfall_by_horizon.size() != 4U ||
            !std::isfinite(scenario.probability) ||
            scenario.probability < 0.0 ||
            scenario.probability > 1.0) {
            return false;
        }

        probability_sum += scenario.probability;

        for (const double rainfall : scenario.rainfall_by_horizon) {
            if (!std::isfinite(rainfall) ||
                rainfall < 0.0 ||
                rainfall > 0.20) {
                return false;
            }
        }
    }

    if (probability_sum != 1.0) {
        return false;
    }

    DeterministicScenarioGenerator invalid_generator(9U);
    return invalid_generator.GenerateRainfallScenarios(0U, 2U, 0.10).empty() &&
           invalid_generator.GenerateRainfallScenarios(2U, 0U, 0.10).empty() &&
           invalid_generator.GenerateRainfallScenarios(2U, 2U, -0.10).empty();
}
