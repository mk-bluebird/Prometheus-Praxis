// File: cpp/actuation/shade_sail_controller.cpp
#include <iostream>
#include <chrono>
#include <thread>
#include <cmath>
#include <iomanip>

// shade_sail_controller:
// - Controls a stepper motor that extends/retracts a shade sail over a playground.
// - Uses predicted UTCI (Universal Thermal Climate Index) from a heat-island model
//   to decide sail position.
// - Designed for integration on a local controller; here stepper and UTCI are simulated.

namespace eco {

class StepperMotor {
public:
    StepperMotor(int steps_per_rev, double max_extension_steps)
        : steps_per_rev_(steps_per_rev),
          max_extension_steps_(max_extension_steps),
          current_steps_(0.0) {}

    // Set absolute extension as fraction [0..1] of max.
    void move_to_fraction(double frac) {
        double target_steps = std::clamp(frac, 0.0, 1.0) * max_extension_steps_;
        double delta = target_steps - current_steps_;
        step(delta);
    }

    double extension_fraction() const {
        if (max_extension_steps_ <= 0.0) return 0.0;
        return current_steps_ / max_extension_steps_;
    }

private:
    int    steps_per_rev_;
    double max_extension_steps_;
    double current_steps_;

    void step(double delta_steps) {
        int steps = static_cast<int>(std::round(delta_steps));
        if (steps == 0) return;

        int direction = (steps > 0) ? 1 : -1;
        steps = std::abs(steps);

        for (int i = 0; i < steps; ++i) {
            // In real hardware, toggle GPIO pins with correct sequence.
            // Here we just simulate timing.
            current_steps_ += direction;
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }

        std::cout << "[StepperMotor] Moved to " << current_steps_
                  << " steps (fraction=" << extension_fraction() << ")\n";
    }
};

// Simple UTCI-based controller logic.
// UTCI bands (approximate):
//   < 26 C: no thermal stress.
//   26-32 C: moderate heat.
//   32-38 C: strong heat.
//   > 38 C: very strong/extreme heat.
//
// We map UTCI to sail extension fraction [0..1].
class ShadeSailController {
public:
    ShadeSailController()
        : motor_(200, 2000.0) {}

    void apply_utci(double utci_C) {
        double frac = compute_extension_fraction(utci_C);
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "[ShadeSailController] UTCI=" << utci_C
                  << " C -> extension fraction=" << frac << "\n";
        motor_.move_to_fraction(frac);
    }

private:
    StepperMotor motor_;

    static double compute_extension_fraction(double utci_C) {
        if (utci_C < 26.0) {
            return 0.0; // retract: no strong heat stress
        } else if (utci_C < 32.0) {
            return 0.5; // half extension
        } else if (utci_C < 38.0) {
            return 0.8; // mostly extended
        } else {
            return 1.0; // fully extended for extreme heat
        }
    }
};

} // namespace eco

int main() {
    using namespace eco;

    ShadeSailController controller;

    // Simulated UTCI sequence from heat-island model.
    double utci_values[] = {25.0, 29.5, 34.0, 41.0};
    for (double u : utci_values) {
        controller.apply_utci(u);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    return 0;
}
