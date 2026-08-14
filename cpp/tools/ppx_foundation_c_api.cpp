// File: cpp/tools/ppx_foundation_c_api.cpp
#include "ppx_foundation_c_api.h"

#include "foundation_report.hpp"
#include "foundation_report_json.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>

static_assert(sizeof(std::uint8_t) == 1U);
static_assert(sizeof(std::uint32_t) == 4U);
static_assert(offsetof(PpxFoundationReportV1, abi_version) == 0U);
static_assert(offsetof(PpxFoundationReportV1, maximum_risk_of_harm) >= 8U);
static_assert(offsetof(PpxFoundationReportV1, knowledge_factor) >
              offsetof(PpxFoundationReportV1, maximum_risk_of_harm));
static_assert(offsetof(PpxFoundationReportV1, eco_impact_value) >
              offsetof(PpxFoundationReportV1, knowledge_factor));
static_assert(offsetof(PpxFoundationReportV1, foundation_safe) >
              offsetof(PpxFoundationReportV1, eco_impact_value));

namespace prometheus_praxis::foundation::c_api {
namespace {

bool IsCompleteOutputRequest(
    const char* output_utf8,
    std::size_t output_capacity) noexcept {
    return output_utf8 != nullptr && output_capacity > 0U;
}

FoundationReport ConvertAbiReport(
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

bool HasOnlyBinaryFlags(const PpxFoundationReportV1& report) noexcept {
    return IsAbiBinaryFlag(report.private_heat_accepted) &&
           IsAbiBinaryFlag(report.threat_fail_closed) &&
           IsAbiBinaryFlag(report.water_biodiversity_allowed) &&
           IsAbiBinaryFlag(report.water_biodiversity_invariant_holds) &&
           IsAbiBinaryFlag(report.authorization_accepted) &&
           IsAbiBinaryFlag(report.invasive_control_safe) &&
           IsAbiBinaryFlag(report.irrigation_robustly_feasible) &&
           IsAbiBinaryFlag(report.foundation_safe);
}

bool HasOnlyUnitScores(const PpxFoundationReportV1& report) noexcept {
    return IsAbiUnitScore(report.maximum_risk_of_harm) &&
           IsAbiUnitScore(report.knowledge_factor) &&
           IsAbiUnitScore(report.eco_impact_value);
}

bool IsAbiStructurallyValid(const PpxFoundationReportV1& report) noexcept {
    return report.abi_version == PPX_ABI_VERSION_V1 &&
           HasOnlyBinaryFlags(report) &&
           HasOnlyUnitScores(report);
}

bool WriteCompleteDocument(
    const std::string& document,
    char* output_utf8,
    std::size_t output_capacity) noexcept {
    if (!IsCompleteOutputRequest(output_utf8, output_capacity) ||
        output_capacity <= document.size()) {
        return false;
    }

    std::memcpy(output_utf8, document.data(), document.size());
    output_utf8[document.size()] = '\0';
    return true;
}

std::int32_t ValidateAndSerialize(
    const PpxFoundationReportV1* abi_report,
    char* output_utf8,
    std::size_t output_capacity,
    std::size_t* required_output_bytes) {
    if (required_output_bytes != nullptr) {
        *required_output_bytes = 0U;
    }

    if (abi_report == nullptr) {
        return PPX_RESULT_RUNTIME_ERROR;
    }

    if (abi_report->abi_version != PPX_ABI_VERSION_V1) {
        return PPX_RESULT_UNSUPPORTED_VERSION;
    }

    if (!IsAbiStructurallyValid(*abi_report)) {
        return PPX_RESULT_POLICY_VIOLATION;
    }

    if ((output_utf8 == nullptr && output_capacity != 0U) ||
        (output_utf8 != nullptr && output_capacity == 0U)) {
        return PPX_RESULT_RUNTIME_ERROR;
    }

    const FoundationReport report = ConvertAbiReport(*abi_report);
    const bool policy_valid =
        IsFoundationReportValid(report) && report.foundation_safe;

    const std::string document = SerializeFoundationReportAbiEnvelope(
        *abi_report,
        report,
        policy_valid);

    if (document.empty()) {
        return PPX_RESULT_RUNTIME_ERROR;
    }

    if (required_output_bytes != nullptr) {
        *required_output_bytes = document.size();
    }

    if (output_utf8 == nullptr) {
        return policy_valid ? PPX_RESULT_VALID : PPX_RESULT_POLICY_VIOLATION;
    }

    if (!WriteCompleteDocument(document, output_utf8, output_capacity)) {
        return PPX_RESULT_BUFFER_TOO_SMALL;
    }

    return policy_valid ? PPX_RESULT_VALID : PPX_RESULT_POLICY_VIOLATION;
}

}  // namespace

bool IsAbiBinaryFlag(std::uint8_t value) noexcept {
    return value == 0U || value == 1U;
}

bool IsAbiUnitScore(double value) noexcept {
    return std::isfinite(value) && value >= 0.0 && value <= 1.0;
}

PpxFoundationReportV1 MakeValidAbiReport() {
    return PpxFoundationReportV1{
        PPX_ABI_VERSION_V1,
        1U,
        1U,
        1U,
        1U,
        1U,
        1U,
        1U,
        0.200000,
        0.850000,
        0.900000,
        1U};
}

std::string SerializeFoundationReportAbiEnvelope(
    const PpxFoundationReportV1& abi_report,
    const FoundationReport& report,
    bool policy_valid) {
    if (abi_report.abi_version != PPX_ABI_VERSION_V1) {
        return {};
    }

    return json::SerializeFoundationReportEnvelope(
        report,
        "ppx_foundation_c_api_v1",
        "ppx_foundation_diagnostic_schema_v1",
        policy_valid);
}

bool CApiSelfTest() {
    PpxFoundationReportV1 valid = MakeValidAbiReport();

    std::size_t required_bytes = 0U;
    if (ValidateAndSerialize(&valid, nullptr, 0U, &required_bytes) !=
            PPX_RESULT_VALID ||
        required_bytes == 0U) {
        return false;
    }

    std::string exact_buffer(required_bytes + 1U, '\x5a');
    std::size_t exact_required = 0U;
    if (ValidateAndSerialize(
            &valid,
            exact_buffer.data(),
            exact_buffer.size(),
            &exact_required) != PPX_RESULT_VALID ||
        exact_required != required_bytes ||
        exact_buffer[required_bytes] != '\0' ||
        exact_buffer.find("\"abi_version\":\"ppx_foundation_c_api_v1\"") ==
            std::string::npos ||
        exact_buffer.find("\"policy_valid\":true") == std::string::npos) {
        return false;
    }

    std::string short_buffer(required_bytes, '\x5a');
    const std::string original_short_buffer = short_buffer;
    if (ValidateAndSerialize(
            &valid,
            short_buffer.data(),
            short_buffer.size(),
            nullptr) != PPX_RESULT_BUFFER_TOO_SMALL ||
        short_buffer != original_short_buffer) {
        return false;
    }

    PpxFoundationReportV1 unsafe = valid;
    unsafe.maximum_risk_of_harm = 0.31;
    unsafe.foundation_safe = 0U;
    std::size_t unsafe_required = 0U;
    if (ValidateAndSerialize(
            &unsafe,
            nullptr,
            0U,
            &unsafe_required) != PPX_RESULT_POLICY_VIOLATION ||
        unsafe_required == 0U) {
        return false;
    }

    PpxFoundationReportV1 invalid_flag = valid;
    invalid_flag.authorization_accepted = 2U;
    if (ValidateAndSerialize(
            &invalid_flag,
            nullptr,
            0U,
            nullptr) != PPX_RESULT_POLICY_VIOLATION) {
        return false;
    }

    PpxFoundationReportV1 invalid_metric = valid;
    invalid_metric.knowledge_factor =
        std::numeric_limits<double>::quiet_NaN();
    if (ValidateAndSerialize(
            &invalid_metric,
            nullptr,
            0U,
            nullptr) != PPX_RESULT_POLICY_VIOLATION) {
        return false;
    }

    PpxFoundationReportV1 unsupported_version = valid;
    unsupported_version.abi_version = PPX_ABI_VERSION_V1 + 1U;
    if (ValidateAndSerialize(
            &unsupported_version,
            nullptr,
            0U,
            nullptr) != PPX_RESULT_UNSUPPORTED_VERSION) {
        return false;
    }

    std::size_t ignored_required = 99U;
    if (ValidateAndSerialize(
            nullptr,
            nullptr,
            0U,
            &ignored_required) != PPX_RESULT_RUNTIME_ERROR ||
        ignored_required != 0U ||
        ValidateAndSerialize(
            &valid,
            nullptr,
            1U,
            nullptr) != PPX_RESULT_RUNTIME_ERROR ||
        ValidateAndSerialize(
            &valid,
            exact_buffer.data(),
            0U,
            nullptr) != PPX_RESULT_RUNTIME_ERROR) {
        return false;
    }

    return IsAbiBinaryFlag(0U) &&
           IsAbiBinaryFlag(1U) &&
           !IsAbiBinaryFlag(2U) &&
           IsAbiUnitScore(0.0) &&
           IsAbiUnitScore(1.0) &&
           !IsAbiUnitScore(std::numeric_limits<double>::infinity());
}

}  // namespace prometheus_praxis::foundation::c_api

extern "C" std::int32_t ppx_validate_foundation_report_v1(
    const PpxFoundationReportV1* report,
    char* output_utf8,
    std::size_t output_capacity,
    std::size_t* required_output_bytes) noexcept {
    try {
        return prometheus_praxis::foundation::c_api::ValidateAndSerialize(
            report,
            output_utf8,
            output_capacity,
            required_output_bytes);
    } catch (...) {
        if (required_output_bytes != nullptr) {
            *required_output_bytes = 0U;
        }
        return PPX_RESULT_RUNTIME_ERROR;
    }
}

extern "C" std::int32_t ppx_foundation_c_api_self_test_v1() noexcept {
    try {
        return prometheus_praxis::foundation::c_api::CApiSelfTest()
            ? PPX_RESULT_VALID
            : PPX_RESULT_RUNTIME_ERROR;
    } catch (...) {
        return PPX_RESULT_RUNTIME_ERROR;
    }
}
