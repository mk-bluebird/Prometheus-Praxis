// File: cpp/tools/foundation_golden_tests.cpp
#include "foundation_golden_tests.hpp"

#include <algorithm>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

bool IsValidCommand(const std::string& command) noexcept {
    return command.size() > 2U &&
           command[0] == '-' &&
           command[1] == '-';
}

bool ContainsNewlineNormalizedEquivalent(
    const std::string& expected,
    const std::string& observed) {
    const auto normalize = [](const std::string& input) {
        std::string normalized;
        normalized.reserve(input.size());

        for (std::size_t index = 0U; index < input.size(); ++index) {
            if (input[index] == '\r' &&
                index + 1U < input.size() &&
                input[index + 1U] == '\n') {
                normalized.push_back('\n');
                ++index;
                continue;
            }

            normalized.push_back(input[index]);
        }

        return normalized;
    };

    return normalize(expected) == normalize(observed);
}

bool StreamsMatch(
    const std::string& expected,
    const std::string& observed,
    const bool normalize_newlines) {
    return normalize_newlines
               ? ContainsNewlineNormalizedEquivalent(expected, observed)
               : expected == observed;
}

GoldenCliExpectation MakeExpectation(
    std::string command,
    const int exit_code,
    std::string stdout_exact,
    std::string stderr_exact,
    const bool normalized_newlines = false) {
    return GoldenCliExpectation{
        std::move(command),
        exit_code,
        std::move(stdout_exact),
        std::move(stderr_exact),
        normalized_newlines};
}

}  // namespace

bool RegisterGoldenExpectation(
    std::vector<GoldenCliExpectation>& expectations,
    GoldenCliExpectation expectation) {
    if (!IsValidCommand(expectation.command)) {
        return false;
    }

    const bool duplicate = std::any_of(
        expectations.begin(),
        expectations.end(),
        [&expectation](const GoldenCliExpectation& existing) {
            return existing.command == expectation.command;
        });

    if (duplicate) {
        return false;
    }

    expectations.push_back(std::move(expectation));
    return true;
}

std::vector<GoldenCliExpectation> LoadGoldenExpectations() {
    std::vector<GoldenCliExpectation> expectations;

    const bool self_check_registered = RegisterGoldenExpectation(
        expectations,
        MakeExpectation(
            "--foundation-self-check",
            0,
            "",
            ""));

    const bool extension_registered = RegisterGoldenExpectation(
        expectations,
        MakeExpectation(
            "--foundation-extension-self-test",
            0,
            "foundation_extensions_self_test=1\n",
            ""));

    const bool unknown_registered = RegisterGoldenExpectation(
        expectations,
        MakeExpectation(
            "--unknown-command",
            64,
            "",
            "unsupported command\n"));

    const bool wrong_arity_registered = RegisterGoldenExpectation(
        expectations,
        MakeExpectation(
            "--wrong-arity",
            64,
            "",
            ""));

    if (!self_check_registered ||
        !extension_registered ||
        !unknown_registered ||
        !wrong_arity_registered) {
        return {};
    }

    return expectations;
}

std::optional<GoldenCliExpectation> FindGoldenExpectation(
    const std::vector<GoldenCliExpectation>& expectations,
    const std::string& command) {
    const auto iterator = std::find_if(
        expectations.begin(),
        expectations.end(),
        [&command](const GoldenCliExpectation& expectation) {
            return expectation.command == command;
        });

    if (iterator == expectations.end()) {
        return std::nullopt;
    }

    return *iterator;
}

