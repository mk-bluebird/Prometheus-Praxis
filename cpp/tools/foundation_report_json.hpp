// File: cpp/tools/foundation_report_json.hpp
#pragma once

#include "foundation_report.hpp"

#include <sstream>
#include <string>
#include <string_view>

namespace prometheus_praxis::foundation::json {

struct FoundationJsonOptions {
    bool final_newline{true};
    std::string schema_version{"foundation_report_v1"};
    bool classic_locale{true};
    bool six_digit_precision{true};
};

std::string SerializeFoundationReportJson(
    const FoundationReport& report,
    const FoundationJsonOptions& options = {});

std::string SerializeFoundationReportJsonLine(
    const FoundationReport& report,
    const FoundationJsonOptions& options = {});

std::string SerializeFoundationReportEnvelope(
    const FoundationReport& report,
    std::string_view abi_version,
    std::string_view diagnostic_schema_version,
    bool policy_valid);

void WriteJsonString(std::ostringstream& out, std::string_view value);
void WriteJsonDouble(std::ostringstream& out, double value);
void WriteJsonBool(std::ostringstream& out, bool value);

bool FoundationReportJsonSerializerSelfTest();

}  // namespace prometheus_praxis::foundation::json
