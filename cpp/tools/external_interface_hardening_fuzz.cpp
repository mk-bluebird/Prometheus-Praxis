// File: cpp/tools/external_interface_hardening_fuzz.cpp
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string_view>

namespace eco_restoration {

constexpr std::size_t kMaximumFrameBytes = 1024;

std::uint16_t modbus_crc(const std::uint8_t* bytes, std::size_t size) {
    std::uint16_t crc = 0xFFFF;
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= bytes[i];
        for (int bit = 0; bit < 8; ++bit) crc = (crc & 1U) ? (crc >> 1U) ^ 0xA001U : crc >> 1U;
    }
    return crc;
}

bool valid_modbus_input_response(const std::uint8_t* frame, std::size_t size,
                                 std::uint8_t expected_address) {
    if (!frame || size < 7 || size > kMaximumFrameBytes || frame[0] != expected_address ||
        frame[1] != 0x04 || frame[2] == 0 || size != static_cast<std::size_t>(frame[2]) + 5)
        return false;
    const std::uint16_t received = frame[size - 2] | (static_cast<std::uint16_t>(frame[size - 1]) << 8U);
    return modbus_crc(frame, size - 2) == received;
}

bool valid_http_identifier(std::string_view value, std::size_t maximum) {
    return !value.empty() && value.size() <= maximum &&
           std::all_of(value.begin(), value.end(), [](unsigned char c) {
               return std::isalnum(c) || c == '_' || c == '-';
           });
}

bool valid_telemetry_payload(std::string_view payload) {
    if (payload.empty() || payload.size() > kMaximumFrameBytes) return false;
    return std::all_of(payload.begin(), payload.end(), [](unsigned char c) {
        return std::isprint(c) || c == '\n' || c == '\r' || c == '\t';
    });
}

bool valid_quic_frame(std::string_view device_id, std::string_view payload) {
    return valid_http_identifier(device_id, 64) && valid_telemetry_payload(payload);
}

}  // namespace eco_restoration

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    if (!data) return 0;
    eco_restoration::valid_modbus_input_response(data, size, 1);
    eco_restoration::valid_telemetry_payload(
        std::string_view(reinterpret_cast<const char*>(data), std::min(size, eco_restoration::kMaximumFrameBytes)));
    if (size > 1) eco_restoration::valid_quic_frame(
        std::string_view(reinterpret_cast<const char*>(data), std::min<std::size_t>(size, 64)),
        std::string_view(reinterpret_cast<const char*>(data + 1), size - 1));
    return 0;
}
