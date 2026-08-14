// File: cpp/tools/header_self_sufficiency.cpp
#include "header_self_sufficiency.hpp"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

bool IsRepositoryHeaderPath(const std::string_view path) noexcept {
    if (path.size() < 5U ||
        path.front() == '/' ||
        path.find('\\') != std::string_view::npos ||
        path.find("..") != std::string_view::npos) {
        return false;
    }

    return path.ends_with(".hpp");
}

bool IsSystemInclude(const std::string_view include_name) noexcept {
    return include_name.size() > 2U &&
           include_name.front() == '<' &&
           include_name.back() == '>' &&
           include_name.find('/') == std::string_view::npos &&
           include_name.find('\\') == std::string_view::npos;
}

bool IsIdentifier(const std::string_view identifier) noexcept {
    if (identifier.empty()) {
        return false;
    }

    const char first = identifier.front();
    if (!((first >= 'A' && first <= 'Z') ||
          (first >= 'a' && first <= 'z') ||
          first == '_')) {
        return false;
    }

    for (const char character : identifier) {
        const bool alphabetic =
            (character >= 'A' && character <= 'Z') ||
            (character >= 'a' && character <= 'z');
        const bool numeric = character >= '0' && character <= '9';

        if (!alphabetic && !numeric && character != '_') {
            return false;
        }
    }

    return true;
}

bool IsDuplicateRequirement(
    const std::vector<HeaderIncludeRequirement>& requirements,
    const std::size_t index) {
    for (std::size_t other = 0U; other < requirements.size(); ++other) {
        if (other == index) {
            continue;
        }

        if (requirements[index].header_path == requirements[other].header_path &&
            requirements[index].required_include ==
                requirements[other].required_include &&
            requirements[index].identifier == requirements[other].identifier) {
            return true;
        }
    }

    return false;
}

}  // namespace

std::vector<HeaderIncludeRequirement> HeaderSelfSufficiencyRequirements() {
    return {
        HeaderIncludeRequirement{
            "cpp/tools/foundation_report.hpp",
            "<string>",
            "std_string",
            "Owns report explanation and difference text.",
            true},
        HeaderIncludeRequirement{
            "cpp/tools/foundation_report.hpp",
            "<vector>",
            "std_vector",
            "Owns report-difference and validation-reason collections.",
            true},
        HeaderIncludeRequirement{
            "cpp/tools/foundation_extension_registry.hpp",
            "<optional>",
            "std_optional",
            "Owns registry validation error results.",
            true},
        HeaderIncludeRequirement{
            "cpp/tools/foundation_command_dispatch.hpp",
            "<functional>",
            "std_function",
            "Owns command-handler function objects.",
            true},
        HeaderIncludeRequirement{
            "cpp/eco_restoration/water_biodiversity_diagnostics.hpp",
            "<cstdint>",
            "std_int64_t",
            "Owns millilitre allocation quantities.",
            true},
        HeaderIncludeRequirement{
            "cpp/simulation/irrigation_scenario_diagnostics.hpp",
            "<numeric>",
            "std_accumulate",
            "Owns probability aggregation without transitive includes.",
            true}};
}

HeaderIncludePolicyValidation ValidateHeaderIncludePolicy(
    const std::vector<HeaderIncludeRequirement>& requirements) {
    HeaderIncludePolicyValidation validation{true, {}};

    if (requirements.empty()) {
        validation.valid = false;
        validation.reasons.emplace_back(
            "header include policy must contain at least one requirement");
        return validation;
    }

    for (std::size_t index = 0U; index < requirements.size(); ++index) {
        const HeaderIncludeRequirement& requirement = requirements[index];

        if (!IsRepositoryHeaderPath(requirement.header_path)) {
            validation.reasons.emplace_back(
                "header path must be a normalized repository-relative .hpp path");
        }

        if (!IsSystemInclude(requirement.required_include)) {
            validation.reasons.emplace_back(
                "required include must use a direct system-header form");
        }

        if (!IsIdentifier(requirement.identifier)) {
            validation.reasons.emplace_back(
                "requirement identifier must be a non-empty C++ identifier");
        }

        if (requirement.purpose.empty()) {
            validation.reasons.emplace_back(
                "header include requirement purpose must not be empty");
        }

        if (IsDuplicateRequirement(requirements, index)) {
            validation.reasons.emplace_back(
                "duplicate header include requirement is not permitted");
        }
    }

    validation.valid = validation.reasons.empty();
    return validation;
}

std::string ExplainHeaderIncludeRequirement(
    const HeaderIncludeRequirement& requirement) {
    std::string output;
    output.reserve(
        requirement.header_path.size() +
        requirement.required_include.size() +
        requirement.identifier.size() +
        requirement.purpose.size() + 64U);

    output += "header_path=";
    output += requirement.header_path;
    output += "; required_include=";
    output += requirement.required_include;
    output += "; identifier=";
    output += requirement.identifier;
    output += "; required=";
    output += requirement.required ? "true" : "false";
    output += "; purpose=";
    output += requirement.purpose;
    return output;
}

bool HeaderSelfSufficiencyLinterSelfTest() {
    const std::vector<HeaderIncludeRequirement> requirements =
        HeaderSelfSufficiencyRequirements();

    const HeaderIncludePolicyValidation known_validation =
        ValidateHeaderIncludePolicy(requirements);

    if (!known_validation.valid || requirements.empty()) {
        return false;
    }

    bool has_irrigation_numeric_requirement = false;
    for (const HeaderIncludeRequirement& requirement : requirements) {
        if (requirement.header_path ==
                "cpp/simulation/irrigation_scenario_diagnostics.hpp" &&
            requirement.required_include == "<numeric>" &&
            requirement.identifier == "std_accumulate" &&
            requirement.required) {
            has_irrigation_numeric_requirement = true;
        }
    }

    if (!has_irrigation_numeric_requirement) {
        return false;
    }

    const std::string explanation =
        ExplainHeaderIncludeRequirement(requirements.front());
    if (explanation.find("header_path=cpp/tools/foundation_report.hpp") != 0U) {
        return false;
    }

    std::vector<HeaderIncludeRequirement> invalid_include = requirements;
    invalid_include.push_back(
        HeaderIncludeRequirement{
            "cpp/tools/example.hpp",
            "vector",
            "std_vector",
            "Invalid direct-include spelling.",
            true});

    if (ValidateHeaderIncludePolicy(invalid_include).valid) {
        return false;
    }

    std::vector<HeaderIncludeRequirement> duplicate = requirements;
    duplicate.push_back(requirements.front());

    if (ValidateHeaderIncludePolicy(duplicate).valid) {
        return false;
    }

    return true;
}
