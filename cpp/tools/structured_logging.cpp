// File: cpp/tools/structured_logging.cpp
#include "structured_logging.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::size_t kMaximumStructuredLogLineWidth = 119U;

bool IsLowerSnakeCase(const std::string_view value) noexcept {
    if (value.empty() || value.front() == '_' || value.back() == '_') {
        return false;
    }

    bool previous_underscore = false;
    for (const char character : value) {
        const bool lowercase = character >= 'a' && character <= 'z';
        const bool digit = character >= '0' && character <= '9';

        if (character == '_') {
            if (previous_underscore) {
                return false;
            }
            previous_underscore = true;
            continue;
        }

        if (!lowercase && !digit) {
            return false;
        }
        previous_underscore = false;
    }

    return true;
}

bool IsSingleLine(const std::string_view value) noexcept {
    return value.find('\n') == std::string_view::npos &&
           value.find('\r') == std::string_view::npos;
}

bool HasUniqueFieldKeys(const std::vector<StructuredLogField>& fields) {
    for (std::size_t left = 0U; left < fields.size(); ++left) {
        for (std::size_t right = left + 1U; right < fields.size(); ++right) {
            if (fields[left].key == fields[right].key) {
                return false;
            }
        }
    }
    return true;
}

bool IsValidSeverity(const StructuredLogSeverity severity) noexcept {
    return severity == StructuredLogSeverity::Debug ||
           severity == StructuredLogSeverity::Info ||
           severity == StructuredLogSeverity::Warning ||
           severity == StructuredLogSeverity::Error;
}

std::string SeverityName(const StructuredLogSeverity severity) {
    switch (severity) {
        case StructuredLogSeverity::Debug:
            return "debug";
        case StructuredLogSeverity::Info:
            return "info";
        case StructuredLogSeverity::Warning:
            return "warning";
        case StructuredLogSeverity::Error:
            return "error";
    }
    return "invalid";
}

std::int64_t UnixMilliseconds(
    const std::chrono::system_clock::time_point timestamp) noexcept {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               timestamp.time_since_epoch())
        .count();
}

bool IsLineValid(const StructuredLogLine& line) {
    if (!IsValidSeverity(line.severity) ||
        !IsLowerSnakeCase(line.event) ||
        !HasUniqueFieldKeys(line.fields)) {
        return false;
    }

    for (const StructuredLogField& field : line.fields) {
        if (!IsLowerSnakeCase(field.key) || !IsSingleLine(field.value)) {
            return false;
        }
    }

    return true;
}

std::string BuildStructuredLogLine(const StructuredLogLine& line) {
    std::ostringstream output;
    output << "timestamp_ms=" << UnixMilliseconds(line.timestamp)
           << " severity=" << SeverityName(line.severity)
           << " event=" << line.event;

    for (const StructuredLogField& field : line.fields) {
        output << ' ' << field.key << '='
               << EscapeStructuredLogValue(field.value);
    }

    return output.str();
}

}  // namespace

std::string EscapeStructuredLogValue(const std::string_view value) {
    std::string output;
    output.reserve(value.size());

    for (const char character : value) {
        switch (character) {
            case '\\':
                output += "\\\\";
                break;
            case '"':
                output += "\\\"";
                break;
            case '=':
                output += "\\=";
                break;
            case ' ':
                output += "\\s";
                break;
            case '\t':
                output += "\\t";
                break;
            default:
                output.push_back(character);
                break;
        }
    }

    return output;
}

std::string FormatStructuredLogLine(const StructuredLogLine& line) {
    if (!IsLineValid(line)) {
        return {};
    }

    const std::string formatted = BuildStructuredLogLine(line);
    if (formatted.size() > kMaximumStructuredLogLineWidth) {
        return {};
    }

    return formatted;
}

std::string ExplainStructuredLogLine(const StructuredLogLine& line) {
    if (!IsLineValid(line)) {
        return "structured_log_line=invalid";
    }

    const std::string formatted = BuildStructuredLogLine(line);
    if (formatted.size() > kMaximumStructuredLogLineWidth) {
        return "structured_log_line=invalid; reason=line_width_exceeds_119";
    }

    return std::string("structured_log_line=valid; ") + formatted;
}

bool StructuredLoggingSelfTest() {
    const StructuredLogLine valid{
        StructuredLogSeverity::Info,
        std::chrono::system_clock::time_point{
            std::chrono::milliseconds{1700000000123LL}},
        "foundation_self_check",
        std::vector<StructuredLogField>{
            StructuredLogField{"status", "safe"},
            StructuredLogField{"detail", "risk = 0.20\tverified"}}};

    const std::string expected =
        "timestamp_ms=1700000000123 severity=info event=foundation_self_check "
        "status=safe detail=risk\\s\\=\\s0.20\\tverified";

    if (FormatStructuredLogLine(valid) != expected ||
        ExplainStructuredLogLine(valid) !=
            std::string("structured_log_line=valid; ") + expected) {
        return false;
    }

    if (EscapeStructuredLogValue("a\\b\"c=d e\tf") !=
        "a\\\\b\\\"c\\=d\\se\\tf") {
        return false;
    }

    StructuredLogLine duplicate = valid;
    duplicate.fields.push_back(StructuredLogField{"status", "duplicate"});
    if (!FormatStructuredLogLine(duplicate).empty()) {
        return false;
    }

    StructuredLogLine invalid_key = valid;
    invalid_key.event = "Invalid_Event";
    if (!FormatStructuredLogLine(invalid_key).empty()) {
        return false;
    }

    StructuredLogLine multiline = valid;
    multiline.fields[0].value = "unsafe\nvalue";
    if (!FormatStructuredLogLine(multiline).empty()) {
        return false;
    }

    StructuredLogLine oversized = valid;
    oversized.fields.push_back(
        StructuredLogField{
            "detail_two",
            std::string(100U, 'x')});
    if (!FormatStructuredLogLine(oversized).empty()) {
        return false;
    }

    return true;
}
