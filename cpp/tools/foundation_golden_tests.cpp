// File: cpp/tools/foundation_golden_tests.cpp
#include "foundation_golden_tests.hpp"

#include <algorithm>
#include <cstddef>
#include <iomanip>
#include <sstream>
#include <utility>

namespace prometheus_praxis::foundation::golden {
namespace {

bool EndsWithNewline(std::string_view bytes) noexcept {
    return !bytes.empty() && bytes.back() == '\n';
}

bool IsValidCommand(std::string_view command) noexcept {
    return command.size() > 2U &&
           command[0] == '-' &&
           command[1] == '-';
}

std::string QuoteBytes(std::string_view value) {
    std::ostringstream output;
    output << '"';

    for (const unsigned char byte : value) {
        switch (byte) {
            case '\\':
                output << "\\\\";
                break;
            case '"':
                output << "\\\"";
                break;
            case '\n':
                output << "\\n";
                break;
            case '\r':
                output << "\\r";
                break;
            case '\t':
                output << "\\t";
                break;
            case '\0':
                output << "\\0";
                break;
            default:
                if (byte >= 32U && byte <= 126U) {
                    output << static_cast<char>(byte);
                } else {
                    output << "\\x" << std::hex << std::uppercase
                           << std::setw(2) << std::setfill('0')
                           << static_cast<unsigned int>(byte)
                           << std::dec << std::nouppercase
                           << std::setfill(' ');
                }
                break;
        }
    }

    output << '"';
    return output.str();
}

std::string DescribeByteDifference(
    std::string_view field,
    std::string_view expected,
    std::string_view observed) {
    const std::size_t shared_length = std::min(expected.size(), observed.size());
    std::size_t offset = 0U;

    while (offset < shared_length && expected[offset] == observed[offset]) {
        ++offset;
    }

    std::ostringstream reason;
    reason << field << " byte mismatch"
           << "; expected_bytes=" << expected.size()
           << "; observed_bytes=" << observed.size()
           << "; first_differing_offset=" << offset;

    if (offset < expected.size()) {
        reason << "; expected_byte="
               << static_cast<unsigned int>(
                      static_cast<unsigned char>(expected[offset]));
    } else {
        reason << "; expected_byte=<end>";
    }

    if (offset < observed.size()) {
        reason << "; observed_byte="
               << static_cast<unsigned int>(
                      static_cast<unsigned char>(observed[offset]));
    } else {
        reason << "; observed_byte=<end>";
    }

    return reason.str();
}

void AppendReason(GoldenCliComparison& comparison, std::string reason) {
    comparison.reasons.push_back(std::move(reason));
}

bool IsExpectationWellFormed(const GoldenCliExpectation& expectation) {
    return IsValidCommand(expectation.command);
}

}  // namespace

bool RegisterGoldenExpectation(
    std::vector<GoldenCliExpectation>& expectations,
    GoldenCliExpectation expectation) {
    if (!IsExpectationWellFormed(expectation)) {
        return false;
    }

    const auto duplicate = std::find_if(
        expectations.begin(),
        expectations.end(),
        [&expectation](const GoldenCliExpectation& existing) {
            return existing.command == expectation.command;
        });

    if (duplicate != expectations.end()) {
        return false;
    }

    expectations.push_back(std::move(expectation));
    return true;
}

std::vector<GoldenCliExpectation> LoadGoldenExpectations() {
    std::vector<GoldenCliExpectation> expectations;
    expectations.reserve(3U);

    const bool self_check_registered = RegisterGoldenExpectation(
        expectations,
        GoldenCliExpectation{
            "--foundation-self-check",
            0,
            "",
            "",
            true,
            false,
            true,
            true});

    const bool extension_registered = RegisterGoldenExpectation(
        expectations,
        GoldenCliExpectation{
            "--foundation-extension-self-test",
            0,
            "foundation_extensions_self_test=1\n",
            "",
            true,
            false,
            true,
            true});

    const bool unknown_registered = RegisterGoldenExpectation(
        expectations,
        GoldenCliExpectation{
            "--unknown-command",
            64,
            "",
            "unsupported command\n",
            false,
            true,
            true,
            true});

    if (!self_check_registered ||
        !extension_registered ||
        !unknown_registered) {
        return {};
    }

    return expectations;
}

std::optional<GoldenCliExpectation> FindGoldenExpectation(
    const std::vector<GoldenCliExpectation>& expectations,
    std::string_view command) {
    const auto found = std::find_if(
        expectations.begin(),
        expectations.end(),
        [command](const GoldenCliExpectation& expectation) {
            return expectation.command == command;
        });

    if (found == expectations.end()) {
        return std::nullopt;
    }

    return *found;
}

GoldenCliComparison CompareObservedToGolden(
    const GoldenCliExpectation& expectation,
    const GoldenCliObservation& observation) {
    GoldenCliComparison comparison;
    comparison.expected_command = expectation.command;
    comparison.expected_exit_code = expectation.exit_code;
    comparison.expected_stdout = expectation.stdout_exact;
    comparison.expected_stderr = expectation.stderr_exact;
    comparison.observed_command = observation.command;
    comparison.observed_exit_code = observation.exit_code;
    comparison.observed_stdout = observation.stdout_observed;
    comparison.observed_stderr = observation.stderr_observed;

    comparison.command_matches = expectation.command == observation.command;
    comparison.exit_code_matches = expectation.exit_code == observation.exit_code;
    comparison.stdout_matches = true;
    comparison.stderr_matches = true;

    if (!comparison.command_matches) {
        AppendReason(
            comparison,
            "command mismatch; expected=" + QuoteBytes(expectation.command) +
                "; observed=" + QuoteBytes(observation.command));
    }

    if (!comparison.exit_code_matches) {
        AppendReason(
            comparison,
            "exit code mismatch; expected=" +
                std::to_string(expectation.exit_code) +
                "; observed=" + std::to_string(observation.exit_code));
    }

    if (expectation.compare_stdout_bytes &&
        expectation.stdout_exact != observation.stdout_observed) {
        comparison.stdout_matches = false;
        AppendReason(
            comparison,
            DescribeByteDifference(
                "stdout",
                expectation.stdout_exact,
                observation.stdout_observed));
    }

    if (expectation.requires_stdout_newline &&
        !EndsWithNewline(observation.stdout_observed)) {
        comparison.stdout_matches = false;
        AppendReason(
            comparison,
            "stdout newline mismatch; expected terminal newline; observed=false");
    }

    if (expectation.compare_stderr_bytes &&
        expectation.stderr_exact != observation.stderr_observed) {
        comparison.stderr_matches = false;
        AppendReason(
            comparison,
            DescribeByteDifference(
                "stderr",
                expectation.stderr_exact,
                observation.stderr_observed));
    }

    if (expectation.requires_stderr_newline &&
        !EndsWithNewline(observation.stderr_observed)) {
        comparison.stderr_matches = false;
        AppendReason(
            comparison,
            "stderr newline mismatch; expected terminal newline; observed=false");
    }

    comparison.matches =
        comparison.command_matches &&
        comparison.exit_code_matches &&
        comparison.stdout_matches &&
        comparison.stderr_matches;
    return comparison;
}

std::string ExplainGoldenMismatch(const GoldenCliComparison& comparison) {
    if (comparison.matches) {
        return "golden comparison matched";
    }

    std::ostringstream explanation;
    explanation << "golden comparison failed"
                << "; expected_command=" << QuoteBytes(comparison.expected_command)
                << "; observed_command=" << QuoteBytes(comparison.observed_command)
                << "; expected_exit_code=" << comparison.expected_exit_code
                << "; observed_exit_code=" << comparison.observed_exit_code
                << "; expected_stdout_bytes=" << comparison.expected_stdout.size()
                << "; observed_stdout_bytes=" << comparison.observed_stdout.size()
                << "; expected_stderr_bytes=" << comparison.expected_stderr.size()
                << "; observed_stderr_bytes=" << comparison.observed_stderr.size();

    for (const std::string& reason : comparison.reasons) {
        explanation << "; reason=" << reason;
    }

    return explanation.str();
}

bool IsGoldenComparisonCorrect(const GoldenCliComparison& comparison) {
    const bool all_fields_match =
        comparison.command_matches &&
        comparison.exit_code_matches &&
        comparison.stdout_matches &&
        comparison.stderr_matches;

    return comparison.matches == all_fields_match &&
           (comparison.matches ? comparison.reasons.empty()
                               : !comparison.reasons.empty());
}

bool FoundationGoldenTestsSelfTest() {
    const GoldenCliExpectation expected{
        "--foundation-self-check",
        0,
        "{\"status\":\"safe\"}\n",
        "",
        true,
        false,
        true,
        true};

    std::vector<GoldenCliExpectation> expectations;
    if (!RegisterGoldenExpectation(expectations, expected) ||
        RegisterGoldenExpectation(expectations, expected) ||
        RegisterGoldenExpectation(
            expectations,
            GoldenCliExpectation{
                "foundation-invalid-command",
                0,
                "",
                "",
                false,
                false,
                true,
                true})) {
        return false;
    }

    const auto found = FindGoldenExpectation(
        expectations,
        "--foundation-self-check");
    if (!found.has_value() ||
        FindGoldenExpectation(expectations, "--missing-command").has_value()) {
        return false;
    }

    const GoldenCliObservation matching{
        "--foundation-self-check",
        0,
        "{\"status\":\"safe\"}\n",
        ""};
    const GoldenCliComparison matching_comparison =
        CompareObservedToGolden(*found, matching);
    if (!matching_comparison.matches ||
        !IsGoldenComparisonCorrect(matching_comparison)) {
        return false;
    }

    GoldenCliObservation wrong_exit = matching;
    wrong_exit.exit_code = 2;
    const GoldenCliComparison wrong_exit_comparison =
        CompareObservedToGolden(*found, wrong_exit);
    if (wrong_exit_comparison.matches ||
        wrong_exit_comparison.exit_code_matches ||
        !IsGoldenComparisonCorrect(wrong_exit_comparison)) {
        return false;
    }

    GoldenCliObservation wrong_stdout = matching;
    wrong_stdout.stdout_observed = "{\"status\":\"unsafe\"}\n";
    const GoldenCliComparison wrong_stdout_comparison =
        CompareObservedToGolden(*found, wrong_stdout);
    if (wrong_stdout_comparison.matches ||
        wrong_stdout_comparison.stdout_matches ||
        !IsGoldenComparisonCorrect(wrong_stdout_comparison)) {
        return false;
    }

    GoldenCliObservation wrong_stderr = matching;
    wrong_stderr.stderr_observed = "unexpected\n";
    const GoldenCliComparison wrong_stderr_comparison =
        CompareObservedToGolden(*found, wrong_stderr);
    if (wrong_stderr_comparison.matches ||
        wrong_stderr_comparison.stderr_matches ||
        !IsGoldenComparisonCorrect(wrong_stderr_comparison)) {
        return false;
    }

    GoldenCliObservation missing_newline = matching;
    missing_newline.stdout_observed = "{\"status\":\"safe\"}";
    const GoldenCliComparison missing_newline_comparison =
        CompareObservedToGolden(*found, missing_newline);
    if (missing_newline_comparison.matches ||
        missing_newline_comparison.stdout_matches ||
        !IsGoldenComparisonCorrect(missing_newline_comparison)) {
        return false;
    }

    const GoldenCliExpectation raw_line_ending_expectation{
        "--raw-line-ending-probe",
        0,
        "line\n",
        "",
        true,
        false,
        true,
        true};
    const GoldenCliObservation raw_line_ending_observation{
        "--raw-line-ending-probe",
        0,
        "line\r\n",
        ""};
    const GoldenCliComparison line_ending_comparison =
        CompareObservedToGolden(
            raw_line_ending_expectation,
            raw_line_ending_observation);
    if (line_ending_comparison.matches ||
        line_ending_comparison.stdout_matches ||
        !IsGoldenComparisonCorrect(line_ending_comparison)) {
        return false;
    }

    return ExplainGoldenMismatch(wrong_stdout_comparison).find(
               "first_differing_offset=") != std::string::npos;
}

}  // namespace prometheus_praxis::foundation::golden
