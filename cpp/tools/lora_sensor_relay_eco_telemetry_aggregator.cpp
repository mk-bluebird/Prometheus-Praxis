// File: cpp/tools/lora_sensor_relay_eco_telemetry_aggregator.cpp
#include <iostream>
#include <string>
#include <vector>
#include <cstdint>

// This module extends a LoRa sensor relay to attach KER metadata to packets
// before forwarding, so edge nodes can participate directly in eco-governance
// without changing the underlying radio stack. Packets are represented as
// simple byte buffers; KER metadata is encoded in a compact header.

namespace eco {

struct SensorReading {
    std::string hex_id;
    std::string module_id;
    double value;      // e.g. temperature, flow, etc.
    double k;          // knowledge factor
    double e;          // eco-efficiency
    double r;          // risk-of-harm
};

struct LoRaPacket {
    std::vector<std::uint8_t> payload;
};

// Compute KER scalar s = k*e - r (clamped to [0,1] for transmission).
double ker_scalar(double k, double e, double r) {
    if (k < 0.0) k = 0.0;
    if (k > 1.0) k = 1.0;
    if (e < 0.0) e = 0.0;
    if (e > 1.0) e = 1.0;
    if (r < 0.0) r = 0.0;
    if (r > 1.0) r = 1.0;
    double s = k * e - r;
    if (s < 0.0) s = 0.0;
    if (s > 1.0) s = 1.0;
    return s;
}

// Encode a double in a fixed-point 16-bit representation (0..1 -> 0..65535).
std::uint16_t encode_fixed_01(double x) {
    if (x < 0.0) x = 0.0;
    if (x > 1.0) x = 1.0;
    return static_cast<std::uint16_t>(x * 65535.0 + 0.5);
}

// Build a LoRa packet with KER header + sensor value.
LoRaPacket build_lora_packet(const SensorReading& sr) {
    LoRaPacket pkt;

    // KER header: [k_fixed(2B)][e_fixed(2B)][r_fixed(2B)][s_fixed(2B)]
    double s = ker_scalar(sr.k, sr.e, sr.r);
    std::uint16_t k_fx = encode_fixed_01(sr.k);
    std::uint16_t e_fx = encode_fixed_01(sr.e);
    std::uint16_t r_fx = encode_fixed_01(sr.r);
    std::uint16_t s_fx = encode_fixed_01(s);

    auto push_u16 = [&](std::uint16_t v) {
        pkt.payload.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
        pkt.payload.push_back(static_cast<std::uint8_t>(v & 0xFF));
    };

    push_u16(k_fx);
    push_u16(e_fx);
    push_u16(r_fx);
    push_u16(s_fx);

    // Sensor value (float32, IEEE 754 encoded)
    float value_f = static_cast<float>(sr.value);
    std::uint32_t v_bits;
    static_assert(sizeof(float) == 4, "float must be 4 bytes");
    std::memcpy(&v_bits, &value_f, sizeof(float));
    pkt.payload.push_back(static_cast<std::uint8_t>((v_bits >> 24) & 0xFF));
    pkt.payload.push_back(static_cast<std::uint8_t>((v_bits >> 16) & 0xFF));
    pkt.payload.push_back(static_cast<std::uint8_t>((v_bits >> 8) & 0xFF));
    pkt.payload.push_back(static_cast<std::uint8_t>(v_bits & 0xFF));

    return pkt;
}

// Simulated forward to edge node: decode KER header and emit SQL for eco-governance.
void forward_to_edge(const SensorReading& sr, const LoRaPacket& pkt) {
    double s = ker_scalar(sr.k, sr.e, sr.r);

    std::cout << "Forwarding LoRa packet for hex " << sr.hex_id
              << ", module " << sr.module_id << " with KER_s=" << s << "\n";

    std::cout << "INSERT INTO edge_eco_telemetry "
              << "(hex_id, module_id, k, e, r, ker_s, value, ts) VALUES ('"
              << sr.hex_id << "', '"
              << sr.module_id << "', "
              << sr.k << ", "
              << sr.e << ", "
              << sr.r << ", "
              << s << ", "
              << sr.value << ", "
              << "CURRENT_TIMESTAMP);\n";
}

} // namespace eco

int main() {
    using namespace eco;

    SensorReading sr{"hex_LORA_001", "module_SENSOR_01",
                     /*value=*/42.5,
                     /*k=*/0.8, /*e=*/0.9, /*r=*/0.2};

    LoRaPacket pkt = build_lora_packet(sr);
    std::cout << "LoRa packet length (bytes): " << pkt.payload.size() << "\n";

    forward_to_edge(sr, pkt);

    return 0;
}
