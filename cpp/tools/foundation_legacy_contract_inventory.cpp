// File: cpp/tools/foundation_legacy_contract_inventory.cpp
#include "foundation_legacy_contract_inventory.hpp"

#include <algorithm>
#include <array>
#include <set>
#include <sstream>
#include <utility>

namespace prometheus_praxis::foundation::legacy_contract {
namespace {

constexpr std::array<std::string_view, 2U> kCurrentLegacyCommands{
    "--foundation-self-check",
    "--foundation-extension-self-test",
};

bool BeginsWithDoubleDash(std::string_view value) {
    return value.size() > 2U && value[0] == '-' && value[1] == '-';
}

bool IsSupportedExitCode(int exit_code) {
    return exit_code == 0 || exit_code == 1 || exit_code == 2 || exit_code == 64;
}

bool IsUsableEvidenceTag(std::string_view evidence_tag) {
    return evidence_tag.find("cpp/tools/prometheus_praxis_foundation_main.cpp") !=
           std::string_view::npos;
}

std::string StreamName(LegacyStream stream) {
    switch (stream) {
        case LegacyStream::Stdout:
            return "stdout";
        case LegacyStream::Stderr:
            return "stderr";
        case LegacyStream::None:
            return "none";
    }
    return "invalid";
}

bool IsKnownCommand(std::string_view command) {
    return std::find(kCurrentLegacyCommands.begin(),
                     kCurrentLegacyCommands.end(),
                     command) != kCurrentLegacyCommands.end();
}

bool HasValidStreamShape(const LegacyCommandContract& contract) {
    if (contract.stdout_stream == LegacyStream::Stderr ||
        contract.stderr_stream == LegacyStream::Stdout) {
        return false;
    }

    if (contract.stdout_utf8_json && contract.stdout_stream != LegacyStream::Stdout) {
        return false;
    }

    if (contract.stderr_utf8_text && contract.stderr_stream != LegacyStream::Stderr) {
        return false;
    }

    if (contract.stdout_stream == LegacyStream::None && contract.stdout_utf8_json) {
        return false;
    }

    if (contract.stderr_stream == LegacyStream::None && contract.stderr_utf8_text) {
        return false;
    }

    return true;
}

bool IsValidContractRecord(const LegacyCommandContract& contract) {
    return BeginsWithDoubleDash(contract.command) &&
           !contract.argv_fixture.empty() &&
           IsSupportedExitCode(contract.expected_exit_code) &&
           HasValidStreamShape(contract) &&
           IsUsableEvidenceTag(contract.evidence_tag);
}

std::vector<std::string> MissingEvidenceFor(
    const LegacyContractInventory& inventory) {
    std::vector<std::string> missing;
    std::set<std::string> observed;

    if (inventory.binary_identity.empty()) {
        missing.emplace_back("binary_identity");
    }
    if (inventory.build_configuration.empty()) {
        missing.emplace_back("build_configuration");
    }
    if (inventory.capture_plan.empty()) {
        missing.emplace_back("capture_plan");
    }

    for (const LegacyCommandContract& contract : inventory.commands) {
        if (!BeginsWithDoubleDash(contract.command)) {
            missing.emplace_back("invalid_command:" + contract.command);
            continue;
        }

        if (!IsKnownCommand(contract.command)) {
            missing.emplace_back("unknown_source_command:" + contract.command);
        }

        if (!observed.insert(contract.command).second) {
            missing.emplace_back("duplicate_command:" + contract.command);
        }

        if (contract.argv_fixture.empty()) {
            missing.emplace_back("missing_argv_fixture:" + contract.command);
        }
        if (!IsSupportedExitCode(contract.expected_exit_code)) {
            missing.emplace_back("invalid_exit_code:" + contract.command);
        }
        if (!HasValidStreamShape(contract)) {
            missing.emplace_back("invalid_stream_metadata:" + contract.command);
        }
        if (!IsUsableEvidenceTag(contract.evidence_tag)) {
            missing.emplace_back("missing_source_evidence:" + contract.command);
        }
    }

    for (const std::string_view command : kCurrentLegacyCommands) {
        if (observed.find(std::string(command)) == observed.end()) {
            missing.emplace_back("missing_command:" + std::string(command));
        }
    }

    return missing;
}

}  // namespace

LegacyContractInventory BuildCurrentLegacyContractInventory() {
    return LegacyContractInventory{
        {
            LegacyCommandContract{
                "--foundation-self-check",
                "prometheus_praxis_foundation --foundation-self-check",
                LegacyStream::Stdout,
                LegacyStream::None,
                0,
                true,
                true,
                false,
                false,
                "cpp/tools/prometheus_praxis_foundation_main.cpp:"
                "foundation_self_check;main_dispatch"},
            LegacyCommandContract{
                "--foundation-extension-self-test",
                "prometheus_praxis_foundation --foundation-extension-self-test",
                LegacyStream::Stdout,
                LegacyStream::None,
                0,
                true,
                false,
                false,
                true,
                "cpp/tools/prometheus_praxis_foundation_main.cpp:"
                "extension_registry_self_test;main_dispatch"},
        },
        "prometheus_praxis_foundation",
        "C++20 foundation command-line executable; read-only contract inventory",
        "Capture raw stdout, stderr, exit status, and terminal newline externally "
        "for each argv fixture. Also capture unsupported-command and wrong-arity "
        "paths as dispatcher evidence without adding non-command records."
    };
}

std::optional<LegacyCommandContract> FindLegacyContract(
    const LegacyContractInventory& inventory,
    std::string_view command) {
    const auto iterator = std::find_if(
        inventory.commands.begin(),
        inventory.commands.end(),
        [command](const LegacyCommandContract& contract) {
            return contract.command == command;
        });

    if (iterator == inventory.commands.end()) {
        return std::nullopt;
    }

    return *iterator;
}

bool IsLegacyContractInventoryComplete(
    const LegacyContractInventory& inventory) {
    return MissingEvidenceFor(inventory).empty();
}

std::vector<std::string> MissingLegacyContractEvidence(
    const LegacyContractInventory& inventory) {
    return MissingEvidenceFor(inventory);
}

std::string ExplainLegacyContract(
    const LegacyCommandContract& contract) {
    std::ostringstream explanation;
    explanation << "command=" << contract.command
                << "; argv_fixture=" << contract.argv_fixture
                << "; stdout=" << StreamName(contract.stdout_stream)
                << "; stderr=" << StreamName(contract.stderr_stream)
                << "; exit_code=" << contract.expected_exit_code
                << "; newline=" << (contract.requires_newline ? "required" : "not_required")
                << "; stdout_utf8_json="
                << (contract.stdout_utf8_json ? "true" : "false")
                << "; stderr_utf8_text="
                << (contract.stderr_utf8_text ? "true" : "false")
                << "; safety_exit_code="
                << (contract.uses_safety_exit_code ? "true" : "false")
                << "; evidence=" << contract.evidence_tag;
    return explanation.str();
}

bool FoundationLegacyContractInventorySelfTest() {
    const LegacyContractInventory inventory = BuildCurrentLegacyContractInventory();

    if (!IsLegacyContractInventoryComplete(inventory) ||
        inventory.commands.size() != kCurrentLegacyCommands.size()) {
        return false;
    }

    const auto self_check = FindLegacyContract(inventory, "--foundation-self-check");
    if (!self_check.has_value() ||
        self_check->stdout_stream != LegacyStream::Stdout ||
        self_check->stderr_stream != LegacyStream::None ||
        self_check->expected_exit_code != 0 ||
        !self_check->stdout_utf8_json ||
        !self_check->requires_newline) {
        return false;
    }

    const auto extension_test =
        FindLegacyContract(inventory, "--foundation-extension-self-test");
    if (!extension_test.has_value() ||
        extension_test->stdout_stream != LegacyStream::Stdout ||
        extension_test->stderr_stream != LegacyStream::None ||
        (extension_test->expected_exit_code != 0 &&
         extension_test->expected_exit_code != 2) ||
        extension_test->stdout_utf8_json ||
        !extension_test->uses_safety_exit_code) {
        return false;
    }

    if (FindLegacyContract(inventory, "--unrecognized-command").has_value()) {
        return false;
    }

    LegacyContractInventory duplicate = inventory;
    duplicate.commands.push_back(inventory.commands.front());
    const std::vector<std::string> duplicate_missing =
        MissingLegacyContractEvidence(duplicate);
    if (std::find(duplicate_missing.begin(),
                  duplicate_missing.end(),
                  "duplicate_command:--foundation-self-check") ==
        duplicate_missing.end()) {
        return false;
    }

    LegacyContractInventory incomplete = inventory;
    incomplete.commands.front().expected_exit_code = 99;
    incomplete.commands.front().stdout_stream = LegacyStream::None;
    incomplete.commands.front().stdout_utf8_json = true;
    const std::vector<std::string> incomplete_missing =
        MissingLegacyContractEvidence(incomplete);
    const bool has_exit_failure =
        std::find(incomplete_missing.begin(),
                  incomplete_missing.end(),
                  "invalid_exit_code:--foundation-self-check") !=
        incomplete_missing.end();
    const bool has_stream_failure =
        std::find(incomplete_missing.begin(),
                  incomplete_missing.end(),
                  "invalid_stream_metadata:--foundation-self-check") !=
        incomplete_missing.end();

    return has_exit_failure && has_stream_failure;
}

}  // namespace prometheus_praxis::foundation::legacy_contract
