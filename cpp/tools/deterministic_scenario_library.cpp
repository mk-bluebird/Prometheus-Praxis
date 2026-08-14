// File: cpp/tools/deterministic_scenario_library.cpp
#include "deterministic_scenario_library.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace prometheus_praxis::foundation::scenario {
namespace {

constexpr std::uint64_t kMultiplier = 6364136223846793005ULL;
constexpr std::uint64_t kIncrement = 1442695040888963407ULL;
constexpr double kInverseTwoToFiftyThree = 1.0 / 9007199254740992.0;

bool IsFinite(const double value) noexcept {
    return std::isfinite(value);
}

bool IsFiniteNonNegative(const double value) noexcept {
    return IsFinite(value) && value >= 0.0;
}

bool IsFiniteRange(
    const double minimum,
    const double maximum) noexcept {
    return IsFinite(minimum) &&
           IsFinite(maximum) &&
           minimum <= maximum;
}

bool EqualSequences(
    const std::vector<double>& left,
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
                left[scenario_index].rainfall,
                right[scenario_index].rainfall)) {
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

    const double span = maximum - minimum;
    if (!IsFinite(span)) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    const double value = minimum + span * NextUnitInterval();
    return IsFinite(value) ? value : std::numeric_limits<double>::quiet_NaN();
}

std::vector<double> DeterministicScenarioGenerator::GenerateUnitIntervalValues(
    const std::size_t count) {
    if (count == 0U) {
        return {};
    }

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

    const std::vector<double> probabilities =
        GenerateProbabilities(scenario_count);
    if (probabilities.size() != scenario_count) {
        return {};
    }

    std::vector<eco_restoration::RainfallScenario> scenarios;
    scenarios.reserve(scenario_count);

    for (std::size_t scenario_index = 0U;
         scenario_index < scenario_count;
         ++scenario_index) {
        eco_restoration::RainfallScenario scenario;
        scenario.probability = probabilities[scenario_index];
        scenario.rainfall.reserve(horizon);

        for (std::size_t horizon_index = 0U;
             horizon_index < horizon;
             ++horizon_index) {
            const double rainfall = NextInRange(0.0, maximum_rain);
            if (!IsFiniteNonNegative(rainfall) || rainfall > maximum_rain) {
                return {};
            }
            scenario.rainfall.push_back(rainfall);
        }

        scenarios.push_back(std::move(scenario));
    }

    return scenarios;
}

std::uint64_t DeterministicScenarioGenerator::State() const noexcept {
    return state_;
}

void DeterministicScenarioGenerator::Reset(
    const std::uint64_t seed) noexcept {
    state_ = seed;
}

std::vector<double> GenerateProbabilities(const std::size_t count) {
    if (count == 0U) {
        return {};
    }

    std::vector<double> probabilities(count, 0.0);
    if (count == 1U) {
        probabilities.front() = 1.0;
        return probabilities;
    }

    const double equal_probability = 1.0 / static_cast<double>(count);
    double assigned = 0.0;

    for (std::size_t index = 0U; index + 1U < count; ++index) {
        probabilities[index] = equal_probability;
        assigned += equal_probability;
    }

    probabilities.back() = 1.0 - assigned;
    return probabilities;
}

std::vector<double> GenerateBoundedValues(
    const std::size_t count,
    const double minimum,
    const double maximum) {
    if (count == 0U || !IsFiniteRange(minimum, maximum)) {
        return {};
    }

    DeterministicScenarioGenerator generator{0U};
    std::vector<double> values;
    values.reserve(count);

    for (std::size_t index = 0U; index < count; ++index) {
        const double value = generator.NextInRange(minimum, maximum);
        if (!IsFinite(value)) {
            return {};
        }
        values.push_back(value);
    }

    return values;
}

bool DeterministicScenarioLibrarySelfTest() {
    DeterministicScenarioGenerator first{0x123456789abcdef0ULL};
    DeterministicScenarioGenerator second{0x123456789abcdef0ULL};
    DeterministicScenarioGenerator different{0x0fedcba987654321ULL};

    const std::vector<double> first_values =
        first.GenerateUnitIntervalValues(32U);
    const std::vector<double> second_values =
        second.GenerateUnitIntervalValues(32U);
    const std::vector<double> different_values =
        different.GenerateUnitIntervalValues(32U);

    if (!EqualSequences(first_values, second_values) ||
        EqualSequences(first_values, different_values) ||
        first_values.size() != 32U) {
        return false;
    }

    for (const double value : first_values) {
        if (!IsFinite(value) || value < 0.0 || value >= 1.0) {
            return false;
        }
    }

    DeterministicScenarioGenerator reset_generator{42U};
    static_cast<void>(reset_generator.NextUnsigned());
    reset_generator.Reset(42U);
    DeterministicScenarioGenerator reset_reference{42U};
    if (reset_generator.NextUnsigned() != reset_reference.NextUnsigned()) {
        return false;
    }

    DeterministicScenarioGenerator range_generator{99U};
    if (range_generator.NextInRange(0.75, 0.75) != 0.75 ||
        !std::isnan(range_generator.NextInRange(1.0, 0.0)) ||
        !std::isnan(range_generator.NextInRange(
            std::numeric_limits<double>::infinity(), 1.0)) ||
        !GenerateBoundedValues(3U, 2.0, 1.0).empty() ||
        !GenerateBoundedValues(3U, 0.0,
            std::numeric_limits<double>::infinity()).empty()) {
        return false;
    }

    const std::vector<double> probabilities = GenerateProbabilities(3U);
    if (probabilities.size() != 3U ||
        probabilities[0] <= 0.0 ||
        probabilities[1] <= 0.0 ||
        probabilities[2] <= 0.0 ||
        probabilities[0] + probabilities[1] + probabilities[2] != 1.0) {
        return false;
    }

    DeterministicScenarioGenerator scenario_left{777U};
    DeterministicScenarioGenerator scenario_right{777U};
    const std::vector<eco_restoration::RainfallScenario> generated_left =
        scenario_left.GenerateRainfallScenarios(3U, 4U, 0.20);
    const std::vector<eco_restoration::RainfallScenario> generated_right =
        scenario_right.GenerateRainfallScenarios(3U, 4U, 0.20);

    if (generated_left.size() != 3U ||
        !EqualScenarios(generated_left, generated_right) ||
        scenario_left.State() != scenario_right.State()) {
        return false;
    }

    double probability_sum = 0.0;
    for (const eco_restoration::RainfallScenario& scenario : generated_left) {
        if (!IsFinite(scenario.probability) ||
            scenario.probability < 0.0 ||
            scenario.probability > 1.0 ||
            scenario.rainfall.size() != 4U) {
            return false;
        }

        probability_sum += scenario.probability;
        for (const double rainfall : scenario.rainfall) {
            if (!IsFiniteNonNegative(rainfall) || rainfall > 0.20) {
                return false;
            }
        }
    }

    DeterministicScenarioGenerator invalid_generator{9U};
    return probability_sum == 1.0 &&
           invalid_generator.GenerateUnitIntervalValues(0U).empty() &&
           invalid_generator.GenerateRainfallScenarios(0U, 2U, 0.10).empty() &&
           invalid_generator.GenerateRainfallScenarios(2U, 0U, 0.10).empty() &&
           invalid_generator.GenerateRainfallScenarios(2U, 2U, -0.10).empty() &&
           invalid_generator.GenerateRainfallScenarios(
               2U,
               2U,
               std::numeric_limits<double>::infinity()).empty();
}

}  // namespace prometheus_praxis::foundation::scenario
