// File: cpp/embedded/solar_panel_tracker.cpp
#include <iostream>
#include <chrono>
#include <thread>
#include <cmath>
#include <iomanip>

// Embedded solar panel tracker:
// - Reads PV voltage and current (simulated here).
// - Computes approximate maximum power point (MPP) via perturb-and-observe (P&O).
// - Adjusts a linear actuator position (panel orientation) to maximize harvested power.
// - Logs cumulative energy harvested over time.
// The code is self-contained and ready for adaptation to microcontroller hardware.

namespace eco {

class PVSensor {
public:
    PVSensor() : irradiance_Wm2_(800.0), temperature_C_(35.0) {}

    // Simulate PV voltage/current as a function of actuator position (0..1).
    // In real hardware, these would come from ADC measurements.
    void read(double actuator_position, double& voltage_V, double& current_A) const {
        double pos_factor = 0.5 + 0.5 * std::cos((actuator_position - 0.5) * M_PI);
        double effective_irradiance = irradiance_Wm2_ * pos_factor;

        voltage_V = 18.0 - 0.02 * (temperature_C_ - 25.0); // simple temp dependence
        current_A = (effective_irradiance / 1000.0) * 3.0; // scaled ISC at STC
    }

private:
    double irradiance_Wm2_;
    double temperature_C_;
};

class LinearActuator {
public:
    LinearActuator() : position_(0.5) {}

    // Set position in [0, 1].
    void set_position(double pos) {
        position_ = std::clamp(pos, 0.0, 1.0);
    }

    double position() const {
        return position_;
    }

private:
    double position_;
};

class SolarPanelTracker {
public:
    SolarPanelTracker()
        : pv_(), actuator_(),
          cumulative_energy_Wh_(0.0),
          step_size_(0.05),
          last_power_W_(0.0),
          direction_(1.0) {}

    void run_tracking_loop(double total_minutes, double dt_seconds) {
        int steps = static_cast<int>((total_minutes * 60.0) / dt_seconds);
        std::cout << std::fixed << std::setprecision(3);
        std::cout << "Starting solar panel tracker for " << total_minutes << " minutes.\n";

        for (int i = 0; i < steps; ++i) {
            double V, I;
            pv_.read(actuator_.position(), V, I);
            double P = V * I;

            // MPPT perturb-and-observe logic.
            if (i == 0) {
                last_power_W_ = P;
            } else {
                if (P > last_power_W_) {
                    // Continue in same direction.
                } else {
                    // Reverse direction.
                    direction_ = -direction_;
                }
                double new_pos = actuator_.position() + direction_ * step_size_;
                actuator_.set_position(new_pos);
                last_power_W_ = P;
            }

            // Integrate energy.
            cumulative_energy_Wh_ += P * (dt_seconds / 3600.0);

            if (i % static_cast<int>(60.0 / dt_seconds) == 0) {
                std::cout << "t=" << (i * dt_seconds) / 60.0 << " min, "
                          << "pos=" << actuator_.position()
                          << ", V=" << V << " V, I=" << I << " A, P=" << P << " W, "
                          << "E_cum=" << cumulative_energy_Wh_ << " Wh\n";
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        std::cout << "Tracking complete. Total energy harvested: "
                  << cumulative_energy_Wh_ << " Wh\n";
    }

private:
    PVSensor pv_;
    LinearActuator actuator_;
    double cumulative_energy_Wh_;
    double step_size_;
    double last_power_W_;
    double direction_;
};

} // namespace eco

int main() {
    using namespace eco;

    SolarPanelTracker tracker;
    tracker.run_tracking_loop(30.0, 5.0); // 30 minutes, 5-second steps

    return 0;
}
