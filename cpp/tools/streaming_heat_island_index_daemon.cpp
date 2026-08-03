// File: cpp/tools/streaming_heat_island_index_daemon.cpp
#include <iostream>
#include <vector>
#include <string>
#include <cmath>

// This daemon-style component sketches streaming computation of a Heat-Stress Index (HSI)
// per hex. In a real deployment it would subscribe to Phoenix IoT feeds (temperature,
// humidity, solar radiation, NDVI, etc.), but here we simulate periodic updates and
// emit SQL suitable for writing into hex_heat_stress_profile.

namespace eco {

struct IoTReading {
    std::string hex_id;
    double air_temp_C;     // °C
    double surface_temp_C; // °C
    double humidity;       // 0..1
    double ndvi;           // -1..1 vegetation index
};

struct HeatStressProfile {
    std::string hex_id;
    double HSI; // Heat-Stress Index
};

// Compute Heat-Stress Index from IoT readings.
// Example formula: combine air and surface temperature, humidity penalty, and NDVI relief.
double compute_HSI(const IoTReading& r) {
    double T_air = r.air_temp_C;
    double T_surf = r.surface_temp_C;
    double hum = std::max(0.0, std::min(1.0, r.humidity));
    double veg = r.ndvi;

    // Base heat index (simple average of temps).
    double T_mean = 0.5 * (T_air + T_surf);

    // Humidity amplifies perceived heat.
    double humidity_factor = 1.0 + 0.5 * hum;

    // Vegetation reduces heat stress (higher NDVI -> more relief).
    double veg_relief = 1.0 - 0.3 * std::max(-1.0, std::min(1.0, veg));

    double HSI_raw = T_mean * humidity_factor * veg_relief;

    // Normalize to a corridor-friendly scale, e.g., 0..1 relative to 50°C worst-case.
    double HSI = HSI_raw / 50.0;
    if (HSI < 0.0) HSI = 0.0;
    if (HSI > 1.0) HSI = 1.0;
    return HSI;
}

// Convert IoT readings to HeatStressProfile.
HeatStressProfile process_reading(const IoTReading& r) {
    HeatStressProfile p{};
    p.hex_id = r.hex_id;
    p.HSI = compute_HSI(r);
    return p;
}

// Emit SQL update for hex_heat_stress_profile.
void emit_heat_stress_sql(const HeatStressProfile& p, const std::string& ts_iso) {
    std::cout << "INSERT INTO hex_heat_stress_profile "
              << "(hex_id, ts, hsi) VALUES ('"
              << p.hex_id << "', '"
              << ts_iso << "', "
              << p.HSI << ");\n";
}

} // namespace eco

int main() {
    using namespace eco;

    // Simulated streaming loop: in practice, this would run as a daemon reading IoT messages.
    std::vector<IoTReading> readings = {
        {"hex_HSI_1", 42.0, 48.0, 0.35, 0.3},
        {"hex_HSI_2", 39.0, 45.0, 0.50, 0.1},
        {"hex_HSI_3", 37.0, 43.0, 0.40, 0.6}
    };

    std::string ts_iso = "2026-08-03T14:00:00Z";

    for (const auto& r : readings) {
        HeatStressProfile p = process_reading(r);
        std::cout << "Streaming HSI for " << p.hex_id << ":\n";
        std::cout << "  HSI = " << p.HSI << "\n";
        emit_heat_stress_sql(p, ts_iso);
        std::cout << "\n";
    }

    return 0;
}
