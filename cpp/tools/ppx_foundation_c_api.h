// File: cpp/tools/ppx_foundation_c_api.h
#ifndef PROMETHEUS_PRAXIS_PPX_FOUNDATION_C_API_H
#define PROMETHEUS_PRAXIS_PPX_FOUNDATION_C_API_H

#include <stddef.h>
#include <stdint.h>

#define PPX_ABI_VERSION_V1 1u
#define PPX_FOUNDATION_DIAGNOSTIC_SCHEMA_VERSION_V1 1u

#define PPX_RESULT_VALID 0
#define PPX_RESULT_RUNTIME_ERROR 1
#define PPX_RESULT_POLICY_VIOLATION 2
#define PPX_RESULT_BUFFER_TOO_SMALL 3
#define PPX_RESULT_UNSUPPORTED_VERSION 4

#ifdef __cplusplus
extern "C" {
#endif

struct PpxFoundationReportV1 {
    uint32_t abi_version;
    uint8_t private_heat_accepted;
    uint8_t threat_fail_closed;
    uint8_t water_biodiversity_allowed;
    uint8_t water_biodiversity_invariant_holds;
    uint8_t authorization_accepted;
    uint8_t invasive_control_safe;
    uint8_t irrigation_robustly_feasible;
    double maximum_risk_of_harm;
    double knowledge_factor;
    double eco_impact_value;
    uint8_t foundation_safe;
};

int32_t ppx_validate_foundation_report_v1(
    const struct PpxFoundationReportV1* report,
    char* output_utf8,
    size_t output_capacity,
    size_t* required_output_bytes)
#ifdef __cplusplus
    noexcept
#endif
    ;

int32_t ppx_foundation_c_api_self_test_v1(void)
#ifdef __cplusplus
    noexcept
#endif
    ;

#ifdef __cplusplus
}

#include <cstddef>
#include <type_traits>

static_assert(std::is_standard_layout_v<PpxFoundationReportV1>);
static_assert(std::is_trivially_copyable_v<PpxFoundationReportV1>);
static_assert(sizeof(uint8_t) == 1U);
static_assert(sizeof(uint32_t) == 4U);
static_assert(sizeof(double) == 8U);

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
#endif

#endif