GoldenCliComparison CompareObservedToGolden(
    const GoldenCliExpectation& expectation,
    const GoldenCliObservation& observation) {
    GoldenCliComparison result{};
    result.expected_command = expectation.command;
    result.expected_exit_code = expectation.exit_code;
    result.expected_stdout = expectation.stdout_exact;
    result.expected_stderr = expectation.stderr_exact;
    result.observed_command = observation.command;
    result.observed_exit_code = observation.exit_code;
    result.observed_stdout = observation.stdout_observed;
    result.observed_stderr = observation.stderr_observed;

    result.command_matches = expectation.command == observation.command;
    result.exit_code_matches = expectation.exit_code == observation.exit_code;
    result.stdout_matches = StreamsMatch(
        expectation.stdout_exact,
        observation.stdout_observed,
        expectation.normalized_newlines);
    result.stderr_matches = StreamsMatch(
        expectation.stderr_exact,
        observation.stderr_observed,
        expectation.normalized_newlines);

    result.matches = result.command_matches &&
                     result.exit_code_matches &&
                     result.stdout_matches &&
                     result.stderr_matches;
    return result;
}

std::string ExplainGoldenMismatch(const GoldenCliComparison& comparison) {
    if (comparison.matches) {
        return "golden_cli_comparison=match";
    }

    std::ostringstream output;
    output << "golden_cli_comparison=mismatch";

    if (!comparison.command_matches) {
        output << "\ncommand: expected=[" << comparison.expected_command
               << "], observed=[" << comparison.observed_command << ']';
    }

    if (!comparison.exit_code_matches) {
        output << "\nexit_code: expected=" << comparison.expected_exit_code
               << ", observed=" << comparison.observed_exit_code;
    }

    if (!comparison.stdout_matches) {
        output << "\nstdout: expected_bytes="
               << comparison.expected_stdout.size()
               << ", observed_bytes="
               << comparison.observed_stdout.size();
    }

    if (!comparison.stderr_matches) {
        output << "\nstderr: expected_bytes="
               << comparison.expected_stderr.size()
               << ", observed_bytes="
               << comparison.observed_stderr.size();
    }

    return output.str();
}

bool FoundationGoldenTestsSelfTest() {
    const std::vector<GoldenCliExpectation> expectations =
        LoadGoldenExpectations();

    if (expectations.size() != 4U) {
        return false;
    }

    const std::optional<GoldenCliExpectation> extension =
        FindGoldenExpectation(
            expectations,
            "--foundation-extension-self-test");

    if (!extension.has_value()) {
        return false;
    }

    const GoldenCliObservation matching_observation{
        "--foundation-extension-self-test",
        0,
        "foundation_extensions_self_test=1\n",
        ""};

    const GoldenCliComparison matching_comparison =
        CompareObservedToGolden(*extension, matching_observation);

    if (!matching_comparison.matches ||
        ExplainGoldenMismatch(matching_comparison) !=
            "golden_cli_comparison=match") {
        return false;
    }

    const GoldenCliObservation mismatching_observation{
        "--foundation-extension-self-test",
        2,
        "foundation_extensions_self_test=0\n",
        ""};

    const GoldenCliComparison mismatch =
        CompareObservedToGolden(*extension, mismatching_observation);

    if (mismatch.matches ||
        mismatch.exit_code_matches ||
        mismatch.stdout_matches) {
        return false;
    }

    const std::string mismatch_detail = ExplainGoldenMismatch(mismatch);
    if (mismatch_detail.find("exit_code: expected=0, observed=2") ==
            std::string::npos ||
        mismatch_detail.find("stdout: expected_bytes=34, observed_bytes=34") ==
            std::string::npos) {
        return false;
    }

    std::vector<GoldenCliExpectation> registry;
    if (!RegisterGoldenExpectation(
            registry,
            MakeExpectation("--probe", 0, "ok\n", "")) ||
        RegisterGoldenExpectation(
            registry,
            MakeExpectation("--probe", 0, "changed\n", "")) ||
        RegisterGoldenExpectation(
            registry,
            MakeExpectation("probe", 0, "ok\n", ""))) {
        return false;
    }

    const GoldenCliExpectation newline_expected =
        MakeExpectation("--newline-probe", 0, "line\n", "", true);
    const GoldenCliObservation newline_observed{
        "--newline-probe",
        0,
        "line\r\n",
        ""};

    return CompareObservedToGolden(
               newline_expected,
               newline_observed).matches;
}
