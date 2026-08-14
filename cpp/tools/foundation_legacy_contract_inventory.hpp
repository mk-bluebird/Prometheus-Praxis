// File: cpp/tools/foundation_legacy_contract_inventory.hpp
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace prometheus_praxis::foundation::legacy_contract {

enum class LegacyStream {
    Stdout,
    Stderr,
    None
};

struct LegacyCommandContract {
    std::string command;
    std::string argv_fixture;
    LegacyStream stdout_stream;
    LegacyStream stderr_stream;
    int expected_exit_code;
    bool requires_newline;
    bool stdout_utf8_json;
    bool stderr_utf8_text;
    bool uses_safety_exit_code;
    std::string evidence_tag;
};

struct LegacyContractInventory {
    std::vector<LegacyCommandContract> commands;
    std::string binary_identity;
    std::string build_configuration;
    std::string capture_plan;
};

LegacyContractInventory BuildCurrentLegacyContractInventory();

std::optional<LegacyCommandContract> FindLegacyContract(
    const LegacyContractInventory& inventory,
    std::string_view command);

bool IsLegacyContractInventoryComplete(
    const LegacyContractInventory& inventory);

std::vector<std::string> MissingLegacyContractEvidence(
    const LegacyContractInventory& inventory);

std::string ExplainLegacyContract(
    const LegacyCommandContract& contract);

bool FoundationLegacyContractInventorySelfTest();

}  // namespace prometheus_praxis::foundation::legacy_contract
