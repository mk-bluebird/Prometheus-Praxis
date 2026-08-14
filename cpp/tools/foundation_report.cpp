// File: cpp/tools/foundation_report.cpp
#include "foundation_report.hpp"

#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace prometheus_praxis::foundation {
namespace {

constexpr double kMinimumScore = 0.0;
constexpr double kMaximumScore = 1.0;
constexpr double kMaximumSafeRiskOfHarm = 0.30;

bool IsFiniteUnitInterval(double value) noexcept {
    return std::isfinite(value) &&
           value >= kMinimumScore &&
           value <= kMaximumScore;
}

bool IsFiniteNonNegative(double value) noexcept {
    return std::isfinite(value) && value >= 0.0;
}

bool ThreatFailClosedBlocksThreats(const FoundationReport& report) noexcept {
    return report.threat_fail_closed;
}

bool HasAllSafeConditions(const FoundationReport& report) noexcept {
    return report.private_heat_accepted &&
           ThreatFailClosedBlocksThreats(report) &&
           report.water_biodiversity_allowed &&
           report.water_biodiversity_invariant_holds &&
           report.authorization_accepted &&
           report.invasive_control_safe &&
           report.irrigation_robustly_feasible;
}

std::string BooleanText(bool value) {
    return value ? "true" : "false";
}

std::string FormatDouble(double value) {
    if (std::isnan(value)) {
        return "nan";
    }
    if (std::isinf(value)) {
        return value > 0.0 ? "infinity" : "-infinity";
    }

    std::ostringstream output;
    output << std::setprecision(17) << value;
    return output.str();
}

void AddDifference(
    FoundationReportDiff& diff,
    std::string_view field,
    bool left,
    bool right) {
    if (left != right) {
        diff.differences.emplace_back(
            std::string(field) + " differs; left=" + BooleanText(left) +
            "; right=" + BooleanText(right));
    }
}

void AddMetricDifference(
    FoundationReportDiff& diff,
    std::string_view field,
    double left,
    double right,
    double tolerance) {
    if (!std::isfinite(left) || !std::isfinite(right)) {
        if (std::isnan(left) || std::isnan(right) || left != right) {
            diff.differences.emplace_back(
                std::string(field) + " non-finite comparison; left=" +
                FormatDouble(left) + "; right=" + FormatDouble(right));
        }
        return;
    }

    if (std::fabs(left - right) > tolerance) {
        diff.differences.emplace_back(
            std::string(field) + " differs; left=" + FormatDouble(left) +
            "; right=" + FormatDouble(right) +
            "; tolerance=" + FormatDouble(tolerance));
    }
}

void AddMetricValidation(
    FoundationReportValidation& validation,
    std::string_view field,
    double value) {
    if (!std::isfinite(value)) {
        validation.reasons.emplace_back(
            std::string(field) + " must be finite");
        return;
    }

    if (value < kMinimumScore || value > kMaximumScore) {
        validation.reasons.emplace_back(
            std::string(field) + " must be within [0,1]");
    }
}

}  // namespace

FoundationReportBuilder& FoundationReportBuilder::SetPrivateHeat(bool accepted) {
    report.private_heat_accepted = accepted;
    return *this;
}

FoundationReportBuilder& FoundationReportBuilder::SetContainmentBlocked(bool blocked) {
    report.threat_fail_closed = blocked;
    return *this;
}

FoundationReportBuilder& FoundationReportBuilder::SetWaterAllowed(bool allowed) {
    report.water_biodiversity_allowed = allowed;
    return *this;
}

FoundationReportBuilder& FoundationReportBuilder::SetWaterInvariant(bool holds) {
    report.water_biodiversity_invariant_holds = holds;
    return *this;
}

FoundationReportBuilder& FoundationReportBuilder::SetAuthorizationAccepted(bool accepted) {
    report.authorization_accepted = accepted;
    return *this;
}

FoundationReportBuilder& FoundationReportBuilder::SetInvasiveSafe(bool safe) {
    report.invasive_control_safe = safe;
    return *this;
}

FoundationReportBuilder& FoundationReportBuilder::SetIrrigationFeasible(bool feasible) {
    report.irrigation_robustly_feasible = feasible;
    return *this;
}

FoundationReportBuilder& FoundationReportBuilder::SetScores(
    double risk,
    double knowledge,
    double impact) {
    report.maximum_risk_of_harm = risk;
    report.knowledge_factor = knowledge;
    report.eco_impact_value = impact;
    return *this;
}

