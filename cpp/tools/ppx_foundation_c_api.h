// File: cpp/tools/ppx_foundation_c_api.h
#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

#define PPX_ABI_VERSION_V1 1u
#define PPX_FOUNDATION_DIAGNOSTIC_SCHEMA_VERSION_V1 1u

#define PPX_RESULT_VALID 0
#define PPX_RESULT_RUNTIME_ERROR 1
#define PPX_RESULT_POLICY_VIOLATION 2
#define PPX_RESULT_BUFFER_TOO_SMALL 3
#define PPX_RESULT_UNSUPPORTED_VERSION 4

extern "C" {

struct PpxFoundationReportV1 {
    std::uint32_t abi_version;
    std::uint8_t private_heat_accepted;
    std::uint8_t threat_fail_closed;
    std::uint8_t water_biodiversity_allowed;
    std::uint8_t water_biodiversity_invariant_holds;
    std::uint8_t authorization_accepted;
    std::uint8_t invasive_control_safe;
    std::uint8_t irrigation_robustly_feasible;
    double maximum_risk_of_harm;
    double knowledge_factor;
    double eco_impact_value;
    std::uint8_t foundation_safe;
};

std::int32_t ppx_validate_foundation_report_v1(
    const PpxFoundationReportV1* report,
    char* output_utf8,
    std::size_t output_capacity,
    std::size_t* required_output_bytes) noexcept;

std::int32_t ppx_foundation_c_api_self_test_v1() noexcept;

}  // extern "C"

static_assert(std::is_standard_layout_v<PpxFoundationReportV1>);
static_assert(std::is_trivially_copyable_v<PpxFoundationReportV1>);
static_assert(sizeof(PpxFoundationReportV1) == 48U);
static_assert(alignof(PpxFoundationReportV1) == 8U);

static_assert(offsetof(PpxFoundationReportV1, abi_version) == 0U);
static_assert(offsetof(PpxFoundationReportV1, private_heat_accepted) == 4U);
static_assert(offsetof(PpxFoundationReportV1, threat_fail_closed) == 5U);
static_assert(
    offsetof(PpxFoundationReportV1, water_biodiversity_allowed) == 6U);
static_assert(
    offsetof(PpxFoundationReportV1, water_biodiversity_invariant_holds) == 7U);
static_assert(offsetof(PpxFoundationReportV1, authorization_accepted) == 8U);
static_assert(offsetof(PpxFoundationReportV1, invasive_control_safe) == 9U);
static_assert(
    offsetof(PpxFoundationReportV1, irrigation_robustly_feasible) == 10U);
static_assert(offsetof(PpxFoundationReportV1, maximum_risk_of_harm) == 16U);
static_assert(offsetof(PpxFoundationReportV1, knowledge_factor) == 24U);
static_assert(offsetof(PpxFoundationReportV1, eco_impact_value) == 32U);
static_assert(offsetof(PpxFoundationReportV1, foundation_safe) == 40U);
