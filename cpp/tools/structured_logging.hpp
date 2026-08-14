// File: cpp/tools/structured_logging.hpp
#pragma once

#include <chrono>
#include <string>
#include <string_view>
#include <vector>

enum class StructuredLogSeverity {
    Debug,
    Info,
    Warning,
    Error
};

struct StructuredLogField {
    std::string key;
    std::string value;
};

struct StructuredLogLine {
    StructuredLogSeverity severity{StructuredLogSeverity::Info};
    std::chrono::system_clock::time_point timestamp{};
    std::string event;
    std::vector<StructuredLogField> fields;
};

std::string EscapeStructuredLogValue(std::string_view value);

std::string FormatStructuredLogLine(const StructuredLogLine& line);

std::string ExplainStructuredLogLine(const StructuredLogLine& line);

bool StructuredLoggingSelfTest();
