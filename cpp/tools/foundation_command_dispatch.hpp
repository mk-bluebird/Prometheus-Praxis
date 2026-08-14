// File: cpp/tools/foundation_command_dispatch.hpp
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace prometheus_praxis::foundation::dispatch {

enum class FoundationExitCode : int {
    Success = 0,
    RuntimeFailure = 1,
    SafetyBlocked = 2,
    InvalidUsage = 64
};

struct FoundationCommandDispatchResult {
    bool recognized{};
    bool executed{};
    int platform_exit_code{static_cast<int>(FoundationExitCode::RuntimeFailure)};
    std::string command;
    std::string detail;
};

using FoundationCommandHandler =
    std::function<FoundationCommandDispatchResult(std::string_view)>;

struct FoundationCommandRecord {
    std::string command;
    std::string summary;
    FoundationCommandHandler handler;
    FoundationExitCode success_exit_code{FoundationExitCode::Success};
};

int ToPlatformExitCode(FoundationExitCode code) noexcept;

FoundationExitCode FoundationExitCodeFromPlatform(int code) noexcept;

bool IsValidFoundationExitCode(int code) noexcept;

std::string_view FoundationExitCodeName(FoundationExitCode code) noexcept;

bool IsFoundationCommandNameValid(std::string_view command) noexcept;

bool IsFoundationCommandRegistryValid(
    const std::vector<FoundationCommandRecord>& registry) noexcept;

bool IsFoundationCommandDispatchResultValid(
    const FoundationCommandDispatchResult& result) noexcept;

std::optional<FoundationCommandRecord> FindUnifiedFoundationCommand(
    const std::vector<FoundationCommandRecord>& registry,
    std::string_view command);

std::vector<FoundationCommandRecord> BuildUnifiedFoundationCommandRegistry();

FoundationCommandDispatchResult DispatchUnifiedFoundationCommand(
    const std::vector<FoundationCommandRecord>& registry,
    std::string_view command);

FoundationCommandDispatchResult RunFoundationExtensionSelfTestCommand(
    std::string_view command);

FoundationCommandDispatchResult RunFoundationAllSelfTestsCommand(
    std::string_view command);

bool FoundationCommandDispatcherSelfTest();

}  // namespace prometheus_praxis::foundation::dispatch
