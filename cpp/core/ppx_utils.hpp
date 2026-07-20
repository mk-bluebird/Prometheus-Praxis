#ifndef INCLUDED_PROMETHEUS_PRAXIS_UTILS_HPP
#define INCLUDEDPROMETHEUS_PRAXIS_UTILS_HPP

// Header-only utility library for Prometheus-Praxis.
// Patterns are inspired by common C++ utility and header-only libraries. [web:64][web:69]

#include <string>
#include <vector>
#include <chrono>
#include <sstream>
#include <fstream>
#include <optional>
#include <map>
#include <iomanip>
#include <iostream>

namespace ppx {

// -----------------------------------------------------------------------------
// Result<T> – lightweight success/error carrier
// -----------------------------------------------------------------------------

template <typename T>
class Result {
public:
    static Result<T> ok(T value) {
        return Result<T>(std::move(value), std::nullopt);
    }

    static Result<T> err(std::string message) {
        return Result<T>(T{}, std::make_optional(std::move(message)));
    }

    bool is_ok() const { return !error_.has_value(); }
    bool is_err() const { return error_.has_value(); }

    const T& value() const { return value_; }
    T& value() { return value_; }

    const std::optional<std::string>& error() const { return error_; }

private:
    T value_{};
    std::optional<std::string> error_;

    Result(T value, std::optional<std::string> error)
        : value_(std::move(value)), error_(std::move(error)) {}
};

// -----------------------------------------------------------------------------
// ScopedTimer – measure wall-clock durations
// -----------------------------------------------------------------------------

class ScopedTimer {
public:
    explicit ScopedTimer(const std::string& label)
        : label_(label), start_(std::chrono::steady_clock::now()) {}

    ~ScopedTimer() {
        auto end = std::chrono::steady_clock::now();
        auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(end - start_).count();
        std::cerr << "[ppx::ScopedTimer] " << label_ << " took " << ms << " ms\n";
    }

private:
    std::string label_;
    std::chrono::steady_clock::time_point start_;
};

// -----------------------------------------------------------------------------
// String helpers
// -----------------------------------------------------------------------------

inline std::string trim(const std::string& s) {
    std::size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) {
        ++start;
    }
    std::size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
        --end;
    }
    return s.substr(start, end - start);
}

inline std::vector<std::string> split(const std::string& s, char delim) {
    std::vector<std::string> parts;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim)) {
        parts.push_back(item);
    }
    return parts;
}

// -----------------------------------------------------------------------------
// Simple key-value telemetry map
// -----------------------------------------------------------------------------

using TelemetryMap = std::map<std::string, std::string>;

inline std::string telemetry_to_json(const TelemetryMap& tm) {
    std::ostringstream oss;
    oss << "{";
    bool first = true;
    for (const auto& kv : tm) {
        if (!first) {
            oss << ",";
        }
        first = false;
        oss << "\"" << kv.first << "\":\"" << kv.second << "\"";
    }
    oss << "}";
    return oss.str();
}

// -----------------------------------------------------------------------------
// File I/O helpers
// -----------------------------------------------------------------------------

inline Result<std::string> read_file(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) {
        return Result<std::string>::err("Failed to open file: " + path);
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return Result<std::string>::ok(buffer.str());
}

inline Result<void> write_file(const std::string& path, const std::string& data) {
    std::ofstream out(path);
    if (!out.is_open()) {
        return Result<void>::err("Failed to open file for writing: " + path);
    }
    out << data;
    return Result<void>::ok({});
}

// -----------------------------------------------------------------------------
// Timestamp helper
// -----------------------------------------------------------------------------

inline std::string utc_timestamp_iso8601() {
    auto now = std::chrono::system_clock::now();
    auto tt  = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32) || defined(_WIN64)
    gmtime_s(&tm, &tt);
#else
    gmtime_r(&tt, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return std::string(buf);
}

} // namespace ppx

#endif // INCLUDEDPROMETHEUS_PRAXIS_UTILS_HPP
