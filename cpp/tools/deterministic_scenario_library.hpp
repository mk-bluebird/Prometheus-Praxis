// File: cpp/tools/deterministic_scenario_library.hpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "../simulation/irrigation_scenario_diagnostics.hpp"

namespace prometheus_praxis::foundation::scenario {

class DeterministicScenarioGenerator {
public:
    explicit DeterministicScenarioGenerator(std::uint64_t seed) noexcept;

    std::uint64_t NextUnsigned() noexcept;
    double NextUnitInterval() noexcept;
    double NextInRange(double minimum, double maximum) noexcept;

    std::vector<double> GenerateUnitIntervalValues(std::size_t count);

    std::vector<eco_restoration::RainfallScenario> GenerateRainfallScenarios(
        std::size_t scenario_count,
        std::size_t horizon,
        double maximum_rain);

    std::uint64_t State() const noexcept;
    void Reset(std::uint64_t seed) noexcept;

private:
    std::uint64_t state_{};
};

std::vector<double> GenerateProbabilities(std::size_t count);

std::vector<double> GenerateBoundedValues(
    std::size_t count,
    double minimum,
    double maximum);

bool DeterministicScenarioLibrarySelfTest();

}  // namespace prometheus_praxis::foundation::scenario
