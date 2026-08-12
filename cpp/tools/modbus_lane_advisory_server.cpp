// File: cpp/tools/modbus_lane_advisory_server.cpp

#include <modbus/modbus.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace eco_restoration {

std::uint16_t advisory_action(
    std::uint16_t power_w,
    std::uint16_t temperature_centi_c,
    std::uint16_t water_quality_milli) {

    const double temperature_c = static_cast<double>(temperature_centi_c) / 100.0;
    const double water_quality = std::clamp(static_cast<double>(water_quality_milli) / 1000.0, 0.0, 1.0);
    const double energy_risk = std::clamp(static_cast<double>(power_w) / 500.0, 0.0, 1.0);
    const double heat_risk = std::clamp((temperature_c - 30.0) / 15.0, 0.0, 1.0);
    const double water_risk = 1.0 - water_quality;
    const double risk = std::max({energy_risk, heat_risk, water_risk});

    return risk > 0.70 ? 2U : risk > 0.35 ? 1U : 0U;
}

}  // namespace eco_restoration

int main() {
    modbus_t* context = modbus_new_tcp(nullptr, 1502);
    modbus_mapping_t* mapping = modbus_mapping_new(0, 8, 16, 0);
    if (context == nullptr || mapping == nullptr) {
        modbus_free(context);
        modbus_mapping_free(mapping);
        return 1;
    }

    const int listener = modbus_tcp_listen(context, 1);
    if (listener < 0) {
        modbus_mapping_free(mapping);
        modbus_free(context);
        return 1;
    }

    std::uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH]{};
    for (;;) {
        if (modbus_tcp_accept(context, &listener) < 0) continue;

        int received = 0;
        while ((received = modbus_receive(context, query)) > 0) {
            const std::uint16_t decision = eco_restoration::advisory_action(
                mapping->tab_registers[0],
                mapping->tab_registers[1],
                mapping->tab_registers[2]);

            mapping->tab_input_registers[0] = decision;
            mapping->tab_input_registers[1] = 1U;
            modbus_reply(context, query, received, mapping);
        }
        modbus_close(context);
    }
}
