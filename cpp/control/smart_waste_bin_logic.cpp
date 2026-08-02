// File: cpp/control/smart_waste_bin_logic.cpp
#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <cmath>
#include <iomanip>

// smart_waste_bin_logic:
// - Reads an ultrasonic fill-level sensor (simulated).
// - When threshold exceeded, triggers a WasteStreamOptimizer routing decision.
// - Sends the routing result over a BLE-like interface to a collector's tablet,
//   updating the community scheduler in near-real time.
//
// This code uses simulated sensor, optimizer, and BLE transport but preserves
// structural logic for real embedded/control integration.

namespace eco {

class UltrasonicFillSensor {
public:
    UltrasonicFillSensor(double bin_height_cm)
        : bin_height_cm_(bin_height_cm), current_distance_cm_(bin_height_cm) {}

    double read_distance_cm() const {
        // In real hardware, this would read from a sensor via I2C/ADC.
        return current_distance_cm_;
    }

    void simulate_fill(double added_height_cm) {
        current_distance_cm_ = std::max(0.0, current_distance_cm_ - added_height_cm);
    }

    double fill_level_fraction() const {
        double filled = bin_height_cm_ - current_distance_cm_;
        return std::clamp(filled / bin_height_cm_, 0.0, 1.0);
    }

private:
    double bin_height_cm_;
    double current_distance_cm_;
};

struct WasteItemAggregate {
    std::string material_stream;
    double total_mass_kg;
};

// Simple WasteStreamOptimizer: maps fill-level and stream to route priority.
class WasteStreamOptimizer {
public:
    WasteItemAggregate optimize(const std::string& stream,
                                double fill_fraction) const {
        WasteItemAggregate agg;
        agg.material_stream = stream;
        // The mass is approximated from fill fraction and bin capacity.
        double capacity_kg = (stream == "organics") ? 50.0 : 40.0;
        agg.total_mass_kg = capacity_kg * fill_fraction;
        return agg;
    }

    std::string select_route(const WasteItemAggregate& agg) const {
        // Route selection based on material and mass.
        if (agg.material_stream == "organics") {
            if (agg.total_mass_kg > 30.0) return "ROUTE-ORG-PRIORITY";
            return "ROUTE-ORG-NORMAL";
        }
        if (agg.material_stream == "recyclables") {
            if (agg.total_mass_kg > 25.0) return "ROUTE-REC-PRIORITY";
            return "ROUTE-REC-NORMAL";
        }
        return "ROUTE-MIXED";
    }
};

// Simulated BLE transmitter to collector's tablet.
class BLETransmitter {
public:
    BLETransmitter(std::string device_id)
        : device_id_(std::move(device_id)) {}

    void send_route_update(const std::string& route_id,
                           double fill_fraction,
                           const std::string& bin_id) {
        std::cout << "[BLE] Device " << device_id_
                  << " -> Tablet: bin=" << bin_id
                  << ", fill=" << std::fixed << std::setprecision(2) << (fill_fraction * 100.0) << "%, "
                  << "route=" << route_id << "\n";
    }

private:
    std::string device_id_;
};

class SmartWasteBinLogic {
public:
    SmartWasteBinLogic(const std::string& bin_id,
                       const std::string& material_stream,
                       double bin_height_cm,
                       double fill_threshold_fraction,
                       const std::string& ble_device_id)
        : bin_id_(bin_id),
          stream_(material_stream),
          sensor_(bin_height_cm),
          optimizer_(),
          ble_(ble_device_id),
          fill_threshold_(fill_threshold_fraction) {}

    void run_simulation(int steps, double dt_minutes) {
        std::cout << "Starting smart waste bin simulation for bin "
                  << bin_id_ << " [" << stream_ << "].\n";

        for (int i = 0; i < steps; ++i) {
            // Simulate incremental filling.
            sensor_.simulate_fill(1.0); // 1 cm per step.
            double fill_fraction = sensor_.fill_level_fraction();

            std::cout << "t=" << i * dt_minutes << " min, "
                      << "fill=" << std::fixed << std::setprecision(2)
                      << (fill_fraction * 100.0) << "%\n";

            if (fill_fraction >= fill_threshold_) {
                WasteItemAggregate agg = optimizer_.optimize(stream_, fill_fraction);
                std::string route_id = optimizer_.select_route(agg);
                ble_.send_route_update(route_id, fill_fraction, bin_id_);
                // After dispatch, reset threshold to avoid spamming.
                fill_threshold_ = 1.1; // effectively disable until emptied.
            }

            std::this_thread::sleep_for(
                std::chrono::milliseconds(static_cast<int>(dt_minutes * 1000))
            );
        }

        std::cout << "Smart waste bin simulation complete.\n";
    }

private:
    std::string bin_id_;
    std::string stream_;
    UltrasonicFillSensor sensor_;
    WasteStreamOptimizer optimizer_;
    BLETransmitter ble_;
    double fill_threshold_;
};

} // namespace eco

int main() {
    using namespace eco;

    SmartWasteBinLogic bin_logic(
        "BIN-ORG-001",
        "organics",
        80.0,   // bin height (cm)
        0.8,    // trigger when 80% full
        "BLE-EDGE-01"
    );

    bin_logic.run_simulation(20, 0.5); // 20 steps, 0.5 minute per step

    return 0;
}
