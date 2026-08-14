// File: cpp/tools/foundation_command_dispatch.cpp
#include "foundation_command_dispatch.hpp"

#include "foundation_extension_registry.hpp"
#include "foundation_report.hpp"

#include <algorithm>
#include <exception>
#include <functional>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

bool IsCommandNameValid(const std::string& command) noexcept {
    return command.size() > 2U &&
           command[0] == '-' &&
           command[1] == '-';
}

bool ContainsDuplicateCommand(
    const std::vector<FoundationCommandRecord>& registry,
    const std::string& command) {
    return std::count_if(
               registry.begin(),
               registry.end(),
               [&command](const FoundationCommandRecord& record) {
                   return record.command == command;
               }) > 1;
}

FoundationCommandDispatchResult MakeResult(
    const bool recognized,
    const bool executed,
    const FoundationExitCode exit_code,
    std::string command,
    std::string detail) {
    return FoundationCommandDispatchResult{
        recognized,
        executed,
        ToPlatformExitCode(exit_code),
        std::move(command),
        std::move(detail)};
}

FoundationCommandDispatchResult SuccessProbe(const std::string& command) {
    return MakeResult(
        true,
        true,
        FoundationExitCode::Success,
        command,
        "probe_success=1\n");
}

FoundationCommandDispatchResult SafetyBlockedProbe(const std::string& command) {
    return MakeResult(
        true,
        true,
        FoundationExitCode::SafetyBlocked,
        command,
        "probe_safety_blocked=1\n");
}

FoundationCommandDispatchResult ThrowingProbe(const std::string&) {
    throw std::runtime_error("controlled dispatcher self-test exception");
}

}  // namespace

int ToPlatformExitCode(const FoundationExitCode code) noexcept {
    return static_cast<int>(code);
}

std::optional<FoundationCommandRecord> FindUnifiedFoundationCommand(
    const std::vector<FoundationCommandRecord>& registry,
    const std::string& command) {
    const auto iterator = std::find_if(
        registry.begin(),
        registry.end(),
        [&command](const FoundationCommandRecord& record) {
            return record.command == command;
        });

    if (iterator == registry.end()) {
        return std::nullopt;
    }

    if (!IsCommandNameValid(iterator->command) ||
        iterator->summary.empty() ||
        !iterator->handler ||
        ContainsDuplicateCommand(registry, iterator->command)) {
        return std::nullopt;
    }

    return *iterator;
}

std::vector<FoundationCommandRecord> BuildUnifiedFoundationCommandRegistry() {
    std::vector<FoundationCommandRecord> registry;
    registry.push_back(
        FoundationCommandRecord{
            "--foundation-extension-self-test",
            "Runs all registered canonical diagnostic extension self-tests.",
            RunFoundationExtensionSelfTestCommand,
            FoundationExitCode::Success});
    registry.push_back(
        FoundationCommandRecord{
            "--foundation-all-self-tests",
            "Runs foundation-report and canonical-extension diagnostic tests.",
            RunFoundationAllSelfTestsCommand,
            FoundationExitCode::Success});
    return registry;
}

FoundationCommandDispatchResult DispatchUnifiedFoundationCommand(
    const std::vector<FoundationCommandRecord>& registry,
    const std::string& command) {
    if (!IsCommandNameValid(command)) {
        return MakeResult(
            false,
            false,
            FoundationExitCode::InvalidUsage,
            command,
            "unsupported command\n");
    }

    const std::optional<FoundationCommandRecord> record =
        FindUnifiedFoundationCommand(registry, command);

    if (!record.has_value()) {
        return MakeResult(
            false,
            false,
            FoundationExitCode::InvalidUsage,
            command,
            "unsupported command\n");
    }

    try {
        FoundationCommandDispatchResult result = record->handler(command);

        if (!result.recognized) {
            result.recognized = true;
        }
        if (result.command.empty()) {
            result.command = command;
        }

        if (!IsFoundationCommandDispatchResultValid(result)) {
            return MakeResult(
                true,
                true,
                FoundationExitCode::RuntimeFailure,
                command,
                "foundation command returned an invalid dispatch result\n");
        }

        return result;
    } catch (const std::exception&) {
        return MakeResult(
            true,
            true,
            FoundationExitCode::RuntimeFailure,
            command,
            "foundation command runtime failure\n");
    } catch (...) {
        return MakeResult(
            true,
            true,
            FoundationExitCode::RuntimeFailure,
            command,
            "foundation command runtime failure\n");
    }
}

bool IsFoundationCommandDispatchResultValid(
    const FoundationCommandDispatchResult& result) noexcept {
    if (result.command.empty()) {
        return false;
    }

    if (!result.recognized && result.executed) {
        return false;
    }

    if (!result.recognized &&
        result.platform_exit_code !=
            ToPlatformExitCode(FoundationExitCode::InvalidUsage)) {
        return false;
    }

    if (result.platform_exit_code !=
            ToPlatformExitCode(FoundationExitCode::Success) &&
        result.platform_exit_code !=
            ToPlatformExitCode(FoundationExitCode::RuntimeFailure) &&
        result.platform_exit_code !=
            ToPlatformExitCode(FoundationExitCode::SafetyBlocked) &&
        result.platform_exit_code !=
            ToPlatformExitCode(FoundationExitCode::InvalidUsage)) {
        return false;
    }

    return true;
}

