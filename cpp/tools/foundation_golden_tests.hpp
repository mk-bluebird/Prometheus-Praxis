// File: cpp/tools/foundation_golden_tests.hpp
#pragma once

#include <optional>
#include <string>
#include <vector>

struct GoldenCliExpectation {
    std::string command;
    int exit_code{};
    std::string stdout_exact;
    std::string stderr_exact;
    bool normalized_newlines{};
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
};

bool RegisterGoldenExpectation(
    std::vector<GoldenCliExpectation>& expectations,
    GoldenCliExpectation expectation);

std::vector<GoldenCliExpectation> LoadGoldenExpectations();

std::optional<GoldenCliExpectation> FindGoldenExpectation(
    const std::vector<GoldenCliExpectation>& expectations,
    const std::string& command);

GoldenCliComparison CompareObservedToGolden(
    const GoldenCliExpectation& expectation,
    const GoldenCliObservation& observation);

std::string ExplainGoldenMismatch(const GoldenCliComparison& comparison);

bool FoundationGoldenTestsSelfTest();
