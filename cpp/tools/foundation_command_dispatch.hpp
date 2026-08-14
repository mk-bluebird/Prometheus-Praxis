// File: cpp/tools/foundation_command_dispatch.hpp
#pragma once

#include <functional>
#include <optional>
#include <string>
#include <vector>

enum class FoundationExitCode : int {
    Success = 0,
    RuntimeFailure = 1,
    SafetyBlocked = 2,
    InvalidUsage = 64
};

struct FoundationCommandDispatchResult {
    bool recognized{};
    bool executed{};
    int platform_exit_code{};
    std::string command;
    std::string detail;
};

using FoundationCommandHandler =
    std::function<FoundationCommandDispatchResult(const std::string&)>;

struct FoundationCommandRecord {
    std::string command;
    std::string summary;
    FoundationCommandHandler handler;
    FoundationExitCode success_exit_code{FoundationExitCode::Success};
};

int ToPlatformExitCode(FoundationExitCode code) noexcept;

std::optional<FoundationCommandRecord> FindUnifiedFoundationCommand(
    const std::vector<FoundationCommandRecord>& registry,
    const std::string& command);

std::vector<FoundationCommandRecord> BuildUnifiedFoundationCommandRegistry();

FoundationCommandDispatchResult DispatchUnifiedFoundationCommand(
    const std::vector<FoundationCommandRecord>& registry,
    const std::string& command);

bool IsFoundationCommandDispatchResultValid(
    const FoundationCommandDispatchResult& result) noexcept;

FoundationCommandDispatchResult RunFoundationExtensionSelfTestCommand(
    const std::string& command);

FoundationCommandDispatchResult RunFoundationAllSelfTestsCommand(
    const std::string& command);

bool FoundationCommandDispatcherSelfTest();
