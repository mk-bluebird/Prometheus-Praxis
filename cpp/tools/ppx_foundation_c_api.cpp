// File: cpp/tools/ppx_foundation_c_api.cpp
#include "ppx_foundation_c_api.h"

#include "foundation_report.hpp"

#include <cmath>
#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>

namespace {

bool IsBinaryFlag(const std::uint8_t value) noexcept {
    return value == 0U || value == 1U;
}

bool AbiFlagsAreValid(const PpxFoundationReportV1& report) noexcept {
    return IsBinaryFlag(report.private_heat_accepted) &&
           IsBinaryFlag(report.threat_fail_closed) &&
           IsBinaryFlag(report.water_biodiversity_allowed) &&
           IsBinaryFlag(report.water_biodiversity_invariant_holds) &&
           IsBinaryFlag(report.authorization_accepted) &&
           IsBinaryFlag(report.invasive_control_safe) &&
           IsBinaryFlag(report.irrigation_robustly_feasible) &&
           IsBinaryFlag(report.foundation_safe);
}

bool AbiMetricsAreValid(const PpxFoundationReportV1& report) noexcept {
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

FoundationReport ToFoundationReport(
    const PpxFoundationReportV1& report) noexcept {
    return FoundationReport{
        report.private_heat_accepted == 1U,
        report.threat_fail_closed == 1U,
        report.water_biodiversity_allowed == 1U,
        report.water_biodiversity_invariant_holds == 1U,
        report.authorization_accepted == 1U,
        report.invasive_control_safe == 1U,
        report.irrigation_robustly_feasible == 1U,
        report.maximum_risk_of_harm,
        report.knowledge_factor,
        report.eco_impact_value,
        report.foundation_safe == 1U};
}

std::string SerializeDiagnosticEnvelope(
    const PpxFoundationReportV1& abi_report,
    const FoundationReport& report,
    const bool policy_valid) {
    const std::string report_json = serialize_foundation_report_json(report);

    std::string output;
    output.reserve(report_json.size() + 112U);
    output += "{\"abi_version\":";
    output += std::to_string(abi_report.abi_version);
    output += ",\"diagnostic_schema_version\":";
    output += std::to_string(PPX_FOUNDATION_DIAGNOSTIC_SCHEMA_VERSION_V1);
    output += ",\"validation_status\":\"";
    output += policy_valid ? "valid" : "policy_violation";
    output += "\",\"report\":";
    output += report_json;
    output += '}';
    return output;
}

bool BufferArgumentsAreValid(
    const char* output_utf8,
    const std::size_t output_capacity) noexcept {
    return (output_utf8 == nullptr && output_capacity == 0U) ||
           (output_utf8 != nullptr && output_capacity > 0U);
}

bool WriteWholeDocument(
    const std::string_view document,
    char* output_utf8,
    const std::size_t output_capacity) noexcept {
    if (output_utf8 == nullptr || output_capacity < document.size() + 1U) {
        return false;
    }

    std::memcpy(output_utf8, document.data(), document.size());
    output_utf8[document.size()] = '\0';
    return true;
}

PpxFoundationReportV1 BuildValidReport() noexcept {
    return PpxFoundationReportV1{
        PPX_ABI_VERSION_V1,
        1U,
        0U,
        1U,
        1U,
        1U,
        1U,
        1U,
        0.20,
        0.90,
        0.85,
        1U};
}

bool RunSelfTest() {
    PpxFoundationReportV1 report = BuildValidReport();
    std::size_t required_size = 0U;

    if (ppx_validate_foundation_report_v1(
            &report,
            nullptr,
            0U,
            &required_size) != PPX_RESULT_VALID ||
        required_size == 0U) {
        return false;
    }

    std::string output(required_size + 1U, '\0');
    if (ppx_validate_foundation_report_v1(
            &report,
            output.data(),
            output.size(),
            &required_size) != PPX_RESULT_VALID ||
        output.find("\"abi_version\":1") == std::string::npos ||
        output.find("\"diagnostic_schema_version\":1") == std::string::npos ||
        output.find("\"validation_status\":\"valid\"") == std::string::npos ||
        output.find("\"report\":{") == std::string::npos) {
        return false;
    }

    char untouched_buffer[4]{'x', 'x', 'x', '\0'};
    std::size_t small_required_size = 0U;
    if (ppx_validate_foundation_report_v1(
            &report,
            untouched_buffer,
            sizeof(untouched_buffer),
            &small_required_size) != PPX_RESULT_BUFFER_TOO_SMALL ||
        small_required_size != required_size ||
        untouched_buffer[0] != 'x' ||
        untouched_buffer[1] != 'x' ||
        untouched_buffer[2] != 'x') {
        return false;
    }

    PpxFoundationReportV1 policy_invalid = report;
    policy_invalid.foundation_safe = 0U;
    std::size_t policy_invalid_size = 0U;
    if (ppx_validate_foundation_report_v1(
            &policy_invalid,
            nullptr,
            0U,
            &policy_invalid_size) != PPX_RESULT_POLICY_VIOLATION ||
        policy_invalid_size == 0U) {
        return false;
    }

    if (ppx_validate_foundation_report_v1(
            nullptr,
            nullptr,
            0U,
            &required_size) != PPX_RESULT_RUNTIME_ERROR ||
        ppx_validate_foundation_report_v1(
            &report,
            nullptr,
            1U,
            &required_size) != PPX_RESULT_RUNTIME_ERROR ||
        ppx_validate_foundation_report_v1(
            &report,
            output.data(),
            0U,
            &required_size) != PPX_RESULT_RUNTIME_ERROR) {
        return false;
    }

    PpxFoundationReportV1 unsupported = report;
    unsupported.abi_version = PPX_ABI_VERSION_V1 + 1U;
    if (ppx_validate_foundation_report_v1(
            &unsupported,
            nullptr,
            0U,
            &required_size) != PPX_RESULT_UNSUPPORTED_VERSION) {
        return false;
    }

    PpxFoundationReportV1 invalid_flag = report;
    invalid_flag.foundation_safe = 2U;
    if (ppx_validate_foundation_report_v1(
            &invalid_flag,
            nullptr,
            0U,
            &required_size) != PPX_RESULT_RUNTIME_ERROR) {
        return false;
    }

    return true;
}

}  // namespace

extern "C" std::int32_t ppx_validate_foundation_report_v1(
    const PpxFoundationReportV1* report,
    char* output_utf8,
    const std::size_t output_capacity,
    std::size_t* required_output_bytes) noexcept {
    try {
        if (required_output_bytes == nullptr ||
            !BufferArgumentsAreValid(output_utf8, output_capacity)) {
            return PPX_RESULT_RUNTIME_ERROR;
        }

        *required_output_bytes = 0U;

        if (report == nullptr) {
            return PPX_RESULT_RUNTIME_ERROR;
        }

        if (report->abi_version != PPX_ABI_VERSION_V1) {
            return PPX_RESULT_UNSUPPORTED_VERSION;
        }

        if (!AbiFlagsAreValid(*report) || !AbiMetricsAreValid(*report)) {
            return PPX_RESULT_RUNTIME_ERROR;
        }

        const FoundationReport converted = ToFoundationReport(*report);
        const bool policy_valid = IsFoundationReportValid(converted);
        const std::string document =
            SerializeDiagnosticEnvelope(*report, converted, policy_valid);

        *required_output_bytes = document.size();

        if (output_capacity < document.size() + 1U) {
            return PPX_RESULT_BUFFER_TOO_SMALL;
        }

        if (!WriteWholeDocument(document, output_utf8, output_capacity)) {
            return PPX_RESULT_RUNTIME_ERROR;
        }

        return policy_valid ? PPX_RESULT_VALID : PPX_RESULT_POLICY_VIOLATION;
    } catch (...) {
        return PPX_RESULT_RUNTIME_ERROR;
    }
}

extern "C" std::int32_t ppx_foundation_c_api_self_test_v1() noexcept {
    try {
        return RunSelfTest() ? PPX_RESULT_VALID : PPX_RESULT_RUNTIME_ERROR;
    } catch (...) {
        return PPX_RESULT_RUNTIME_ERROR;
    }
}
