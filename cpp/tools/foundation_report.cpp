// File: cpp/tools/foundation_report.cpp
#include "foundation_report.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr double kMinimumScore = 0.0;
constexpr double kMaximumScore = 1.0;

bool IsFiniteUnitInterval(const double value) noexcept {
    return std::isfinite(value) && value >= kMinimumScore &&
           value <= kMaximumScore;
}

bool AreEqual(const double left, const double right,
              const double tolerance) noexcept {
    return std::fabs(left - right) <= tolerance;
}

bool DerivedFoundationSafe(const FoundationReport& report) noexcept {
    return report.private_heat_accepted &&
           !report.threat_fail_closed &&
           report.water_biodiversity_allowed &&
           report.water_biodiversity_invariant_holds &&
           report.authorization_accepted &&
           report.invasive_control_safe &&
           report.irrigation_robustly_feasible &&
           std::isfinite(report.maximum_risk_of_harm) &&
           report.maximum_risk_of_harm <= kFoundationMaximumSafeRiskOfHarm;
}

void AddDifference(FoundationReportDiff& diff, const bool differs,
                   const std::string& field_name) {
    if (differs) {
        diff.differences.push_back(field_name);
    }
}

void AddBooleanFailure(std::vector<std::string>& reasons, const bool condition,
                       const char* reason) {
    if (!condition) {
        reasons.emplace_back(reason);
    }
}

void AddMetricFailure(std::vector<std::string>& reasons, const double value,
                      const char* name) {
    if (!std::isfinite(value)) {
        reasons.emplace_back(std::string(name) + " must be finite");
        return;
    }
    if (value < kMinimumScore || value > kMaximumScore) {
        reasons.emplace_back(std::string(name) + " must be in [0,1]");
    }
}

}  // namespace

FoundationReportValidation ValidateFoundationReport(
    const FoundationReport& report) {
    FoundationReportValidation result{};

    AddMetricFailure(
        result.reasons, report.maximum_risk_of_harm, "maximum_risk_of_harm");
    AddMetricFailure(result.reasons, report.knowledge_factor, "knowledge_factor");
    AddMetricFailure(result.reasons, report.eco_impact_value, "eco_impact_value");

    const bool derived_safe = DerivedFoundationSafe(report);

    if (report.foundation_safe != derived_safe) {
        result.reasons.emplace_back(
            "foundation_safe does not match the derived safety decision");
    }

    if (report.foundation_safe) {
        if (!report.private_heat_accepted) {
            result.reasons.emplace_back("private heat stage was not accepted");
        }
        if (report.threat_fail_closed) {
            result.reasons.emplace_back(
                "threat_fail_closed is true; its public semantics require "
                "source-backed reconciliation before exposure");
        }
        if (!report.water_biodiversity_allowed) {
            result.reasons.emplace_back(
                "water biodiversity stage was not allowed");
        }
        if (!report.water_biodiversity_invariant_holds) {
            result.reasons.emplace_back(
                "water biodiversity invariant does not hold");
        }
        if (!report.authorization_accepted) {
            result.reasons.emplace_back("authorization stage was not accepted");
        }
        if (!report.invasive_control_safe) {
            result.reasons.emplace_back("invasive control stage is not safe");
        }
        if (!report.irrigation_robustly_feasible) {
            result.reasons.emplace_back(
                "irrigation schedule is not robustly feasible");
        }
        if (std::isfinite(report.maximum_risk_of_harm) &&
            report.maximum_risk_of_harm > kFoundationMaximumSafeRiskOfHarm) {
            result.reasons.emplace_back(
                "maximum_risk_of_harm exceeds the safe threshold");
        }
    } else if (derived_safe) {
        result.reasons.emplace_back(
            "foundation_safe is false despite all safe-stage conditions");
    }

    result.valid = result.reasons.empty();
    return result;
}

bool IsFoundationReportValid(const FoundationReport& report) {
    return ValidateFoundationReport(report).valid;
}

std::string ExplainFoundationReportValidation(
    const FoundationReport& report) {
    const FoundationReportValidation validation = ValidateFoundationReport(report);

    std::ostringstream output;
    if (validation.valid) {
        output << "FoundationReport validation passed.";
        return output.str();
    }

    output << "FoundationReport validation failed:";
    for (const std::string& reason : validation.reasons) {
        output << '\n' << "- " << reason;
    }
    return output.str();
}

