// File: fog_heuristic_drainage_service.cpp
// Destination: Prometheus-Praxis/fog-heuristic-drainage/fog_heuristic_drainage_service.cpp
// License: MIT OR Apache-2.0

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <sstream>
#include <iostream>

/**
 * FogHeuristicDrainage2026v1 microservice
 *
 * Responsibilities:
 *  - Read live weather telemetry (stdin or simple sensor feed).
 *  - Compute FogHeuristicDrainage2026v1 risk score.
 *  - Emit risk update messages to a Rust crate via a Unix domain socket.
 *
 * Message format sent over Unix socket (newline-delimited JSON):
 *
 * {
 *   "schema": "FogHeuristicDrainage2026v1",
 *   "station_id": "AZ-PHX-001",
 *   "timestamp_utc": "2026-07-23T15:56:00Z",
 *   "visibility_m": 120.0,
 *   "relative_humidity": 0.94,
 *   "rain_rate_mm_h": 3.2,
 *   "drainage_risk_score": 0.73
 * }
 *
 * The Rust crate can deserialize this and update the hydrology plane.
 */

struct TelemetrySample {
    std::string station_id;
    std::string timestamp_utc;
    double visibility_m;
    double relative_humidity;
    double rain_rate_mm_h;
    double wind_speed_m_s;
};

static const char* SOCKET_PATH = "/tmp/fog_drainage_mt6883.sock";

/**
 * Simple FogHeuristicDrainage2026v1 kernel:
 *
 * Inputs:
 *  - visibility_m: lower visibility → higher risk
 *  - relative_humidity: higher humidity → higher risk
 *  - rain_rate_mm_h: higher rain rate → higher risk
 *  - wind_speed_m_s: low wind → higher persistence, higher risk
 *
 * Output:
 *  - drainage_risk_score in [0.0, 1.0]
 */
double compute_fog_drainage_risk(const TelemetrySample& s) {
    const double VIS_SAFE_M   = 1000.0;
    const double RH_SAFE      = 0.80;
    const double RAIN_SAFE    = 1.0;
    const double WIND_SAFE_MS = 3.0;

    // Visibility: inverse scaling
    double vis_norm = 0.0;
    if (s.visibility_m < VIS_SAFE_M) {
        vis_norm = (VIS_SAFE_M - s.visibility_m) / VIS_SAFE_M;
        if (vis_norm < 0.0) vis_norm = 0.0;
        if (vis_norm > 1.0) vis_norm = 1.0;
    }

    // Humidity: above safe threshold
    double rh_norm = 0.0;
    if (s.relative_humidity > RH_SAFE) {
        rh_norm = (s.relative_humidity - RH_SAFE) / (1.0 - RH_SAFE);
        if (rh_norm < 0.0) rh_norm = 0.0;
        if (rh_norm > 1.0) rh_norm = 1.0;
    }

    // Rain rate: above safe threshold
    double rain_norm = 0.0;
    if (s.rain_rate_mm_h > RAIN_SAFE) {
        rain_norm = (s.rain_rate_mm_h - RAIN_SAFE) / (20.0 - RAIN_SAFE);
        if (rain_norm < 0.0) rain_norm = 0.0;
        if (rain_norm > 1.0) rain_norm = 1.0;
    }

    // Wind speed: lower than safe threshold increases risk
    double wind_norm = 0.0;
    if (s.wind_speed_m_s < WIND_SAFE_MS) {
        wind_norm = (WIND_SAFE_MS - s.wind_speed_m_s) / WIND_SAFE_MS;
        if (wind_norm < 0.0) wind_norm = 0.0;
        if (wind_norm > 1.0) wind_norm = 1.0;
    }

    const double w_vis  = 0.35;
    const double w_rh   = 0.25;
    const double w_rain = 0.25;
    const double w_wind = 0.15;

    double combined = w_vis * vis_norm
                    + w_rh * rh_norm
                    + w_rain * rain_norm
                    + w_wind * wind_norm;

    if (combined < 0.0) combined = 0.0;
    if (combined > 1.0) combined = 1.0;

    return combined;
}

