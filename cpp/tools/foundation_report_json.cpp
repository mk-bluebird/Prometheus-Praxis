// File: cpp/tools/foundation_report_json.cpp
#include "foundation_report_json.hpp"

#include <cmath>
#include <iomanip>
#include <locale>
#include <stdexcept>
#include <string>
#include <utility>

namespace prometheus_praxis::foundation::json {
namespace {

bool IsFiniteReport(const FoundationReport& report) {
    return std::isfinite(report.maximum_risk_of_harm) &&
           std::isfinite(report.knowledge_factor) &&
           std::isfinite(report.eco_impact_value);
}

void ApplyLocale(
    std::ostringstream& out,
    const FoundationJsonOptions& options) {
    if (options.classic_locale) {
        out.imbue(std::locale::classic());
    }
}

void ApplyPrecision(
    std::ostringstream& out,
    const FoundationJsonOptions& options) {
    if (options.six_digit_precision) {
        out << std::fixed << std::setprecision(6);
    }
}

void WriteFieldPrefix(
    std::ostringstream& out,
    std::string_view key,
    bool& first) {
    if (!first) {
        out << ',';
    }
    first = false;
    WriteJsonString(out, key);
    out << ':';
}

void WriteReportObject(
    std::ostringstream& out,
    const FoundationReport& report,
    const FoundationJsonOptions& options) {
    bool first = true;
    out << '{';

    WriteFieldPrefix(out, "private_heat_accepted", first);
    WriteJsonBool(out, report.private_heat_accepted);
    WriteFieldPrefix(out, "threat_fail_closed", first);
    WriteJsonBool(out, report.threat_fail_closed);
    WriteFieldPrefix(out, "water_biodiversity_allowed", first);
    WriteJsonBool(out, report.water_biodiversity_allowed);
    WriteFieldPrefix(out, "water_biodiversity_invariant_holds", first);
    WriteJsonBool(out, report.water_biodiversity_invariant_holds);
    WriteFieldPrefix(out, "authorization_accepted", first);
    WriteJsonBool(out, report.authorization_accepted);
    WriteFieldPrefix(out, "invasive_control_safe", first);
    WriteJsonBool(out, report.invasive_control_safe);
    WriteFieldPrefix(out, "irrigation_robustly_feasible", first);
    WriteJsonBool(out, report.irrigation_robustly_feasible);
    WriteFieldPrefix(out, "maximum_risk_of_harm", first);
    ApplyPrecision(out, options);
    WriteJsonDouble(out, report.maximum_risk_of_harm);
    WriteFieldPrefix(out, "knowledge_factor", first);
    WriteJsonDouble(out, report.knowledge_factor);
    WriteFieldPrefix(out, "eco_impact_value", first);
    WriteJsonDouble(out, report.eco_impact_value);
    WriteFieldPrefix(out, "foundation_safe", first);
    WriteJsonBool(out, report.foundation_safe);

    out << '}';
}

std::string SerializeDocument(
    const FoundationReport& report,
    const FoundationJsonOptions& options,
    bool append_newline) {
    if (!IsFiniteReport(report)) {
        throw std::invalid_argument(
            "FoundationReport JSON serialization requires finite numeric fields");
    }

    std::ostringstream out;
    ApplyLocale(out, options);
    WriteReportObject(out, report, options);

    if (append_newline) {
        out << '\n';
    }

    return out.str();
}

}  // namespace

void WriteJsonString(std::ostringstream& out, std::string_view value) {
    out << '"';

    for (const unsigned char byte : value) {
        switch (byte) {
            case '"':
                out << "\\\"";
                break;
            case '\\':
                out << "\\\\";
                break;
            case '\b':
                out << "\\b";
                break;
            case '\f':
                out << "\\f";
                break;
            case '\n':
                out << "\\n";
                break;
            case '\r':
                out << "\\r";
                break;
            case '\t':
                out << "\\t";
                break;
            default:
                if (byte < 0x20U) {
                    const std::ios_base::fmtflags flags = out.flags();
                    const char fill = out.fill();
                    out << "\\u00" << std::uppercase << std::hex
                        << std::setw(2) << std::setfill('0')
                        << static_cast<unsigned int>(byte);
                    out.flags(flags);
                    out.fill(fill);
                } else {
                    out << static_cast<char>(byte);
                }
                break;
        }
    }

    out << '"';
}

void WriteJsonDouble(std::ostringstream& out, double value) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument("JSON numbers must be finite");
    }

    out << value;
}