FoundationReportDiff CompareFoundationReports(
    const FoundationReport& left,
    const FoundationReport& right,
    const double tolerance) {
    FoundationReportDiff result{};

    if (!std::isfinite(tolerance) || tolerance < 0.0) {
        result.differences.emplace_back(
            "comparison tolerance must be finite and non-negative");
        result.identical = false;
        return result;
    }

    AddDifference(
        result, left.private_heat_accepted != right.private_heat_accepted,
        "private_heat_accepted");
    AddDifference(
        result, left.threat_fail_closed != right.threat_fail_closed,
        "threat_fail_closed");
    AddDifference(
        result,
        left.water_biodiversity_allowed != right.water_biodiversity_allowed,
        "water_biodiversity_allowed");
    AddDifference(
        result,
        left.water_biodiversity_invariant_holds !=
            right.water_biodiversity_invariant_holds,
        "water_biodiversity_invariant_holds");
    AddDifference(
        result, left.authorization_accepted != right.authorization_accepted,
        "authorization_accepted");
    AddDifference(
        result, left.invasive_control_safe != right.invasive_control_safe,
        "invasive_control_safe");
    AddDifference(
        result,
        left.irrigation_robustly_feasible !=
            right.irrigation_robustly_feasible,
        "irrigation_robustly_feasible");
    AddDifference(
        result,
        !std::isfinite(left.maximum_risk_of_harm) ||
            !std::isfinite(right.maximum_risk_of_harm) ||
            !AreEqual(
                left.maximum_risk_of_harm,
                right.maximum_risk_of_harm,
                tolerance),
        "maximum_risk_of_harm");
    AddDifference(
        result,
        !std::isfinite(left.knowledge_factor) ||
            !std::isfinite(right.knowledge_factor) ||
            !AreEqual(left.knowledge_factor, right.knowledge_factor, tolerance),
        "knowledge_factor");
    AddDifference(
        result,
        !std::isfinite(left.eco_impact_value) ||
            !std::isfinite(right.eco_impact_value) ||
            !AreEqual(
                left.eco_impact_value,
                right.eco_impact_value,
                tolerance),
        "eco_impact_value");
    AddDifference(
        result, left.foundation_safe != right.foundation_safe,
        "foundation_safe");

    result.identical = result.differences.empty();
    return result;
}

std::string ExplainFoundationReportDiff(const FoundationReportDiff& diff) {
    if (diff.identical && diff.differences.empty()) {
        return "FoundationReport values are identical within the supplied tolerance.";
    }

    std::ostringstream output;
    output << "FoundationReport differences:";
    for (const std::string& difference : diff.differences) {
        output << '\n' << "- " << difference;
    }
    return output.str();
}

bool FoundationReportValidatorSelfTest() {
    const FoundationReport safe{
        true, false, true, true, true, true, true,
        0.25, 0.90, 0.85, true};

    if (!IsFoundationReportValid(safe)) {
        return false;
    }

    FoundationReport unsafe = safe;
    unsafe.maximum_risk_of_harm = 0.31;
    unsafe.foundation_safe = false;
    if (!IsFoundationReportValid(unsafe)) {
        return false;
    }

    FoundationReport inconsistent = safe;
    inconsistent.foundation_safe = false;
    if (IsFoundationReportValid(inconsistent)) {
        return false;
    }

    FoundationReport nonfinite = safe;
    nonfinite.knowledge_factor = std::numeric_limits<double>::quiet_NaN();
    if (IsFoundationReportValid(nonfinite)) {
        return false;
    }

    FoundationReport invalid_risk = safe;
    invalid_risk.maximum_risk_of_harm = -0.01;
    invalid_risk.foundation_safe = false;
    if (IsFoundationReportValid(invalid_risk)) {
        return false;
    }

    FoundationReport nearby = safe;
    nearby.knowledge_factor = safe.knowledge_factor + 0.0005;
    if (!CompareFoundationReports(safe, nearby, 0.001).identical) {
        return false;
    }
    if (CompareFoundationReports(safe, nearby, 0.0001).identical) {
        return false;
    }

    FoundationReport nan_comparison = safe;
    nan_comparison.eco_impact_value =
        std::numeric_limits<double>::quiet_NaN();
    if (CompareFoundationReports(safe, nan_comparison, 0.01).identical) {
        return false;
    }

    if (CompareFoundationReports(
            safe, safe, -0.01).identical) {
        return false;
    }

    return true;
}
