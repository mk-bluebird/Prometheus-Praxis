// File: cpp/tools/header_self_sufficiency.hpp
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace prometheus_praxis::foundation::headers {

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

struct HeaderIncludePolicySnapshot {
    std::vector<HeaderIncludeRequirement> requirements;
    std::uint32_t version{1U};
    std::string generated_by;
};

std::vector<HeaderIncludeRequirement> HeaderSelfSufficiencyRequirements();

HeaderIncludePolicyValidation ValidateHeaderIncludePolicy(
    const std::vector<HeaderIncludeRequirement>& requirements);

std::string ExplainHeaderIncludeRequirement(
    const HeaderIncludeRequirement& requirement);

HeaderIncludePolicySnapshot SnapshotHeaderIncludePolicy(
    const std::vector<HeaderIncludeRequirement>& requirements);

bool HeaderSelfSufficiencyLinterSelfTest();

}  // namespace prometheus_praxis::foundation::headers
