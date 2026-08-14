// File: cpp/tools/ppx_foundation_c_api.cpp
#include "ppx_foundation_c_api.h"

#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <string>

namespace {

bool is_flag(const std::uint8_t value) noexcept {
    return value == 0U || value == 1U;
}

bool valid_flags(const PpxFoundationReportV1& report) noexcept {
    return is_flag(report.private_heat_accepted) &&
           is_flag(report.threat_fail_closed) &&
           is_flag(report.water_biodiversity_allowed) &&
           is_flag(report.water_biodiversity_invariant_holds) &&
           is_flag(report.authorization_accepted) &&
           is_flag(report.invasive_control_safe) &&
           is_flag(report.irrigation_robustly_feasible) &&
           is_flag(report.foundation_safe);
}

bool valid_metrics(const PpxFoundationReportV1& report) noexcept {
    return std::isfinite(report.maximum_risk_of_harm) &&
           std::isfinite(report.knowledge_factor) &&
           std::isfinite(report.eco_impact_value) &&
           report.maximum_risk_of_harm >= 0.0 &&
           report.maximum_risk_of_harm <= 1.0 &&
           report.knowledge_factor >= 0.0 &&
           report.knowledge_factor <= 1.0 &&
           report.eco_impact_value >= 0.0 &&
           report.eco_impact_value <= 1.0;
}

bool computed_foundation_safe(const PpxFoundationReportV1& report) noexcept {
    return report.private_heat_accepted == 1U &&
           report.threat_fail_closed == 0U &&
           report.water_biodiversity_allowed == 1U &&
           report.water_biodiversity_invariant_holds == 1U &&
           report.authorization_accepted == 1U &&
           report.invasive_control_safe == 1U &&
           report.irrigation_robustly_feasible == 1U &&
           report.maximum_risk_of_harm <= 0.30;
}

std::string bool_json(const std::uint8_t value) {
    return value == 1U ? "true" : "false";
}

std::string serialize_json(const PpxFoundationReportV1& report,
                           const bool policy_valid) {
    std::ostringstream output;
    output.imbue(std::locale::classic());
    output << std::fixed << std::setprecision(6);
    output << '{'
           << "\"abi_version\":" << PPX_ABI_VERSION_V1
           << ",\"diagnostic_schema_version\":"
           << PPX_FOUNDATION_DIAGNOSTIC_SCHEMA_VERSION_V1
           << ",\"private_heat_accepted\":" << bool_json(report.private_heat_accepted)
           << ",\"threat_fail_closed\":" << bool_json(report.threat_fail_closed)
           << ",\"water_biodiversity_allowed\":"
           << bool_json(report.water_biodiversity_allowed)
           << ",\"water_biodiversity_invariant_holds\":"
           << bool_json(report.water_biodiversity_invariant_holds)
           << ",\"authorization_accepted\":" << bool_json(report.authorization_accepted)
           << ",\"invasive_control_safe\":" << bool_json(report.invasive_control_safe)
           << ",\"irrigation_robustly_feasible\":"
           << bool_json(report.irrigation_robustly_feasible)
           << ",\"maximum_risk_of_harm\":" << report.maximum_risk_of_harm
           << ",\"knowledge_factor\":" << report.knowledge_factor
           << ",\"eco_impact_value\":" << report.eco_impact_value
           << ",\"foundation_safe\":" << bool_json(report.foundation_safe)
           << ",\"validation_status\":\""
           << (policy_valid ? "valid" : "policy_violation") << "\"}";
    return output.str();
}

}  // namespace

extern "C" std::int32_t ppx_validate_foundation_report_v1(
    const PpxFoundationReportV1* report,
    char* output_utf8,
    const std::size_t output_capacity,
    std::size_t* required_output_bytes) noexcept {
    try {
        if (required_output_bytes == nullptr) {
            return PPX_RESULT_RUNTIME_ERROR;
        }

        *required_output_bytes = 0U;

        if (report == nullptr ||
            (output_utf8 == nullptr && output_capacity != 0U) ||
            (output_utf8 != nullptr && output_capacity == 0U)) {
            return PPX_RESULT_RUNTIME_ERROR;
        }

        if (report->abi_version != PPX_ABI_VERSION_V1) {
            return PPX_RESULT_UNSUPPORTED_VERSION;
        }

        if (!valid_flags(*report) || !valid_metrics(*report)) {
            return PPX_RESULT_RUNTIME_ERROR;
        }

        const bool expected_safe = computed_foundation_safe(*report);
        const bool policy_valid =
            (report->foundation_safe == (expected_safe ? 1U : 0U));
        const std::string document = serialize_json(*report, policy_valid);
        *required_output_bytes = document.size();

        if (output_capacity < document.size() + 1U) {
            return PPX_RESULT_BUFFER_TOO_SMALL;
        }

        for (std::size_t index = 0U; index < document.size(); ++index) {
            output_utf8[index] = document[index];
        }
        output_utf8[document.size()] = '\0';

        return policy_valid ? PPX_RESULT_VALID : PPX_RESULT_POLICY_VIOLATION;
    } catch (...) {
        return PPX_RESULT_RUNTIME_ERROR;
    }
}