FoundationReport FoundationReportBuilder::Build() {
    report.foundation_safe = DerivedFoundationSafe(report);
    return report;
}

bool DerivedFoundationSafe(const FoundationReport& report) {
    return IsFiniteUnitInterval(report.maximum_risk_of_harm) &&
           IsFiniteUnitInterval(report.knowledge_factor) &&
           IsFiniteUnitInterval(report.eco_impact_value) &&
           report.maximum_risk_of_harm <= kMaximumSafeRiskOfHarm &&
           HasAllSafeConditions(report);
}

FoundationReportValidation ValidateFoundationReport(
    const FoundationReport& report) {
    FoundationReportValidation validation;

    AddMetricValidation(
        validation,
        "maximum_risk_of_harm",
        report.maximum_risk_of_harm);
    AddMetricValidation(
        validation,
        "knowledge_factor",
        report.knowledge_factor);
    AddMetricValidation(
        validation,
        "eco_impact_value",
        report.eco_impact_value);

    const bool derived_safe = DerivedFoundationSafe(report);
    if (report.foundation_safe != derived_safe) {
        validation.reasons.emplace_back(
            "foundation_safe does not equal DerivedFoundationSafe");
    }

    if (!derived_safe) {
        if (!report.private_heat_accepted) {
            validation.reasons.emplace_back(
                "unsafe: private heat was not accepted");
        }
        if (!ThreatFailClosedBlocksThreats(report)) {
            validation.reasons.emplace_back(
                "unsafe: threat handling is not fail-closed");
        }
        if (!report.water_biodiversity_allowed) {
            validation.reasons.emplace_back(
                "unsafe: water biodiversity activity is not allowed");
        }
        if (!report.water_biodiversity_invariant_holds) {
            validation.reasons.emplace_back(
                "unsafe: water biodiversity invariant does not hold");
        }
        if (!report.authorization_accepted) {
            validation.reasons.emplace_back(
                "unsafe: authorization was not accepted");
        }
        if (!report.invasive_control_safe) {
            validation.reasons.emplace_back(
                "unsafe: invasive control is not safe");
        }
        if (!report.irrigation_robustly_feasible) {
            validation.reasons.emplace_back(
                "unsafe: irrigation is not robustly feasible");
        }
        if (std::isfinite(report.maximum_risk_of_harm) &&
            report.maximum_risk_of_harm > kMaximumSafeRiskOfHarm) {
            validation.reasons.emplace_back(
                "unsafe: maximum_risk_of_harm exceeds 0.30");
        }
    }

    validation.valid =
        IsFiniteUnitInterval(report.maximum_risk_of_harm) &&
        IsFiniteUnitInterval(report.knowledge_factor) &&
        IsFiniteUnitInterval(report.eco_impact_value) &&
        report.foundation_safe == derived_safe;

    return validation;
}

bool IsFoundationReportValid(const FoundationReport& report) {
    return ValidateFoundationReport(report).valid;
}

std::string ExplainFoundationReportValidation(
    const FoundationReport& report) {
    const FoundationReportValidation validation =
        ValidateFoundationReport(report);

    std::ostringstream output;
    output << "foundation_report="
           << (validation.valid ? "valid" : "invalid")
           << "; derived_safe="
           << BooleanText(DerivedFoundationSafe(report))
           << "; declared_safe="
           << BooleanText(report.foundation_safe);

    for (const std::string& reason : validation.reasons) {
        output << "; reason=" << reason;
    }

    return output.str();
}

