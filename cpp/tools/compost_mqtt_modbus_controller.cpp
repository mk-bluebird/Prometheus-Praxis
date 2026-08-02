// File: cpp/tools/compost_mqtt_modbus_controller.cpp
#include <iostream>
#include <string>
#include <vector>
#include <cmath>

// This file sketches the wiring pattern for a distributed sensor network:
//  - Edge nodes: ESP32 + BME280 + soil moisture sensor publish readings via MQTT.
//  - Central collector: MQTT subscriber writes data into InfluxDB.
//  - Python analytics worker monitors moisture time series and triggers
//    a composting-pile aeration motor via Modbus when moisture drops below threshold.
//
// Here we implement the core C++ logic for:
//  - Representing sensor messages.
//  - Evaluating moisture thresholds.
//  - Encoding Modbus coil/holding-register commands to drive the aeration motor.
// Actual MQTT and Modbus I/O are abstracted behind simple interfaces so this code
// is safe and portable; concrete wiring would use libraries such as Eclipse Paho (MQTT)
// and libmodbus or equivalent for Modbus.

struct SensorReading {
    std::string node_id;
    double temperature_C;
    double humidity_rel;
    double soil_moisture_frac; // 0–1
    std::int64_t timestamp_s;
};

class MoistureThresholdLogic {
public:
    MoistureThresholdLogic(double min_moisture_frac,
                           std::int64_t min_interval_s)
        : min_moisture_frac_(min_moisture_frac),
          min_interval_s_(min_interval_s),
          last_trigger_time_s_(0)
    {}

    bool should_trigger(const SensorReading& r) {
        if (r.soil_moisture_frac >= min_moisture_frac_) {
            return false;
        }
        if (last_trigger_time_s_ == 0 ||
            (r.timestamp_s - last_trigger_time_s_) >= min_interval_s_) {
            last_trigger_time_s_ = r.timestamp_s;
            return true;
        }
        return false;
    }

private:
    double min_moisture_frac_;
    std::int64_t min_interval_s_;
    std::int64_t last_trigger_time_s_;
};

struct ModbusCommand {
    int unit_id;
    int function_code;
    int address;
    int value;
};

// Minimal Modbus builder for a coil write (on/off motor) and a holding register write (speed).
ModbusCommand build_aeration_on_command(int unit_id, int coil_address) {
    ModbusCommand cmd;
    cmd.unit_id = unit_id;
    cmd.function_code = 5; // Write Single Coil
    cmd.address = coil_address;
    cmd.value = 1; // ON
    return cmd;
}

ModbusCommand build_aeration_off_command(int unit_id, int coil_address) {
    ModbusCommand cmd;
    cmd.unit_id = unit_id;
    cmd.function_code = 5; // Write Single Coil
    cmd.address = coil_address;
    cmd.value = 0; // OFF
    return cmd;
}

ModbusCommand build_aeration_speed_command(int unit_id, int reg_address, int speed_percent) {
    ModbusCommand cmd;
    cmd.unit_id = unit_id;
    cmd.function_code = 6; // Write Single Register
    cmd.address = reg_address;
    cmd.value = std::max(0, std::min(100, speed_percent));
    return cmd;
}

// MQTT subscriber callback signature.
using SensorCallback = std::function<void(const SensorReading&)>;

// Controller ties moisture logic to Modbus commands.
class CompostAerationController {
public:
    CompostAerationController(MoistureThresholdLogic logic,
                              int modbus_unit_id,
                              int coil_address,
                              int speed_reg_address)
        : logic_(logic),
          modbus_unit_id_(modbus_unit_id),
          coil_address_(coil_address),
          speed_reg_address_(speed_reg_address)
    {}

    void on_sensor_reading(const SensorReading& r) {
        if (logic_.should_trigger(r)) {
            ModbusCommand on_cmd = build_aeration_on_command(modbus_unit_id_, coil_address_);
            ModbusCommand speed_cmd = build_aeration_speed_command(modbus_unit_id_, speed_reg_address_, 60);
            send_modbus(on_cmd);
            send_modbus(speed_cmd);
        } else {
            // Optionally, turn off if moisture has recovered.
        }
    }

    // Placeholder for actual Modbus send; in real wiring, connect to libmodbus or similar.
    void send_modbus(const ModbusCommand& cmd) {
        std::cout << "Modbus send: unit=" << cmd.unit_id
                  << " fc=" << cmd.function_code
                  << " addr=" << cmd.address
                  << " val=" << cmd.value << "\n";
    }

private:
    MoistureThresholdLogic logic_;
    int modbus_unit_id_;
    int coil_address_;
    int speed_reg_address_;
};

// Example main emulating MQTT messages arriving and Modbus commands being issued.
int main() {
    MoistureThresholdLogic logic(0.25, 1800); // threshold 25% moisture, min 30 min between triggers
    CompostAerationController controller(logic, 1, 10, 20);

    // Emulate sensor readings (ESP32 + BME280 + soil moisture) delivered via MQTT.
    std::vector<SensorReading> readings;
    readings.push_back(SensorReading{"node-1", 30.0, 70.0, 0.35, 1700000000});
    readings.push_back(SensorReading{"node-1", 31.0, 65.0, 0.20, 1700002000});
    readings.push_back(SensorReading{"node-1", 32.0, 60.0, 0.18, 1700006000});

    for (const auto& r : readings) {
        controller.on_sensor_reading(r);
    }

    return 0;
}