void WriteJsonBool(std::ostringstream& out, bool value) {
    out << (value ? "true" : "false");
}

std::string SerializeFoundationReportJson(
    const FoundationReport& report,
    const FoundationJsonOptions& options) {
    return SerializeDocument(report, options, options.final_newline);
}

std::string SerializeFoundationReportJsonLine(
    const FoundationReport& report,
    const FoundationJsonOptions& options) {
    return SerializeDocument(report, options, true);
}

std::string SerializeFoundationReportEnvelope(
    const FoundationReport& report,
    std::string_view abi_version,
    std::string_view diagnostic_schema_version,
    bool policy_valid) {
    FoundationJsonOptions options;
    options.final_newline = false;

    if (!IsFiniteReport(report)) {
        throw std::invalid_argument(
            "FoundationReport envelope serialization requires finite numeric fields");
    }

    std::ostringstream out;
    ApplyLocale(out, options);

    out << '{';
    WriteJsonString(out, "abi_version");
    out << ':';
    WriteJsonString(out, abi_version);
    out << ',';
    WriteJsonString(out, "diagnostic_schema_version");
    out << ':';
    WriteJsonString(out, diagnostic_schema_version);
    out << ',';
    WriteJsonString(out, "policy_valid");
    out << ':';
    WriteJsonBool(out, policy_valid);
    out << ',';
    WriteJsonString(out, "report");
    out << ':';
    WriteReportObject(out, report, options);
    out << '}';

    return out.str();
}

bool FoundationReportJsonSerializerSelfTest() {
    const FoundationReport report{
        true,
        true,
        true,
        true,
        true,
        true,
        true,
        0.2,
        0.85,
        0.9,
        true};

    const std::string expected =
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
        "\"foundation_safe\":true}\n";

    if (SerializeFoundationReportJson(report) != expected) {
        return false;
    }

    FoundationJsonOptions no_newline;
    no_newline.final_newline = false;
    const std::string without_newline =
        SerializeFoundationReportJson(report, no_newline);
    if (without_newline + '\n' != expected ||
        SerializeFoundationReportJsonLine(report, no_newline) != expected) {
        return false;
    }

    const std::string envelope = SerializeFoundationReportEnvelope(
        report,
        "foundation_abi_v1",
        "foundation_diagnostic_v1",
        true);
    if (envelope.find("\"abi_version\":\"foundation_abi_v1\"") ==
            std::string::npos ||
        envelope.find("\"diagnostic_schema_version\":"
                      "\"foundation_diagnostic_v1\"") == std::string::npos ||
        envelope.find("\"policy_valid\":true") == std::string::npos ||
        envelope.find("\"report\":{") == std::string::npos ||
        !envelope.empty() && envelope.back() == '\n') {
        return false;
    }

    std::ostringstream escaped;
    escaped.imbue(std::locale::classic());
    WriteJsonString(escaped, "\"\\\n\r\t\b\f\x01");
    if (escaped.str() != "\"\\\"\\\\\\n\\r\\t\\b\\f\\u0001\"") {
        return false;
    }

    FoundationReport invalid = report;
    invalid.maximum_risk_of_harm = std::numeric_limits<double>::quiet_NaN();
    try {
        static_cast<void>(SerializeFoundationReportJson(invalid));
        return false;
    } catch (const std::invalid_argument&) {
    }

    return true;
}

}  // namespace prometheus_praxis::foundation::json
