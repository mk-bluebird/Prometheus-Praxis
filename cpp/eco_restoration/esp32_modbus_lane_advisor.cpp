// File: cpp/eco_restoration/esp32_modbus_lane_advisor.cpp
#include <Arduino.h>

#include <algorithm>
#include <cmath>

namespace {

constexpr int kRs485RxPin = 16;
constexpr int kRs485TxPin = 17;
constexpr int kRs485DirectionPin = 4;
constexpr int kProceedPin = 25;
constexpr int kDeratePin = 26;
constexpr int kReviewPin = 27;
constexpr std::uint8_t kModbusAddress = 1;

enum class Advisory { Proceed, Derate, Review };

std::uint16_t modbus_crc(const std::uint8_t* data, std::size_t size) {
    std::uint16_t crc = 0xFFFF;
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
    }
    return crc;
}

bool read_input_registers(std::uint16_t* values, std::size_t count) {
    if (count == 0 || count > 8) return false;
    const std::uint8_t request[]{kModbusAddress, 0x04, 0x00, 0x00, 0x00,
                                 static_cast<std::uint8_t>(count), 0x00, 0x00};
    std::uint8_t frame[sizeof(request)];
    std::copy(std::begin(request), std::end(request), frame);
    const std::uint16_t crc = modbus_crc(frame, 6);
    frame[6] = static_cast<std::uint8_t>(crc);
    frame[7] = static_cast<std::uint8_t>(crc >> 8);

    digitalWrite(kRs485DirectionPin, HIGH);
    Serial1.write(frame, sizeof(frame));
    Serial1.flush();
    digitalWrite(kRs485DirectionPin, LOW);

    const std::size_t response_size = 5 + count * 2;
    std::uint8_t response[21]{};
    const unsigned long deadline = millis() + 250;
    std::size_t received = 0;
    while (received < response_size && millis() < deadline) {
        if (Serial1.available()) response[received++] = static_cast<std::uint8_t>(Serial1.read());
    }
    if (received != response_size || response[0] != kModbusAddress ||
        response[1] != 0x04 || response[2] != count * 2) return false;
    const std::uint16_t received_crc = response[response_size - 2] |
                                       (static_cast<std::uint16_t>(response[response_size - 1]) << 8);
    if (modbus_crc(response, response_size - 2) != received_crc) return false;
    for (std::size_t i = 0; i < count; ++i)
        values[i] = (static_cast<std::uint16_t>(response[3 + i * 2]) << 8) | response[4 + i * 2];
    return true;
}

Advisory evaluate(double temperature_c, double turbidity_ntu, double oxygen_mg_l,
                  double water_quality_index) {
    const double heat = std::clamp((temperature_c - 30.0) / 15.0, 0.0, 1.0);
    const double water = std::clamp(0.5 * (1.0 - water_quality_index) +
                                    0.3 * turbidity_ntu / (turbidity_ntu + 10.0) +
                                    0.2 / (oxygen_mg_l + 1.0), 0.0, 1.0);
    const double risk = std::max(heat, water);
    return risk > 0.70 ? Advisory::Review : risk > 0.35 ? Advisory::Derate : Advisory::Proceed;
}

void publish(Advisory advisory) {
    digitalWrite(kProceedPin, advisory == Advisory::Proceed);
    digitalWrite(kDeratePin, advisory == Advisory::Derate);
    digitalWrite(kReviewPin, advisory == Advisory::Review);
}

}  // namespace

void setup() {
    pinMode(kRs485DirectionPin, OUTPUT);
    pinMode(kProceedPin, OUTPUT);
    pinMode(kDeratePin, OUTPUT);
    pinMode(kReviewPin, OUTPUT);
    Serial.begin(115200);
    Serial1.begin(9600, SERIAL_8N1, kRs485RxPin, kRs485TxPin);
    publish(Advisory::Review);
}

void loop() {
    std::uint16_t registers[4]{};
    if (read_input_registers(registers, 4)) {
        publish(evaluate(registers[0] / 100.0, registers[1] / 100.0,
                         registers[2] / 100.0, registers[3] / 1000.0));
    } else {
        publish(Advisory::Review);
    }
    delay(5000);
}
