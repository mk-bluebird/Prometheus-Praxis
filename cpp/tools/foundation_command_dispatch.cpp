// File: cpp/tools/foundation_command_dispatch.cpp
#include "foundation_command_dispatch.hpp"

#include "foundation_extension_registry.hpp"
#include "foundation_report.hpp"

#include <algorithm>
#include <exception>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace prometheus_praxis::foundation::dispatch {
namespace {

bool BeginsWithDoubleDash(std::string_view command) noexcept {
    return command.size() > 2U &&
           command[0] == '-' &&
           command[1] == '-';
}

FoundationCommandDispatchResult MakeResult(
    bool recognized,
    bool executed,
    FoundationExitCode exit_code,
    std::string command,
    std::string detail) {
    return FoundationCommandDispatchResult{
        recognized,
        executed,
        ToPlatformExitCode(exit_code),
        std::move(command),
        std::move(detail)};
}

bool IsRegistryValid(
    const std::vector<FoundationCommandRecord>& registry) {
    if (registry.empty()) {
        return false;
    }

    std::set<std::string> names;
    for (const FoundationCommandRecord& record : registry) {
        if (!BeginsWithDoubleDash(record.command) ||
            record.summary.empty() ||
            !record.handler ||
            !IsValidFoundationExitCode(
                ToPlatformExitCode(record.success_exit_code)) ||
            !names.insert(record.command).second) {
            return false;
        }
    }

    return true;
}

FoundationCommandDispatchResult RunExtensionRegistry(
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
        std::string("foundation_extension_self_test=") +
            (passed ? "passed\n" : "failed\n"));
}

FoundationCommandDispatchResult RunAllSelfTests(
    const std::string& command) {
    const bool report_validator_passed = FoundationReportValidatorSelfTest();
    const bool registry_validator_passed = CanonicalExtensionRegistrySelfTest();

    const CanonicalExtensionRegistry registry = BuildKnownExtensionRegistry();
    const std::vector<CanonicalExtensionRunResult> extension_results =
        RunCanonicalExtensionSelfTests(registry);
    const bool extensions_passed =
        AllCanonicalExtensionSelfTestsPassed(extension_results);

    const bool passed =
        report_validator_passed &&
        registry_validator_passed &&
        extensions_passed;

    std::ostringstream detail;
    detail << "foundation_report_validator_self_test="
           << (report_validator_passed ? "passed" : "failed") << '\n'
           << "canonical_extension_registry_self_test="
           << (registry_validator_passed ? "passed" : "failed") << '\n'
           << "foundation_extension_self_test="
           << (extensions_passed ? "passed" : "failed") << '\n';

    return MakeResult(
        true,
        true,
        passed ? FoundationExitCode::Success : FoundationExitCode::SafetyBlocked,
        command,
        detail.str());
}

FoundationCommandDispatchResult SuccessfulHandler(const std::string& command) {
    return MakeResult(
        true,
        true,
        FoundationExitCode::Success,
        command,
        "success\n");
}

FoundationCommandDispatchResult SafetyBlockedHandler(
    const std::string& command) {
    return MakeResult(
        true,
        true,
        FoundationExitCode::SafetyBlocked,
        command,
        "safety_blocked\n");
}

FoundationCommandDispatchResult ThrowingHandler(const std::string&) {
    throw std::runtime_error("controlled dispatcher self-test exception");
}

}  // namespace

int ToPlatformExitCode(FoundationExitCode code) noexcept {
    return static_cast<int>(code);
}

FoundationExitCode FoundationExitCodeFromPlatform(int code) noexcept {
    switch (code) {
        case static_cast<int>(FoundationExitCode::Success):
            return FoundationExitCode::Success;
        case static_cast<int>(FoundationExitCode::RuntimeFailure):
            return FoundationExitCode::RuntimeFailure;
        case static_cast<int>(FoundationExitCode::SafetyBlocked):
            return FoundationExitCode::SafetyBlocked;
        case static_cast<int>(FoundationExitCode::InvalidUsage):
            return FoundationExitCode::InvalidUsage;
        default:
            return FoundationExitCode::RuntimeFailure;
    }
}

