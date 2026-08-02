// File: cpp/tools/water_quality_edge_lorawan.cpp
#include <iostream>
#include <string>
#include <cmath>

// Edge computing wiring pattern for water quality:
// - Device: Arduino Portenta with analogue sensors (e.g., turbidity, conductivity, pH).
// - Runs a stripped-down water_quality_index calculation locally.
// - Sends a binary "safe/unsafe" flag over LoRaWAN to a central dashboard.
// - Full sensor data is stored locally and only uploaded when explicitly queried.
//
// This file encodes the core logic without any networking libraries, focusing on:
//  - water_quality_index computation,
//  - safe/unsafe decision,
//  - compact payload representation suitable for LoRaWAN.

struct WaterQualityReading {
    double turbidity_NTU;
    double conductivity_uS_cm;
    double pH;
    double temperature_C;
};

double compute_water_quality_index(const WaterQualityReading& r) {
    // Simple normalized subindices; in reality, thresholds are Phoenix-specific.
    double turbidity_score = 1.0 - std::min(r.turbidity_NTU / 100.0, 1.0);
    double conductivity_score = 1.0 - std::min(r.conductivity_uS_cm / 2000.0, 1.0);

    double pH_score = 0.0;
    if (r.pH >= 6.5 && r.pH <= 8.5) {
        pH_score = 1.0;
    } else {
        double diff = std::fabs(r.pH - 7.5);
        pH_score = std::max(0.0, 1.0 - diff / 2.0);
    }

    double temp_score = 0.0;
    if (r.temperature_C >= 10.0 && r.temperature_C <= 30.0) {
        temp_score = 1.0;
    } else {
        double diff = std::fabs(r.temperature_C - 20.0);
        temp_score = std::max(0.0, 1.0 - diff / 20.0);
    }

    double wqi = 0.25 * turbidity_score
               + 0.25 * conductivity_score
               + 0.25 * pH_score
               + 0.25 * temp_score;
    return wqi;
}

bool is_water_safe(double wqi, double threshold) {
    return wqi >= threshold;
}

// Encode safe/unsafe flag into a single byte payload for LoRaWAN.
std::uint8_t encode_lorawan_flag(bool safe) {
    return safe ? 0x01 : 0x00;
}

// Local storage stub; in real code, this writes to SD card or flash.
void store_local_reading(const WaterQualityReading& r, double wqi, bool safe) {
    std::cout << "Local store: turbidity=" << r.turbidity_NTU
              << " conductivity=" << r.conductivity_uS_cm
              << " pH=" << r.pH
              << " temp=" << r.temperature_C
              << " WQI=" << wqi
              << " safe=" << (safe ? "yes" : "no") << "\n";
}

// LoRaWAN send stub; in real wiring, map payload to radio packet.
void send_lorawan_flag(std::uint8_t payload) {
    std::cout << "LoRaWAN send: payload=0x"
              << std::hex << static_cast<int>(payload)
              << std::dec << "\n";
}

int main() {
    // Example sensor reading on the edge device.
    WaterQualityReading r;
    r.turbidity_NTU = 12.0;
    r.conductivity_uS_cm = 800.0;
    r.pH = 7.4;
    r.temperature_C = 24.0;

    double wqi = compute_water_quality_index(r);
    bool safe = is_water_safe(wqi, 0.7);

    store_local_reading(r, wqi, safe);

    std::uint8_t flag_payload = encode_lorawan_flag(safe);
    send_lorawan_flag(flag_payload);

    // Full data remains local; when a query arrives (e.g., via downlink or physical retrieval),
    // a separate process can upload the stored readings to the central dashboard.

    return 0;
}