/**
 * Parse a single telemetry line from stdin.
 *
 * Expected CSV-like line:
 *   station_id,timestamp_utc,visibility_m,relative_humidity,rain_rate_mm_h,wind_speed_m_s
 */
bool parse_telemetry_line(const std::string& line, TelemetrySample& out) {
    if (line.empty()) {
        return false;
    }

    std::stringstream ss(line);
    std::string field;
    std::string fields[6];
    int idx = 0;

    while (std::getline(ss, field, ',')) {
        if (idx < 6) {
            fields[idx++] = field;
        }
    }

    if (idx != 6) {
        return false;
    }

    out.station_id = fields[0];
    out.timestamp_utc = fields[1];
    out.visibility_m = std::atof(fields[2].c_str());
    out.relative_humidity = std::atof(fields[3].c_str());
    out.rain_rate_mm_h = std::atof(fields[4].c_str());
    out.wind_speed_m_s = std::atof(fields[5].c_str());

    return true;
}

/**
 * Build JSON message for Unix socket.
 */
std::string build_risk_update_json(const TelemetrySample& s, double risk_score) {
    std::ostringstream oss;
    oss.setf(std::ios::fixed);
    oss.precision(3);

    oss << "{"
        << "\"schema\":\"FogHeuristicDrainage2026v1\","
        << "\"station_id\":\"" << s.station_id << "\","
        << "\"timestamp_utc\":\"" << s.timestamp_utc << "\","
        << "\"visibility_m\":" << s.visibility_m << ","
        << "\"relative_humidity\":" << s.relative_humidity << ","
        << "\"rain_rate_mm_h\":" << s.rain_rate_mm_h << ","
        << "\"wind_speed_m_s\":" << s.wind_speed_m_s << ","
        << "\"drainage_risk_score\":" << risk_score
        << "}\n";

    return oss.str();
}

/**
 * Send message over Unix domain socket.
 */
bool send_unix_socket_message(const std::string& socket_path, const std::string& message) {
    int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        std::perror("socket");
        return false;
    }

    struct sockaddr_un addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    std::snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", socket_path.c_str());

    if (::connect(fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::perror("connect");
        ::close(fd);
        return false;
    }

    const char* data = message.c_str();
    std::size_t total = message.size();
    std::size_t sent = 0;

    while (sent < total) {
        ssize_t n = ::write(fd, data + sent, total - sent);
        if (n <= 0) {
            std::perror("write");
            ::close(fd);
            return false;
        }
        sent += static_cast<std::size_t>(n);
    }

    ::close(fd);
    return true;
}

/**
 * Main service loop:
 *  - Read telemetry lines from stdin.
 *  - Compute risk.
 *  - Send risk updates over Unix socket.
 */
int main(int argc, char** argv) {
    std::string socket_path = SOCKET_PATH;
    if (argc > 1) {
        socket_path = argv[1];
    }

    std::cerr << "FogHeuristicDrainage2026v1 service started. Unix socket path: "
              << socket_path << std::endl;

    std::string line;
    while (std::getline(std::cin, line)) {
        TelemetrySample sample;
        if (!parse_telemetry_line(line, sample)) {
            std::cerr << "Skipping invalid telemetry line: " << line << std::endl;
            continue;
        }

        double risk_score = compute_fog_drainage_risk(sample);
        std::string msg = build_risk_update_json(sample, risk_score);

        if (!send_unix_socket_message(socket_path, msg)) {
            std::cerr << "Failed to send risk update for station "
                      << sample.station_id << " to socket " << socket_path << std::endl;
        } else {
            std::cerr << "Sent risk update: station=" << sample.station_id
                      << " score=" << risk_score << std::endl;
        }
    }

    std::cerr << "FogHeuristicDrainage2026v1 service exiting." << std::endl;
    return 0;
}
