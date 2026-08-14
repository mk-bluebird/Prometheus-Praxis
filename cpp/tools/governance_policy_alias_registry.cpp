// File: cpp/tools/governance_policy_alias_registry.cpp
#include "governance_policy_alias_registry.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

bool IsLowerSnakeCase(const std::string_view value) noexcept {
    if (value.empty() || value.front() == '_' || value.back() == '_') {
        return false;
    }

    bool preceding_underscore = false;
    for (const char character : value) {
        const bool lower_case = character >= 'a' && character <= 'z';
        const bool digit = character >= '0' && character <= '9';

        if (character == '_') {
            if (preceding_underscore) {
                return false;
            }
            preceding_underscore = true;
            continue;
        }

        if (!lower_case && !digit) {
            return false;
        }
        preceding_underscore = false;
    }

    return true;
}

std::vector<GovernanceAliasRecord>::const_iterator FindAlias(
    const std::vector<GovernanceAliasRecord>& records,
    const std::string_view alias) {
    return std::find_if(
        records.begin(),
        records.end(),
        [alias](const GovernanceAliasRecord& record) {
            return record.alias == alias;
        });
}

}  // namespace

bool GovernancePolicyAliasRegistry::AddAlias(
    const std::string_view alias,
    const std::string_view canonical) {
    if (!IsLowerSnakeCase(alias) ||
        !IsLowerSnakeCase(canonical) ||
        alias == canonical ||
        HasDuplicateAlias(alias)) {
        return false;
    }

    records_.push_back(
        GovernanceAliasRecord{
            std::string(alias),
            std::string(canonical),
            true});
    return true;
}

std::optional<std::string> GovernancePolicyAliasRegistry::ResolveAlias(
    const std::string_view alias) const {
    const auto record = FindAlias(records_, alias);
    if (record == records_.end() || !record->active) {
        return std::nullopt;
    }

    return record->canonical;
}

std::vector<std::string> GovernancePolicyAliasRegistry::ListAliases() const {
    std::vector<std::string> aliases;
    aliases.reserve(records_.size());

    for (const GovernanceAliasRecord& record : records_) {
        if (record.active) {
            aliases.push_back(record.alias);
        }
    }

    return aliases;
}

bool GovernancePolicyAliasRegistry::HasDuplicateAlias(
    const std::string_view alias) const {
    return FindAlias(records_, alias) != records_.end();
}

bool GovernancePolicyAliasRegistrySelfTest() {
    GovernancePolicyAliasRegistry registry;

    if (!registry.AddAlias(
            "water_reserve_guard",
            "ecological_water_reserve")) {
        return false;
    }

    const std::optional<std::string> resolved =
        registry.ResolveAlias("water_reserve_guard");

    if (!resolved.has_value() ||
        *resolved != "ecological_water_reserve") {
        return false;
    }

    const std::vector<std::string> aliases = registry.ListAliases();
    if (aliases.size() != 1U ||
        aliases.front() != "water_reserve_guard") {
        return false;
    }

    if (!registry.HasDuplicateAlias("water_reserve_guard") ||
        registry.HasDuplicateAlias("missing_alias")) {
        return false;
    }

    if (registry.AddAlias(
            "water_reserve_guard",
            "another_policy") ||
        registry.AddAlias(
            "same_policy",
            "same_policy") ||
        registry.AddAlias(
            "Invalid_Alias",
            "ecological_water_reserve") ||
        registry.AddAlias(
            "valid_alias",
            "Invalid_Canonical")) {
        return false;
    }

    return !registry.ResolveAlias("missing_alias").has_value();
}
