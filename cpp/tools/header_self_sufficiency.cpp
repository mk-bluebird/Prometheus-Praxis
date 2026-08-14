// File: cpp/tools/header_self_sufficiency.cpp
#include "header_self_sufficiency.hpp"

#include "path_normalization.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace prometheus_praxis::foundation::headers {
namespace {

constexpr std::uint32_t kPolicyVersion = 1U;
constexpr std::string_view kGeneratedBy =
    "foundation_header_self_sufficiency_policy_v1";

bool IsCppIdentifier(std::string_view identifier) noexcept {
    if (identifier.empty()) {
        return false;
    }

    const unsigned char first = static_cast<unsigned char>(identifier.front());
    if (std::isalpha(first) == 0 && first != '_') {
        return false;
    }

    for (const unsigned char character : identifier) {
        if (std::isalnum(character) == 0 && character != '_') {
            return false;
        }
    }

    return true;
}

bool IsNormalizedHeaderPath(std::string_view header_path) {
    const std::optional<std::string> normalized =
        paths::NormalizeRepositoryPath(header_path);

    return normalized.has_value() &&
           *normalized == header_path &&
           header_path.ends_with(".hpp");
}

bool IsSystemInclude(std::string_view include) noexcept {
    if (include.size() < 3U ||
        include.front() != '<' ||
        include.back() != '>') {
        return false;
    }

    const std::string_view header_name =
        include.substr(1U, include.size() - 2U);

    return !header_name.empty() &&
           header_name.find_first_of("/\\\"' \t\r\n") ==
               std::string_view::npos;
}

std::string RequirementKey(const HeaderIncludeRequirement& requirement) {
    return requirement.header_path + '\n' +
           requirement.required_include + '\n' +
           requirement.identifier;
}

void AddReason(
    HeaderIncludePolicyValidation& validation,
    std::string reason) {
    validation.reasons.push_back(std::move(reason));
}

HeaderIncludeRequirement MakeRequirement(
    std::string header_path,
    std::string required_include,
    std::string identifier,
    std::string purpose,
    bool required = true) {
    return HeaderIncludeRequirement{
        std::move(header_path),
        std::move(required_include),
        std::move(identifier),
        std::move(purpose),
        required};
}

}  // namespace

