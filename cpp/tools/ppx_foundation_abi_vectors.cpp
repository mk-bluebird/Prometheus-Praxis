// File: cpp/tools/ppx_foundation_abi_vectors.cpp
#include "ppx_foundation_c_api.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace prometheus_praxis::foundation::abi_vectors {

struct FoundationAbiByteVector {
    std::string name;
    std::vector<std::uint8_t> raw_bytes;
    std::uint32_t expected_abi_version{PPX_ABI_VERSION_V1};
    std::int32_t expected_result_code{};
    std::size_t expected_required_output_bytes{};
    bool output_is_complete{};
    bool output_is_unchanged_on_failure{};
};

struct FoundationAbiVectorSet {
    std::uint32_t abi_version{PPX_ABI_VERSION_V1};
    std::uint32_t diagnostic_schema_version{
        PPX_FOUNDATION_DIAGNOSTIC_SCHEMA_VERSION_V1};
    std::vector<FoundationAbiByteVector> vectors;
};

namespace {

static_assert(sizeof(PpxFoundationReportV1) >= 40U);
static_assert(sizeof(double) == 8U);

constexpr std::size_t kFlagCount = 8U;
constexpr std::size_t kMetricCount = 3U;

void AppendU8(std::vector<std::uint8_t>& bytes, std::uint8_t value) {
    bytes.push_back(value);
}

void AppendU32LittleEndian(
    std::vector<std::uint8_t>& bytes,
    std::uint32_t value) {
    for (unsigned int shift = 0U; shift < 32U; shift += 8U) {
        bytes.push_back(static_cast<std::uint8_t>((value >> shift) & 0xffU));
    }
}

void AppendU64LittleEndian(
    std::vector<std::uint8_t>& bytes,
    std::uint64_t value) {
    for (unsigned int shift = 0U; shift < 64U; shift += 8U) {
        bytes.push_back(static_cast<std::uint8_t>((value >> shift) & 0xffU));
    }
}

void AppendDoubleLittleEndian(
    std::vector<std::uint8_t>& bytes,
    double value) {
    std::uint64_t representation{};
    std::memcpy(&representation, &value, sizeof(representation));
    AppendU64LittleEndian(bytes, representation);
}

std::uint32_t ReadU32LittleEndian(
    const std::vector<std::uint8_t>& bytes,
    std::size_t offset) {
    if (offset + 4U > bytes.size()) {
        return 0U;
    }

    std::uint32_t value = 0U;
    for (unsigned int shift = 0U; shift < 32U; shift += 8U) {
        value |= static_cast<std::uint32_t>(bytes[offset++]) << shift;
    }
    return value;
}

std::vector<std::uint8_t> BuildReportBytes(
    std::uint32_t abi_version,
    std::array<std::uint8_t, kFlagCount> flags,
    double risk,
    double knowledge,
    double impact) {
    std::vector<std::uint8_t> bytes;
    bytes.reserve(4U + kFlagCount + (kMetricCount * sizeof(double)));

    AppendU32LittleEndian(bytes, abi_version);
    for (const std::uint8_t flag : flags) {
        AppendU8(bytes, flag);
    }
    AppendDoubleLittleEndian(bytes, risk);
    AppendDoubleLittleEndian(bytes, knowledge);
    AppendDoubleLittleEndian(bytes, impact);

    return bytes;
}

std::size_t ExpectedEnvelopeBytes(bool policy_valid) {
    const std::string report_json =
        "{\"private_heat_accepted\":true,"
        "\"threat_fail_closed\":true,"
        "\"water_biodiversity_allowed\":true,"
        "\"water_biodiversity_invariant_holds\":true,"
        "\"authorization_accepted\":true,"
        "\"invasive_control_safe\":true,"
        "\"irrigation_robustly_feasible\":true,"
        "\"maximum_risk_of_harm\":0.200000,"
        "\"knowledge_factor\":0.850000,"
        "\"eco_impact_value\":0.900000,"
        "\"foundation_safe\":true}";

    const std::string envelope =
        "{\"abi_version\":\"ppx_foundation_c_api_v1\","
        "\"diagnostic_schema_version\":"
        "\"ppx_foundation_diagnostic_schema_v1\","
        "\"policy_valid\":" +
        std::string(policy_valid ? "true" : "false") +
        ",\"report\":" + report_json + "}";

    return envelope.size();
}

bool IsExpectedWireLayout(
    const std::vector<std::uint8_t>& bytes,
    std::uint32_t expected_version) {
    constexpr std::size_t kWireSize =
        4U + kFlagCount + (kMetricCount * sizeof(double));

    if (bytes.size() != kWireSize ||
        ReadU32LittleEndian(bytes, 0U) != expected_version) {
        return false;
    }

    for (std::size_t index = 0U; index < kFlagCount; ++index) {
        if (bytes[4U + index] > 1U) {
            return false;
        }
    }

    return true;
}

}  // namespace

std::vector<std::uint8_t> BuildValidAbiReportBytes() {
    return BuildReportBytes(
        PPX_ABI_VERSION_V1,
        {1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U},
        0.200000,
        0.850000,
        0.900000);
}

