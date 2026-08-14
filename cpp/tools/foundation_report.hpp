// File: cpp/tools/foundation_report.hpp
#pragma once

#include <string>
#include <vector>

struct FoundationReport {
    bool private_heat_accepted{};
    bool threat_fail_closed{};
    bool water_biodiversity_allowed{};
    bool water_biodiversity_invariant_holds{};
    bool authorization_accepted{};
    bool invasive_control_safe{};
    bool irrigation_robustly_feasible{};
    double maximum_risk_of_harm{};
    double knowledge_factor{};
    double eco_impact_value{};
    bool foundation_safe{};
};

struct FoundationReportDiff {
    std::vector<std::string> differences;
    bool identical{};
};

struct FoundationReportValidation {
    bool valid{};
    std::vector<std::string> reasons;
};

inline constexpr double kFoundationMaximumSafeRiskOfHarm = 0.30;

FoundationReportValidation ValidateFoundationReport(
    const FoundationReport& report);

bool IsFoundationReportValid(const FoundationReport& report);

std::string ExplainFoundationReportValidation(
    const FoundationReport& report);

FoundationReportDiff CompareFoundationReports(
    const FoundationReport& left,
    const FoundationReport& right,
    double tolerance);

std::string ExplainFoundationReportDiff(
    const FoundationReportDiff& diff);

bool FoundationReportValidatorSelfTest();