std::vector<HeaderIncludeRequirement> HeaderSelfSufficiencyRequirements() {
    return {
        MakeRequirement(
            "cpp/tools/foundation_legacy_contract_inventory.hpp",
            "<optional>",
            "std_optional",
            "Owns direct declarations for optional contract lookups."),
        MakeRequirement(
            "cpp/tools/foundation_legacy_contract_inventory.hpp",
            "<string>",
            "std_string",
            "Owns direct declarations for contract metadata text."),
        MakeRequirement(
            "cpp/tools/foundation_legacy_contract_inventory.hpp",
            "<string_view>",
            "std_string_view",
            "Owns direct declarations for command lookup views."),
        MakeRequirement(
            "cpp/tools/foundation_legacy_contract_inventory.hpp",
            "<vector>",
            "std_vector",
            "Owns direct declarations for contract collections."),
        MakeRequirement(
            "cpp/tools/foundation_report.hpp",
            "<string>",
            "std_string",
            "Owns report explanation and difference text."),
        MakeRequirement(
            "cpp/tools/foundation_report.hpp",
            "<vector>",
            "std_vector",
            "Owns report validation and difference collections."),
        MakeRequirement(
            "cpp/tools/foundation_report_json.hpp",
            "<sstream>",
            "std_ostringstream",
            "Owns JSON writer stream declarations."),
        MakeRequirement(
            "cpp/tools/foundation_report_json.hpp",
            "<string>",
            "std_string",
            "Owns serialized report document declarations."),
        MakeRequirement(
            "cpp/tools/foundation_report_json.hpp",
            "<string_view>",
            "std_string_view",
            "Owns JSON string-view declarations."),
        MakeRequirement(
            "cpp/tools/foundation_extension_registry.hpp",
            "<cstddef>",
            "std_size_t",
            "Owns extension execution-order declarations."),
        MakeRequirement(
            "cpp/tools/foundation_extension_registry.hpp",
            "<optional>",
            "std_optional",
            "Owns extension validation result declarations."),
        MakeRequirement(
            "cpp/tools/foundation_extension_registry.hpp",
            "<string>",
            "std_string",
            "Owns extension names and diagnostic text."),
        MakeRequirement(
            "cpp/tools/foundation_extension_registry.hpp",
            "<vector>",
            "std_vector",
            "Owns extension descriptor and result collections."),
        MakeRequirement(
            "cpp/tools/foundation_command_dispatch.hpp",
            "<functional>",
            "std_function",
            "Owns command handler function objects."),
        MakeRequirement(
            "cpp/tools/foundation_command_dispatch.hpp",
            "<optional>",
            "std_optional",
            "Owns command lookup results."),
        MakeRequirement(
            "cpp/tools/foundation_command_dispatch.hpp",
            "<string>",
            "std_string",
            "Owns command records and dispatch details."),
        MakeRequirement(
            "cpp/tools/foundation_command_dispatch.hpp",
            "<string_view>",
            "std_string_view",
            "Owns command lookup views."),
        MakeRequirement(
            "cpp/tools/foundation_command_dispatch.hpp",
            "<vector>",
            "std_vector",
            "Owns command registry declarations."),
        MakeRequirement(
            "cpp/eco_restoration/water_biodiversity_diagnostics.hpp",
            "<cstdint>",
            "std_int64_t",
            "Owns millilitre water-budget quantity declarations."),
        MakeRequirement(
            "cpp/eco_restoration/water_biodiversity_diagnostics.hpp",
            "<string>",
            "std_string",
            "Owns stakeholder identity and diagnostic text."),
        MakeRequirement(
            "cpp/eco_restoration/water_biodiversity_diagnostics.hpp",
            "<vector>",
            "std_vector",
            "Owns stakeholder scenario collections."),
        MakeRequirement(
            "cpp/eco_restoration/invasive_control_diagnostics.hpp",
            "<string>",
            "std_string",
            "Owns stable candidate identifiers and audit text."),
        MakeRequirement(
            "cpp/eco_restoration/invasive_control_diagnostics.hpp",
            "<vector>",
            "std_vector",
            "Owns candidate and audit-summary collections."),
        MakeRequirement(
            "cpp/simulation/irrigation_scenario_diagnostics.hpp",
            "<cstddef>",
            "std_size_t",
            "Owns scenario and horizon index declarations."),
        MakeRequirement(
            "cpp/simulation/irrigation_scenario_diagnostics.hpp",
            "<string>",
            "std_string",
            "Owns irrigation diagnostic explanation text."),
        MakeRequirement(
            "cpp/simulation/irrigation_scenario_diagnostics.hpp",
            "<vector>",
            "std_vector",
            "Owns rainfall and dry-run collections."),
        MakeRequirement(
            "cpp/simulation/irrigation_scenario_diagnostics.hpp",
            "<numeric>",
            "std_accumulate",
            "Owns direct numeric aggregation support for irrigation analysis."),
        MakeRequirement(
            "cpp/tools/authorization_evidence_sequence_ledger.hpp",
            "<cstddef>",
            "std_size_t",
            "Owns ledger size declarations."),
        MakeRequirement(
            "cpp/tools/authorization_evidence_sequence_ledger.hpp",
            "<cstdint>",
            "std_uint64_t",
            "Owns evidence sequence and timestamp declarations."),
        MakeRequirement(
            "cpp/tools/authorization_evidence_sequence_ledger.hpp",
            "<optional>",
            "std_optional",
            "Owns latest evidence lookup declarations."),
        MakeRequirement(
            "cpp/tools/authorization_evidence_sequence_ledger.hpp",
            "<string>",
            "std_string",
            "Owns authorization identifier declarations."),
        MakeRequirement(
            "cpp/tools/authorization_evidence_sequence_ledger.hpp",
            "<vector>",
            "std_vector",
            "Owns ordered authorization evidence collections."),
        MakeRequirement(
            "cpp/tools/proof_checked_dispatch_replay.hpp",
            "<cstddef>",
            "std_size_t",
            "Owns indexed authorization replay results."),
        MakeRequirement(
            "cpp/tools/proof_checked_dispatch_replay.hpp",
            "<cstdint>",
            "std_uint64_t",
            "Owns authorization replay evidence timestamps and sequences."),
        MakeRequirement(
            "cpp/tools/proof_checked_dispatch_replay.hpp",
            "<string>",
            "std_string",
            "Owns replay outcome and diagnostic text."),
        MakeRequirement(
            "cpp/tools/proof_checked_dispatch_replay.hpp",
            "<string_view>",
            "std_string_view",
            "Owns replay policy identifier views."),
        MakeRequirement(
            "cpp/tools/proof_checked_dispatch_replay.hpp",
            "<vector>",
            "std_vector",
            "Owns replay record and result collections."),
        MakeRequirement(
            "cpp/tools/deterministic_scenario_library.hpp",
            "<cstddef>",
            "std_size_t",
            "Owns deterministic fixture size declarations."),
        MakeRequirement(
            "cpp/tools/deterministic_scenario_library.hpp",
            "<cstdint>",
            "std_uint64_t",
            "Owns deterministic generator seed and state declarations."),
        MakeRequirement(
            "cpp/tools/deterministic_scenario_library.hpp",
            "<vector>",
            "std_vector",
            "Owns generated probability and rainfall collections."),
        MakeRequirement(
            "cpp/tools/path_normalization.hpp",
            "<optional>",
            "std_optional",
            "Owns normalized path result declarations."),
        MakeRequirement(
            "cpp/tools/path_normalization.hpp",
            "<string>",
            "std_string",
            "Owns normalized path text and decomposition fields."),
        MakeRequirement(
            "cpp/tools/path_normalization.hpp",
            "<string_view>",
            "std_string_view",
            "Owns raw path input views."),
        MakeRequirement(
            "cpp/tools/path_normalization.hpp",
            "<vector>",
            "std_vector",
            "Owns normalized path component collections."),
        MakeRequirement(
            "cpp/tools/header_self_sufficiency.hpp",
            "<cstdint>",
            "std_uint32_t",
            "Owns header include policy version declarations."),
        MakeRequirement(
            "cpp/tools/header_self_sufficiency.hpp",
            "<string>",
            "std_string",
            "Owns include policy fields and diagnostic text."),
        MakeRequirement(
            "cpp/tools/header_self_sufficiency.hpp",
            "<vector>",
            "std_vector",
            "Owns include policy requirement collections."),
    };
}