FoundationReportDiff CompareFoundationReports(
    const FoundationReport& left,
    const FoundationReport& right,
    double tolerance) {
    FoundationReportDiff diff;

    if (!IsFiniteNonNegative(tolerance)) {
        diff.differences.emplace_back(
            "comparison tolerance must be finite and non-negative; value=" +
            FormatDouble(tolerance));
        diff.identical = false;
        return diff;
    }

    AddDifference(
        diff,
        "private_heat_accepted",
        left.private_heat_accepted,
        right.private_heat_accepted);
    AddDifference(
        diff,
        "threat_fail_closed",
        left.threat_fail_closed,
        right.threat_fail_closed);
    AddDifference(
        diff,
        "water_biodiversity_allowed",
        left.water_biodiversity_allowed,
        right.water_biodiversity_allowed);
    AddDifference(
        diff,
        "water_biodiversity_invariant_holds",
        left.water_biodiversity_invariant_holds,
        right.water_biodiversity_invariant_holds);
    AddDifference(
        diff,
        "authorization_accepted",
        left.authorization_accepted,
        right.authorization_accepted);
    AddDifference(
        diff,
        "invasive_control_safe",
        left.invasive_control_safe,
        right.invasive_control_safe);
    AddDifference(
        diff,
        "irrigation_robustly_feasible",
        left.irrigation_robustly_feasible,
        right.irrigation_robustly_feasible);
    AddMetricDifference(
        diff,
        "maximum_risk_of_harm",
        left.maximum_risk_of_harm,
        right.maximum_risk_of_harm,
        tolerance);
    AddMetricDifference(
        diff,
        "knowledge_factor",
        left.knowledge_factor,
        right.knowledge_factor,
        tolerance);
    AddMetricDifference(
        diff,
        "eco_impact_value",
        left.eco_impact_value,
        right.eco_impact_value,
        tolerance);
    AddDifference(
        diff,
        "foundation_safe",
        left.foundation_safe,
        right.foundation_safe);

    diff.identical = diff.differences.empty();
    return diff;
}

std::string ExplainFoundationReportDiff(
    const FoundationReportDiff& diff) {
    if (diff.identical) {
        return "foundation reports are identical";
    }

    std::ostringstream output;
    output << "foundation reports differ; count="
           << diff.differences.size();

    for (const std::string& difference : diff.differences) {
        output << "; difference=" << difference;
    }

    return output.str();
}

bool FoundationReportValidatorSelfTest() {
    const FoundationReport safe =
        FoundationReportBuilder{}
            .SetPrivateHeat(true)
            .SetContainmentBlocked(true)
            .SetWaterAllowed(true)
            .SetWaterInvariant(true)
            .SetAuthorizationAccepted(true)
            .SetInvasiveSafe(true)
            .SetIrrigationFeasible(true)
            .SetScores(0.25, 0.90, 0.85)
            .Build();

    if (!safe.foundation_safe ||
        !IsFoundationReportValid(safe) ||
        !ValidateFoundationReport(safe).reasons.empty()) {
        return false;
    }

    FoundationReport unsafe_risk = safe;
    unsafe_risk.maximum_risk_of_harm = 0.31;
    unsafe_risk.foundation_safe = false;
    const FoundationReportValidation unsafe_risk_validation =
        ValidateFoundationReport(unsafe_risk);
    if (!unsafe_risk_validation.valid ||
        DerivedFoundationSafe(unsafe_risk) ||
        unsafe_risk_validation.reasons.empty()) {
        return false;
    }

    FoundationReport non_finite = safe;
    non_finite.knowledge_factor =
        std::numeric_limits<double>::quiet_NaN();
    non_finite.foundation_safe = false;
    if (IsFoundationReportValid(non_finite)) {
        return false;
    }

    FoundationReport invalid_range = safe;
    invalid_range.maximum_risk_of_harm = -0.01;
    invalid_range.foundation_safe = false;
    if (IsFoundationReportValid(invalid_range)) {
        return false;
    }

    FoundationReport inconsistent = safe;
    inconsistent.foundation_safe = false;
    if (IsFoundationReportValid(inconsistent)) {
        return false;
    }

    const FoundationReportDiff exact =
        CompareFoundationReports(safe, safe, 0.0);
    if (!exact.identical || !exact.differences.empty()) {
        return false;
    }

    FoundationReport nearby = safe;
    nearby.knowledge_factor += 0.0005;
    if (!CompareFoundationReports(safe, nearby, 0.001).identical ||
        CompareFoundationReports(safe, nearby, 0.0001).identical) {
        return false;
    }

    FoundationReport nan_comparison = safe;
    nan_comparison.eco_impact_value =
        std::numeric_limits<double>::quiet_NaN();
    const FoundationReportDiff nan_diff =
        CompareFoundationReports(safe, nan_comparison, 0.0);
    if (nan_diff.identical || nan_diff.differences.empty()) {
        return false;
    }

    const FoundationReportDiff invalid_tolerance =
        CompareFoundationReports(safe, safe, -0.01);
    return !invalid_tolerance.identical &&
           !invalid_tolerance.differences.empty() &&
           ExplainFoundationReportDiff(nan_diff).find(
               "non-finite comparison") != std::string::npos;
}

}  // namespace prometheus_praxis::foundation
