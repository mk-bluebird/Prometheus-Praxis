// File: cpp/tools/foundation_golden_tests.hpp
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace prometheus_praxis::foundation::golden {

struct GoldenCliExpectation {
    std::string command;
    int exit_code{};
    std::string stdout_exact;
    std::string stderr_exact;
    bool requires_stdout_newline{};
    bool requires_stderr_newline{};
    bool compare_stdout_bytes{true};
    bool compare_stderr_bytes{true};
};

struct GoldenCliObservation {
    std::string command;
    int exit_code{};
    std::string stdout_observed;
    std::string stderr_observed;
};

struct GoldenCliComparison {
    bool matches{};
    bool command_matches{};
    bool exit_code_matches{};
    bool stdout_matches{};
    bool stderr_matches{};
    std::string expected_command;
    int expected_exit_code{};
    std::string expected_stdout;
    std::string expected_stderr;
    std::string observed_command;
    int observed_exit_code{};
    std::string observed_stdout;
    std::string observed_stderr;
    std::vector<std::string> reasons;
};

[[nodiscard]] bool RegisterGoldenExpectation(
    std::vector<GoldenCliExpectation>& expectations,
    GoldenCliExpectation expectation);

[[nodiscard]] std::vector<GoldenCliExpectation> LoadGoldenExpectations();

[[nodiscard]] std::optional<GoldenCliExpectation> FindGoldenExpectation(
    const std::vector<GoldenCliExpectation>& expectations,
    std::string_view command);

[[nodiscard]] GoldenCliComparison CompareObservedToGolden(
    const GoldenCliExpectation& expectation,
    const GoldenCliObservation& observation);

[[nodiscard]] std::string ExplainGoldenMismatch(
    const GoldenCliComparison& comparison);

[[nodiscard]] bool IsGoldenComparisonCorrect(
    const GoldenCliComparison& comparison);

[[nodiscard]] bool FoundationGoldenTestsSelfTest();

}  // namespace prometheus_praxis::foundation::golden