HeaderIncludePolicyValidation ValidateHeaderIncludePolicy(
    const std::vector<HeaderIncludeRequirement>& requirements) {
    HeaderIncludePolicyValidation validation;
    std::set<std::string> unique_requirements;

    if (requirements.empty()) {
        AddReason(
            validation,
            "header include policy must contain at least one requirement");
    }

    for (const HeaderIncludeRequirement& requirement : requirements) {
        const std::string label =
            "header=" + requirement.header_path +
            "; include=" + requirement.required_include +
            "; identifier=" + requirement.identifier;

        if (!IsNormalizedHeaderPath(requirement.header_path)) {
            AddReason(validation, "invalid header path; " + label);
        }

        if (!IsSystemInclude(requirement.required_include)) {
            AddReason(validation, "invalid system include spelling; " + label);
        }

        if (!IsCppIdentifier(requirement.identifier)) {
            AddReason(validation, "invalid identifier; " + label);
        }

        if (requirement.purpose.empty()) {
            AddReason(validation, "empty purpose; " + label);
        }

        if (!unique_requirements.insert(RequirementKey(requirement)).second) {
            AddReason(validation, "duplicate include policy; " + label);
        }
    }

    validation.valid = validation.reasons.empty();
    return validation;
}

std::string ExplainHeaderIncludeRequirement(
    const HeaderIncludeRequirement& requirement) {
    return "header_include_requirement"
           "; header_path=" + requirement.header_path +
           "; required_include=" + requirement.required_include +
           "; identifier=" + requirement.identifier +
           "; required=" + (requirement.required ? "true" : "false") +
           "; purpose=" + requirement.purpose;
}

HeaderIncludePolicySnapshot SnapshotHeaderIncludePolicy(
    const std::vector<HeaderIncludeRequirement>& requirements) {
    return HeaderIncludePolicySnapshot{
        requirements,
        kPolicyVersion,
        std::string(kGeneratedBy)};
}

bool HeaderSelfSufficiencyLinterSelfTest() {
    const std::vector<HeaderIncludeRequirement> requirements =
        HeaderSelfSufficiencyRequirements();
    const HeaderIncludePolicyValidation known_validation =
        ValidateHeaderIncludePolicy(requirements);

    if (!known_validation.valid || requirements.empty()) {
        return false;
    }

    const auto numeric_requirement = std::find_if(
        requirements.begin(),
        requirements.end(),
        [](const HeaderIncludeRequirement& requirement) {
            return requirement.header_path ==
                       "cpp/simulation/irrigation_scenario_diagnostics.hpp" &&
                   requirement.required_include == "<numeric>" &&
                   requirement.identifier == "std_accumulate" &&
                   requirement.required;
        });

    if (numeric_requirement == requirements.end()) {
        return false;
    }

    std::vector<HeaderIncludeRequirement> duplicate = requirements;
    duplicate.push_back(requirements.front());
    if (ValidateHeaderIncludePolicy(duplicate).valid) {
        return false;
    }

    const HeaderIncludeRequirement invalid_include{
        "cpp/tools/foundation_report.hpp",
        "vector",
        "std_vector",
        "Rejects malformed include spelling.",
        true};
    if (ValidateHeaderIncludePolicy({invalid_include}).valid) {
        return false;
    }

    const HeaderIncludeRequirement invalid_path{
        "../foundation_report.hpp",
        "<vector>",
        "std_vector",
        "Rejects unsafe repository paths.",
        true};
    if (ValidateHeaderIncludePolicy({invalid_path}).valid) {
        return false;
    }

    const HeaderIncludeRequirement invalid_identifier{
        "cpp/tools/foundation_report.hpp",
        "<vector>",
        "std-vector",
        "Rejects malformed identifiers.",
        true};
    if (ValidateHeaderIncludePolicy({invalid_identifier}).valid) {
        return false;
    }

    const HeaderIncludePolicySnapshot snapshot =
        SnapshotHeaderIncludePolicy(requirements);
    if (snapshot.version != kPolicyVersion ||
        snapshot.generated_by != kGeneratedBy ||
        snapshot.requirements.size() != requirements.size()) {
        return false;
    }

    return ExplainHeaderIncludeRequirement(*numeric_requirement).find(
               "required_include=<numeric>") != std::string::npos;
}

}  // namespace prometheus_praxis::foundation::headers
