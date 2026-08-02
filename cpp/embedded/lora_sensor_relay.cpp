// File: cpp/embedded/lora_sensor_relay.cpp
#include <iostream>
#include <string>
#include <cstdint>
#include <cstring>
#include <chrono>
#include <iomanip>

// Embedded LoRa sensor relay:
// - Reads a soil moisture value via Modbus (simulated here).
// - Computes a hex anchor ID using HexAnchorPhoenix.
// - Packs sensor value, hex anchor, and timestamp into a compact payload.
// - Prints payload bytes (in a real device, this would be transmitted over LoRaWAN).
//
// This file avoids external stacks; the Modbus and LoRaWAN interfaces are represented
// by simple functions that can be mapped to hardware drivers on real edge devices.

namespace eco {

class HexAnchorPhoenix {
public:
    HexAnchorPhoenix(double hex_size_km,
                     double lat_origin_deg,
                     double lon_origin_deg)
        : hex_size_km_(hex_size_km),
          lat_origin_deg_(lat_origin_deg),
          lon_origin_deg_(lon_origin_deg) {}

    std::string compute_hex_anchor(double lat_deg, double lon_deg) const {
        double x_km = 0.0;
        double y_km = 0.0;
        project_to_local(lat_deg, lon_deg, x_km, y_km,
                         lat_origin_deg_, lon_origin_deg_);

        double q = (std::sqrt(3.0) / 3.0 * x_km - 1.0 / 3.0 * y_km) / hex_size_km_;
        double r = (2.0 / 3.0 * y_km) / hex_size_km_;

        int qi = static_cast<int>(std::round(q));
        int ri = static_cast<int>(std::round(r));

        return "PHX-H3-" + std::to_string(qi) + "-" + std::to_string(ri);
    }

private:
    double hex_size_km_;
    double lat_origin_deg_;
    double lon_origin_deg_;

    static void project_to_local(double lat_deg, double lon_deg,
                                 double& x_km, double& y_km,
                                 double lat_origin_deg,
                                 double lon_origin_deg) {
        const double R = 6371.0;
        double lat_rad = lat_deg * M_PI / 180.0;
        double lon_rad = lon_deg * M_PI / 180.0;
        double lat0_rad = lat_origin_deg * M_PI / 180.0;
        double lon0_rad = lon_origin_deg * M_PI / 180.0;

        double dx = (lon_rad - lon0_rad) * std::cos(lat0_rad);
        double dy = (lat_rad - lat0_rad);

        x_km = R * dx;
        y_km = R * dy;
    }
};

// Simulated Modbus soil moisture sensor interface.
class ModbusSoilMoistureSensor {
public:
    explicit ModbusSoilMoistureSensor(uint8_t device_id)
        : device_id_(device_id) {}

    // In a real system, this would send a Modbus RTU frame and receive data
    // from a register; here we just return a deterministic value.
    double read_moisture_percent() const {
        // Example: 37.5% soil moisture.
        return 37.5;
    }

private:
    uint8_t device_id_;
};

// Simple LoRaWAN payload builder (uplink), using a fixed binary schema:
// [0]   : device_id (uint8)
// [1-4] : timestamp (uint32, seconds since epoch)
// [5-6] : soil moisture (uint16, value * 10, i.e., 1 decimal place)
// [7-?] : hex anchor ID as ASCII (null-terminated)
struct LoRaPayload {
    uint8_t  device_id;
    uint32_t timestamp_s;
    uint16_t moisture_x10;
    char     hex_anchor[32]; // enough for "PHX-H3-q-r" plus terminator
};

class LoRaSensorRelay {
public:
    LoRaSensorRelay(uint8_t device_id,
                    double lat_sensor_deg,
                    double lon_sensor_deg)
        : device_id_(device_id),
          sensor_(device_id),
          anchor_(1.0, 33.4484, -112.0740),
          lat_sensor_deg_(lat_sensor_deg),
          lon_sensor_deg_(lon_sensor_deg) {}

    LoRaPayload build_payload() const {
        LoRaPayload payload{};
        payload.device_id = device_id_;

        // Timestamp in seconds since epoch.
        auto now = std::chrono::system_clock::now();
        auto epoch = std::chrono::system_clock::time_point{};
        auto diff  = now - epoch;
        uint32_t ts = static_cast<uint32_t>(
            std::chrono::duration_cast<std::chrono::seconds>(diff).count()
        );
        payload.timestamp_s = ts;

        double moisture = sensor_.read_moisture_percent();
        payload.moisture_x10 = static_cast<uint16_t>(std::round(moisture * 10.0));

        std::string hex_id = anchor_.compute_hex_anchor(lat_sensor_deg_, lon_sensor_deg_);
        std::size_t len = hex_id.size();
        if (len >= sizeof(payload.hex_anchor)) {
            len = sizeof(payload.hex_anchor) - 1;
        }
        std::memcpy(payload.hex_anchor, hex_id.c_str(), len);
        payload.hex_anchor[len] = '\0';

        return payload;
    }

    static void print_payload(const LoRaPayload& p) {
        std::cout << "LoRaPayload {\n";
        std::cout << "  device_id: " << static_cast<int>(p.device_id) << "\n";
        std::cout << "  timestamp_s: " << p.timestamp_s << "\n";
        std::cout << "  moisture_percent: " << (p.moisture_x10 / 10.0) << "%\n";
        std::cout << "  hex_anchor: " << p.hex_anchor << "\n";
        std::cout << "  raw_bytes: ";
        const std::size_t total_bytes = 1 + 4 + 2 + std::strlen(p.hex_anchor) + 1;
        const uint8_t* raw = reinterpret_cast<const uint8_t*>(&p.device_id);
        std::cout << std::hex << std::setfill('0');
        for (std::size_t i = 0; i < total_bytes; ++i) {
            std::cout << "0x" << std::setw(2) << static_cast<int>(raw[i]) << " ";
        }
        std::cout << std::dec << "\n";
        std::cout << "}\n";
    }

private:
    uint8_t device_id_;
    ModbusSoilMoistureSensor sensor_;
    HexAnchorPhoenix anchor_;
    double lat_sensor_deg_;
    double lon_sensor_deg_;
};

} // namespace eco

int main() {
    using namespace eco;

    // Example edge device configuration.
    uint8_t device_id = 7;
    double lat_sensor = 33.4502;
    double lon_sensor = -112.0715;

    LoRaSensorRelay relay(device_id, lat_sensor, lon_sensor);
    LoRaPayload payload = relay.build_payload();

    LoRaSensorRelay::print_payload(payload);

    return 0;
}
