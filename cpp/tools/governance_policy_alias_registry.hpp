// File: cpp/tools/governance_policy_alias_registry.hpp
#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

struct GovernanceAliasRecord {
    std::string alias;
    std::string canonical;
    bool active{};
};

class GovernancePolicyAliasRegistry {
public:
    bool AddAlias(std::string_view alias, std::string_view canonical);

    std::optional<std::string> ResolveAlias(std::string_view alias) const;

    std::vector<std::string> ListAliases() const;

    bool HasDuplicateAlias(std::string_view alias) const;

private:
    std::vector<GovernanceAliasRecord> records_;
};

bool GovernancePolicyAliasRegistrySelfTest();
