// File: cpp/tools/header_self_sufficiency.hpp
#pragma once

#include <string>
#include <vector>

struct HeaderIncludeRequirement {
    std::string header_path;
    std::string required_include;
    std::string identifier;
    std::string purpose;
    bool required{};
};

struct HeaderIncludePolicyValidation {
    bool valid{};
    std::vector<std::string> reasons;
};

std::vector<HeaderIncludeRequirement> HeaderSelfSufficiencyRequirements();

HeaderIncludePolicyValidation ValidateHeaderIncludePolicy(
    const std::vector<HeaderIncludeRequirement>& requirements);

std::string ExplainHeaderIncludeRequirement(
    const HeaderIncludeRequirement& requirement);

bool HeaderSelfSufficiencyLinterSelfTest();
