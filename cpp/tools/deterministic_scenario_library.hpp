// File: cpp/tools/deterministic_scenario_library.hpp
#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "../simulation/irrigation_scenario_diagnostics.hpp"

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

private:
    std::uint64_t state_;
};

bool DeterministicScenarioLibrarySelfTest();
