// File: cpp/control/modbus_machine_monitor.cpp
#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <cstdint>
#include <iomanip>
#include <random>

// modbus_machine_monitor:
// - Polls simulated Modbus TCP registers for an industrial machine:
//     * energy consumption (kWh)
//     * cycle count
// - Classifies cycles with EcoMachineCycleAssessor (eco / non-eco).
// - Publishes summaries to an MQTT-like interface (simulated print)
//   for downstream energy reporting.
//
// This implementation avoids external Modbus/MQTT libraries and uses
// deterministic simulation suitable for control-plane integration.

namespace eco {

// Simulated Modbus TCP client: in real deployment, this would use a Modbus library.
class ModbusTCPClient {
public:
    ModbusTCPClient(std::string host, uint16_t port)
        : host_(std::move(host)), port_(port),
          energy_kwh_(0.0), cycle_count_(0), rng_(42) {}

    bool connect() {
        // Simulate successful connection.
        std::cout << "[ModbusTCPClient] Connected to " << host_ << ":" << port_ << "\n";
        return true;
    }

    bool read_registers(double& energy_kwh, uint32_t& cycle_count) {
        // Simulate machine activity: increase energy and cycles with randomness.
        std::uniform_real_distribution<double> energy_inc(0.1, 0.5);
        std::uniform_int_distribution<uint32_t> cycle_inc(1, 5);

        energy_kwh_ += energy_inc(rng_);
        cycle_count_ += cycle_inc(rng_);

        energy_kwh = energy_kwh_;
        cycle_count = cycle_count_;
        return true;
    }

private:
    std::string host_;
    uint16_t    port_;
    double      energy_kwh_;
    uint32_t    cycle_count_;
    mutable std::mt19937 rng_;
};

// EcoMachineCycleAssessor:
// - Classifies machine cycles as eco-positive or not based on
//   energy per cycle threshold and simple heuristics.
// - Provides an eco-cycle ratio for reporting.
class EcoMachineCycleAssessor {
public:
    EcoMachineCycleAssessor(double max_energy_per_cycle_kwh)
        : max_energy_per_cycle_kwh_(max_energy_per_cycle_kwh),
          last_total_cycles_(0),
          last_total_energy_(0.0),
          eco_cycles_(0),
          non_eco_cycles_(0) {}

    void update(double total_energy_kwh, uint32_t total_cycles) {
        uint32_t new_cycles = total_cycles - last_total_cycles_;
        double   new_energy = total_energy_kwh - last_total_energy_;
        if (new_cycles == 0) {
            return;
        }

        double energy_per_cycle = new_energy / static_cast<double>(new_cycles);
        bool eco = (energy_per_cycle <= max_energy_per_cycle_kwh_);

        if (eco) {
            eco_cycles_ += new_cycles;
        } else {
            non_eco_cycles_ += new_cycles;
        }

        last_total_cycles_ = total_cycles;
        last_total_energy_ = total_energy_kwh;
    }

    double eco_ratio() const {
        uint32_t total = eco_cycles_ + non_eco_cycles_;
        if (total == 0) return 0.0;
        return static_cast<double>(eco_cycles_) / static_cast<double>(total);
    }

    uint32_t eco_cycles() const { return eco_cycles_; }
    uint32_t non_eco_cycles() const { return non_eco_cycles_; }

private:
    double   max_energy_per_cycle_kwh_;
    uint32_t last_total_cycles_;
    double   last_total_energy_;
    uint32_t eco_cycles_;
    uint32_t non_eco_cycles_;
};

// Simulated MQTT publisher: prints topic/payload.
class MQTTPublisher {
public:
    MQTTPublisher(std::string broker_host, uint16_t port)
        : broker_host_(std::move(broker_host)), port_(port) {}

    bool connect() {
        std::cout << "[MQTTPublisher] Connected to broker " << broker_host_
                  << ":" << port_ << "\n";
        return true;
    }

    void publish(const std::string& topic, const std::string& payload) {
        std::cout << "[MQTT] Topic: " << topic << "\n";
        std::cout << "       Payload: " << payload << "\n";
    }

private:
    std::string broker_host_;
    uint16_t    port_;
};

class ModbusMachineMonitor {
public:
    ModbusMachineMonitor(const std::string& modbus_host,
                         uint16_t modbus_port,
                         const std::string& mqtt_host,
                         uint16_t mqtt_port,
                         double max_energy_per_cycle_kwh)
        : modbus_client_(modbus_host, modbus_port),
          assessor_(max_energy_per_cycle_kwh),
          mqtt_(mqtt_host, mqtt_port) {}

    void run(int intervals, double interval_seconds) {
        if (!modbus_client_.connect()) {
            std::cerr << "Modbus connection failed.\n";
            return;
        }
        if (!mqtt_.connect()) {
            std::cerr << "MQTT connection failed.\n";
            return;
        }

        std::cout << std::fixed << std::setprecision(3);

        for (int i = 0; i < intervals; ++i) {
            double energy_kwh;
            uint32_t cycles;
            if (!modbus_client_.read_registers(energy_kwh, cycles)) {
                std::cerr << "Failed to read Modbus registers.\n";
                break;
            }

            assessor_.update(energy_kwh, cycles);

            double eco_ratio = assessor_.eco_ratio();
            std::string payload = build_payload(energy_kwh, cycles, eco_ratio);
            mqtt_.publish("factory/energy/machine1", payload);

            std::this_thread::sleep_for(
                std::chrono::milliseconds(static_cast<int>(interval_seconds * 1000))
            );
        }
    }

private:
    ModbusTCPClient        modbus_client_;
    EcoMachineCycleAssessor assessor_;
    MQTTPublisher          mqtt_;

    static std::string build_payload(double energy_kwh,
                                     uint32_t cycles,
                                     double eco_ratio) {
        std::ostringstream oss;
        oss << "{"
            << "\"energy_kwh\":" << energy_kwh << ","
            << "\"cycles\":" << cycles << ","
            << "\"eco_cycle_ratio\":" << eco_ratio
            << "}";
        return oss.str();
    }
};

} // namespace eco

int main() {
    using namespace eco;

    ModbusMachineMonitor monitor(
        "192.168.0.10", 502,
        "192.168.0.20", 1883,
        0.15 // max energy per cycle (kWh) considered eco-positive
    );

    monitor.run(10, 2.0); // 10 intervals, 2 seconds apart

    return 0;
}