bool IsValidFoundationExitCode(int code) noexcept {
    return code == ToPlatformExitCode(FoundationExitCode::Success) ||
           code == ToPlatformExitCode(FoundationExitCode::RuntimeFailure) ||
           code == ToPlatformExitCode(FoundationExitCode::SafetyBlocked) ||
           code == ToPlatformExitCode(FoundationExitCode::InvalidUsage);
}

std::string_view FoundationExitCodeName(FoundationExitCode code) noexcept {
    switch (code) {
        case FoundationExitCode::Success:
            return "success";
        case FoundationExitCode::RuntimeFailure:
            return "runtime_failure";
        case FoundationExitCode::SafetyBlocked:
            return "safety_blocked";
        case FoundationExitCode::InvalidUsage:
            return "invalid_usage";
    }

    return "runtime_failure";
}

std::optional<FoundationCommandRecord> FindUnifiedFoundationCommand(
    const std::vector<FoundationCommandRecord>& registry,
    std::string_view command) {
    if (!IsRegistryValid(registry) || !BeginsWithDoubleDash(command)) {
        return std::nullopt;
    }

    const auto found = std::find_if(
        registry.begin(),
        registry.end(),
        [command](const FoundationCommandRecord& record) {
            return record.command == command;
        });

    if (found == registry.end()) {
        return std::nullopt;
    }

    return *found;
}

std::vector<FoundationCommandRecord> BuildUnifiedFoundationCommandRegistry() {
    return {
        FoundationCommandRecord{
            "--foundation-extension-self-test",
            "Run registered foundation diagnostic self-tests.",
            [](const std::string& command) {
                return RunFoundationExtensionSelfTestCommand(command);
            },
            FoundationExitCode::Success},
        FoundationCommandRecord{
            "--foundation-all-self-tests",
            "Run foundation report and extension diagnostic self-tests.",
            [](const std::string& command) {
                return RunFoundationAllSelfTestsCommand(command);
            },
            FoundationExitCode::Success}};
}

FoundationCommandDispatchResult DispatchUnifiedFoundationCommand(
    const std::vector<FoundationCommandRecord>& registry,
    std::string_view command) {
    if (!IsRegistryValid(registry)) {
        return MakeResult(
            false,
            false,
            FoundationExitCode::RuntimeFailure,
            std::string(command),
            "invalid command registry\n");
    }

    if (!BeginsWithDoubleDash(command)) {
        return MakeResult(
            false,
            false,
            FoundationExitCode::InvalidUsage,
            std::string(command),
            "unsupported command\n");
    }

    const std::optional<FoundationCommandRecord> record =
        FindUnifiedFoundationCommand(registry, command);
    if (!record.has_value()) {
        return MakeResult(
            false,
            false,
            FoundationExitCode::InvalidUsage,
            std::string(command),
            "unsupported command\n");
    }

    try {
        FoundationCommandDispatchResult result =
            record->handler(record->command);

        result.recognized = true;
        result.command = record->command;

        if (!IsValidFoundationExitCode(result.platform_exit_code)) {
            return MakeResult(
                true,
                false,
                FoundationExitCode::RuntimeFailure,
                record->command,
                "handler returned invalid platform exit code\n");
        }

        return result;
    } catch (const std::exception& exception) {
        return MakeResult(
            true,
            false,
            FoundationExitCode::RuntimeFailure,
            record->command,
            std::string("command exception: ") + exception.what() + '\n');
    } catch (...) {
        return MakeResult(
            true,
            false,
            FoundationExitCode::RuntimeFailure,
            record->command,
            "command exception: non_standard_exception\n");
    }
}

FoundationCommandDispatchResult RunFoundationExtensionSelfTestCommand(
    std::string_view command) {
    return RunExtensionRegistry(std::string(command));
}

FoundationCommandDispatchResult RunFoundationAllSelfTestsCommand(
    std::string_view command) {
    return RunAllSelfTests(std::string(command));
}