FoundationCommandDispatchResult RunFoundationExtensionSelfTestCommand(
    const std::string& command) {
    const CanonicalExtensionRegistry registry = BuildKnownExtensionRegistry();
    const std::vector<CanonicalExtensionRunResult> results =
        RunCanonicalExtensionSelfTests(registry);
    const bool passed = AllCanonicalExtensionSelfTestsPassed(results);

    return MakeResult(
        true,
        true,
        passed ? FoundationExitCode::Success : FoundationExitCode::SafetyBlocked,
        command,
        std::string("foundation_extensions_self_test=") +
            (passed ? "1\n" : "0\n"));
}

FoundationCommandDispatchResult RunFoundationAllSelfTestsCommand(
    const std::string& command) {
    const bool report_passed = FoundationReportValidatorSelfTest();
    const bool registry_passed = CanonicalExtensionRegistrySelfTest();

    const CanonicalExtensionRegistry registry = BuildKnownExtensionRegistry();
    const bool extensions_passed = AllCanonicalExtensionSelfTestsPassed(
        RunCanonicalExtensionSelfTests(registry));

    const bool passed = report_passed && registry_passed && extensions_passed;
    std::ostringstream output;
    output << "foundation_report_validator_self_test="
           << (report_passed ? "1" : "0") << '\n'
           << "canonical_extension_registry_self_test="
           << (registry_passed ? "1" : "0") << '\n'
           << "foundation_extensions_self_test="
           << (extensions_passed ? "1" : "0") << '\n';

    return MakeResult(
        true,
        true,
        passed ? FoundationExitCode::Success : FoundationExitCode::SafetyBlocked,
        command,
        output.str());
}

bool FoundationCommandDispatcherSelfTest() {
    const std::vector<FoundationCommandRecord> registry{
        FoundationCommandRecord{
            "--probe-success",
            "Runs a successful dispatcher probe.",
            SuccessProbe,
            FoundationExitCode::Success},
        FoundationCommandRecord{
            "--probe-safety-blocked",
            "Runs a safety-blocked dispatcher probe.",
            SafetyBlockedProbe,
            FoundationExitCode::SafetyBlocked},
        FoundationCommandRecord{
            "--probe-throws",
            "Runs a throwing dispatcher probe.",
            ThrowingProbe,
            FoundationExitCode::Success}};

    const FoundationCommandDispatchResult success =
        DispatchUnifiedFoundationCommand(registry, "--probe-success");
    if (!success.recognized || !success.executed ||
        success.platform_exit_code !=
            ToPlatformExitCode(FoundationExitCode::Success) ||
        success.detail != "probe_success=1\n") {
        return false;
    }

    const FoundationCommandDispatchResult blocked =
        DispatchUnifiedFoundationCommand(registry, "--probe-safety-blocked");
    if (!blocked.recognized || !blocked.executed ||
        blocked.platform_exit_code !=
            ToPlatformExitCode(FoundationExitCode::SafetyBlocked)) {
        return false;
    }

    const FoundationCommandDispatchResult unknown =
        DispatchUnifiedFoundationCommand(registry, "--unknown");
    if (unknown.recognized || unknown.executed ||
        unknown.platform_exit_code !=
            ToPlatformExitCode(FoundationExitCode::InvalidUsage) ||
        unknown.detail != "unsupported command\n") {
        return false;
    }

    const FoundationCommandDispatchResult throwing =
        DispatchUnifiedFoundationCommand(registry, "--probe-throws");
    if (!throwing.recognized || !throwing.executed ||
        throwing.platform_exit_code !=
            ToPlatformExitCode(FoundationExitCode::RuntimeFailure)) {
        return false;
    }

    const std::vector<FoundationCommandRecord> duplicates{
        FoundationCommandRecord{
            "--duplicate",
            "First duplicate record.",
            SuccessProbe,
            FoundationExitCode::Success},
        FoundationCommandRecord{
            "--duplicate",
            "Second duplicate record.",
            SuccessProbe,
            FoundationExitCode::Success}};

    if (FindUnifiedFoundationCommand(duplicates, "--duplicate").has_value()) {
        return false;
    }

    const FoundationCommandDispatchResult duplicate_result =
        DispatchUnifiedFoundationCommand(duplicates, "--duplicate");
    if (duplicate_result.recognized || duplicate_result.executed ||
        duplicate_result.platform_exit_code !=
            ToPlatformExitCode(FoundationExitCode::InvalidUsage)) {
        return false;
    }

    return true;
}
