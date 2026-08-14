// File: cpp/tools/foundation_report.hpp
#pragma once

#include <string>
#include <vector>

namespace prometheus_praxis::foundation {

inline constexpr double kFoundationMinimumScore = 0.0;
inline constexpr double kFoundationMaximumScore = 1.0;
inline constexpr double kFoundationMaximumSafeRiskOfHarm = 0.30;

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

struct FoundationReportValidation {
    bool valid{};
    std::vector<std::string> reasons;
};

struct FoundationReportDiff {
    std::vector<std::string> differences;
    bool identical{};
};

class FoundationReportBuilder {
public:
    FoundationReportBuilder() = default;

    FoundationReportBuilder& SetPrivateHeat(bool accepted);
    FoundationReportBuilder& SetContainmentBlocked(bool blocked);
    FoundationReportBuilder& SetWaterAllowed(bool allowed);
    FoundationReportBuilder& SetWaterInvariant(bool holds);
    FoundationReportBuilder& SetAuthorizationAccepted(bool accepted);
    FoundationReportBuilder& SetInvasiveSafe(bool safe);
    FoundationReportBuilder& SetIrrigationFeasible(bool feasible);

    FoundationReportBuilder& SetScores(
        double maximum_risk_of_harm,
        double knowledge_factor,
        double eco_impact_value);

    FoundationReport Build() const;

private:
    FoundationReport report_{};
};

FoundationReportValidation ValidateFoundationReport(
    const FoundationReport& report);

bool IsFoundationReportValid(const FoundationReport& report);

bool DerivedFoundationSafe(const FoundationReport& report);

std::string ExplainFoundationReportValidation(
    const FoundationReport& report);

FoundationReportDiff CompareFoundationReports(
    const FoundationReport& left,
    const FoundationReport& right,
    double tolerance);

std::string ExplainFoundationReportDiff(
    const FoundationReportDiff& diff);

bool FoundationReportValidatorSelfTest();

}  // namespace prometheus_praxis::foundation