bool FoundationCommandDispatcherSelfTest() {
    const std::vector<FoundationExitCode> exit_codes{
        FoundationExitCode::Success,
        FoundationExitCode::RuntimeFailure,
        FoundationExitCode::SafetyBlocked,
        FoundationExitCode::InvalidUsage};

    for (const FoundationExitCode code : exit_codes) {
        const int platform_code = ToPlatformExitCode(code);
        if (!IsValidFoundationExitCode(platform_code) ||
            FoundationExitCodeFromPlatform(platform_code) != code) {
            return false;
        }
    }

    if (IsValidFoundationExitCode(3) ||
        FoundationExitCodeFromPlatform(3) != FoundationExitCode::RuntimeFailure ||
        FoundationExitCodeName(FoundationExitCode::InvalidUsage) !=
            "invalid_usage") {
        return false;
    }

    const std::vector<FoundationCommandRecord> registry{
        FoundationCommandRecord{
            "--successful-command",
            "Successful dispatcher self-test handler.",
            &SuccessfulHandler,
            FoundationExitCode::Success},
        FoundationCommandRecord{
            "--safety-blocked-command",
            "Safety-blocked dispatcher self-test handler.",
            &SafetyBlockedHandler,
            FoundationExitCode::SafetyBlocked},
        FoundationCommandRecord{
            "--throwing-command",
            "Throwing dispatcher self-test handler.",
            &ThrowingHandler,
            FoundationExitCode::RuntimeFailure}};

    const FoundationCommandDispatchResult success =
        DispatchUnifiedFoundationCommand(registry, "--successful-command");
    if (!success.recognized ||
        !success.executed ||
        success.platform_exit_code !=
            ToPlatformExitCode(FoundationExitCode::Success) ||
        success.detail != "success\n") {
        return false;
    }

    const FoundationCommandDispatchResult blocked =
        DispatchUnifiedFoundationCommand(registry, "--safety-blocked-command");
    if (!blocked.recognized ||
        !blocked.executed ||
        blocked.platform_exit_code !=
            ToPlatformExitCode(FoundationExitCode::SafetyBlocked) ||
        blocked.detail != "safety_blocked\n") {
        return false;
    }

    const FoundationCommandDispatchResult unknown =
        DispatchUnifiedFoundationCommand(registry, "--unknown-command");
    if (unknown.recognized ||
        unknown.executed ||
        unknown.platform_exit_code !=
            ToPlatformExitCode(FoundationExitCode::InvalidUsage) ||
        unknown.detail != "unsupported command\n") {
        return false;
    }

    const FoundationCommandDispatchResult malformed_name =
        DispatchUnifiedFoundationCommand(registry, "unknown-command");
    if (malformed_name.recognized ||
        malformed_name.executed ||
        malformed_name.platform_exit_code !=
            ToPlatformExitCode(FoundationExitCode::InvalidUsage) ||
        malformed_name.detail != "unsupported command\n") {
        return false;
    }

    const FoundationCommandDispatchResult throwing =
        DispatchUnifiedFoundationCommand(registry, "--throwing-command");
    if (!throwing.recognized ||
        throwing.executed ||
        throwing.platform_exit_code !=
            ToPlatformExitCode(FoundationExitCode::RuntimeFailure) ||
        throwing.detail.find("command exception: ") != 0U) {
        return false;
    }

    std::vector<FoundationCommandRecord> duplicate = registry;
    duplicate.push_back(registry.front());
    const FoundationCommandDispatchResult invalid_registry =
        DispatchUnifiedFoundationCommand(duplicate, "--successful-command");
    if (invalid_registry.recognized ||
        invalid_registry.executed ||
        invalid_registry.platform_exit_code !=
            ToPlatformExitCode(FoundationExitCode::RuntimeFailure) ||
        invalid_registry.detail != "invalid command registry\n") {
        return false;
    }

    if (!FindUnifiedFoundationCommand(
            registry,
            "--successful-command").has_value() ||
        FindUnifiedFoundationCommand(
            duplicate,
            "--successful-command").has_value()) {
        return false;
    }

    return true;
}

}  // namespace prometheus_praxis::foundation::dispatch