std::vector<std::uint8_t> BuildInvalidPolicyAbiReportBytes() {
    return BuildReportBytes(
        PPX_ABI_VERSION_V1,
        {1U, 1U, 1U, 1U, 1U, 1U, 1U, 0U},
        0.200000,
        0.850000,
        0.900000);
}

std::vector<std::uint8_t> BuildUnsupportedAbiReportBytes() {
    return BuildReportBytes(
        PPX_ABI_VERSION_V1 + 1U,
        {1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U},
        0.200000,
        0.850000,
        0.900000);
}

FoundationAbiVectorSet BuildFoundationAbiVectorSet() {
    const std::size_t valid_required_bytes = ExpectedEnvelopeBytes(true);
    const std::size_t policy_required_bytes = ExpectedEnvelopeBytes(false);

    return FoundationAbiVectorSet{
        PPX_ABI_VERSION_V1,
        PPX_FOUNDATION_DIAGNOSTIC_SCHEMA_VERSION_V1,
        {
            FoundationAbiByteVector{
                "valid_report_size_query",
                BuildValidAbiReportBytes(),
                PPX_ABI_VERSION_V1,
                PPX_RESULT_VALID,
                valid_required_bytes,
                true,
                false},
            FoundationAbiByteVector{
                "valid_report_short_buffer",
                BuildValidAbiReportBytes(),
                PPX_ABI_VERSION_V1,
                PPX_RESULT_BUFFER_TOO_SMALL,
                valid_required_bytes,
                false,
                true},
            FoundationAbiByteVector{
                "invalid_policy_report",
                BuildInvalidPolicyAbiReportBytes(),
                PPX_ABI_VERSION_V1,
                PPX_RESULT_POLICY_VIOLATION,
                policy_required_bytes,
                true,
                false},
            FoundationAbiByteVector{
                "unsupported_abi_version",
                BuildUnsupportedAbiReportBytes(),
                PPX_ABI_VERSION_V1 + 1U,
                PPX_RESULT_UNSUPPORTED_VERSION,
                0U,
                false,
                true},
        }};
}

std::string DescribeFoundationAbiVector(
    const FoundationAbiByteVector& vector) {
    std::ostringstream output;
    output << "foundation_abi_vector"
           << "; name=" << vector.name
           << "; byte_order=little_endian"
           << "; raw_byte_count=" << vector.raw_bytes.size()
           << "; expected_abi_version=" << vector.expected_abi_version
           << "; expected_result_code=" << vector.expected_result_code
           << "; expected_required_output_bytes="
           << vector.expected_required_output_bytes
           << "; output_is_complete="
           << (vector.output_is_complete ? "true" : "false")
           << "; output_is_unchanged_on_failure="
           << (vector.output_is_unchanged_on_failure ? "true" : "false");
    return output.str();
}

bool FoundationCrossLanguageAbiVectorsSelfTest() {
    const std::vector<std::uint8_t> valid = BuildValidAbiReportBytes();
    const std::vector<std::uint8_t> invalid = BuildInvalidPolicyAbiReportBytes();
    const std::vector<std::uint8_t> unsupported = BuildUnsupportedAbiReportBytes();

    if (!IsExpectedWireLayout(valid, PPX_ABI_VERSION_V1) ||
        !IsExpectedWireLayout(invalid, PPX_ABI_VERSION_V1) ||
        !IsExpectedWireLayout(
            unsupported,
            PPX_ABI_VERSION_V1 + 1U) ||
        valid == invalid ||
        valid == unsupported) {
        return false;
    }

    const FoundationAbiVectorSet set = BuildFoundationAbiVectorSet();
    if (set.abi_version != PPX_ABI_VERSION_V1 ||
        set.diagnostic_schema_version !=
            PPX_FOUNDATION_DIAGNOSTIC_SCHEMA_VERSION_V1 ||
        set.vectors.size() != 4U) {
        return false;
    }

    const FoundationAbiByteVector& valid_vector = set.vectors[0];
    const FoundationAbiByteVector& short_vector = set.vectors[1];
    const FoundationAbiByteVector& invalid_vector = set.vectors[2];
    const FoundationAbiByteVector& unsupported_vector = set.vectors[3];

    if (valid_vector.expected_result_code != PPX_RESULT_VALID ||
        !valid_vector.output_is_complete ||
        valid_vector.output_is_unchanged_on_failure ||
        valid_vector.expected_required_output_bytes == 0U ||
        short_vector.expected_result_code != PPX_RESULT_BUFFER_TOO_SMALL ||
        short_vector.output_is_complete ||
        !short_vector.output_is_unchanged_on_failure ||
        short_vector.expected_required_output_bytes !=
            valid_vector.expected_required_output_bytes ||
        invalid_vector.expected_result_code != PPX_RESULT_POLICY_VIOLATION ||
        !invalid_vector.output_is_complete ||
        invalid_vector.expected_required_output_bytes == 0U ||
        unsupported_vector.expected_result_code !=
            PPX_RESULT_UNSUPPORTED_VERSION ||
        unsupported_vector.expected_required_output_bytes != 0U ||
        !unsupported_vector.output_is_unchanged_on_failure) {
        return false;
    }

    return DescribeFoundationAbiVector(valid_vector).find(
               "byte_order=little_endian") != std::string::npos;
}

}  // namespace prometheus_praxis::foundation::abi_vectors
