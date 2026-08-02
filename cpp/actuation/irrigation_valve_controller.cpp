// File: cpp/actuation/irrigation_valve_controller.cpp
#include <iostream>
#include <chrono>
#include <thread>
#include <cmath>
#include <algorithm>

// PID-based irrigation valve controller:
// - Controls a 24 V solenoid valve (on/off) via a digital output or relay.
// - Uses a PID controller on soil moisture error: (target_moisture - actual_moisture).
// - Obeys an eco-irrigation schedule (time windows and max daily runtime) derived
//   from a heat-island/eco-impact model.
// - Designed to be portable to microcontrollers; here we simulate actuation and sensing.

namespace eco {

struct EcoIrrigationWindow {
    int start_hour;   // local time hour (0-23)
    int end_hour;     // local time hour (0-23)
    double max_runtime_minutes; // cap on total valve open time per day
};

class SoilMoistureSensor {
public:
    explicit SoilMoistureSensor(double initial_value = 35.0)
        : moisture_(initial_value) {}

    double read_percent() const {
        // In real hardware, read from ADC or Modbus.
        return moisture_;
    }

    void simulate_evapotranspiration(double dt_minutes) {
        // Simple drying model.
        moisture_ = std::max(0.0, moisture_ - 0.05 * dt_minutes);
    }

    void simulate_irrigation(double dt_minutes) {
        // Moisture increases when valve is open.
        moisture_ = std::min(100.0, moisture_ + 0.3 * dt_minutes);
    }

private:
    double moisture_;
};

class ValveActuator {
public:
    ValveActuator() : is_open_(false), total_open_minutes_(0.0) {}

    void open(double dt_minutes) {
        is_open_ = true;
        total_open_minutes_ += dt_minutes;
    }

    void close() {
        is_open_ = false;
    }

    bool is_open() const {
        return is_open_;
    }

    double total_open_minutes() const {
        return total_open_minutes_;
    }

private:
    bool is_open_;
    double total_open_minutes_;
};

class PIDController {
public:
    PIDController(double Kp, double Ki, double Kd)
        : Kp_(Kp), Ki_(Ki), Kd_(Kd),
          integral_(0.0), prev_error_(0.0), first_(true) {}

    double compute(double error, double dt_minutes) {
        integral_ += error * dt_minutes;
        double derivative = 0.0;
        if (!first_) {
            derivative = (error - prev_error_) / dt_minutes;
        } else {
            first_ = false;
        }
        prev_error_ = error;
        return Kp_ * error + Ki_ * integral_ + Kd_ * derivative;
    }

    void reset() {
        integral_ = 0.0;
        prev_error_ = 0.0;
        first_ = true;
    }

private:
    double Kp_, Ki_, Kd_;
    double integral_;
    double prev_error_;
    bool first_;
};

class IrrigationValveController {
public:
    IrrigationValveController(double target_moisture_percent,
                              EcoIrrigationWindow window,
                              double Kp, double Ki, double Kd)
        : target_moisture_(target_moisture_percent),
          window_(window),
          pid_(Kp, Ki, Kd),
          sensor_(35.0),
          valve_(),
          current_hour_(0) {}

    void run_simulation(int total_hours, double dt_minutes) {
        std::cout << "Starting irrigation valve controller simulation.\n";
        std::cout << "Target moisture: " << target_moisture_ << "%\n";

        int total_steps = static_cast<int>((total_hours * 60) / dt_minutes);
        for (int step = 0; step < total_steps; ++step) {
            double moisture = sensor_.read_percent();
            double error = target_moisture_ - moisture;

            bool in_window = (current_hour_ >= window_.start_hour &&
                              current_hour_ <  window_.end_hour);
            bool under_runtime_cap = valve_.total_open_minutes() < window_.max_runtime_minutes;

            double control = 0.0;
            if (in_window && under_runtime_cap) {
                control = pid_.compute(error, dt_minutes);
            } else {
                pid_.reset();
            }

            // Simple on/off logic based on PID output.
            if (in_window && under_runtime_cap && control > 0.5) {
                valve_.open(dt_minutes);
                sensor_.simulate_irrigation(dt_minutes);
            } else {
                valve_.close();
            }

            // Always simulate drying.
            sensor_.simulate_evapotranspiration(dt_minutes);

            if (step % static_cast<int>(60.0 / dt_minutes) == 0) {
                std::cout << "Hour " << current_hour_
                          << ", moisture=" << moisture << "%, "
                          << "error=" << error << ", "
                          << "valve_open=" << (valve_.is_open() ? "yes" : "no")
                          << ", total_open_minutes=" << valve_.total_open_minutes()
                          << "\n";
            }

            // Advance time.
            double minutes_passed = dt_minutes;
            double hours_increment = minutes_passed / 60.0;
            current_hour_ = (current_hour_ + hours_increment);
            if (current_hour_ >= 24.0) {
                current_hour_ -= 24.0;
                valve_.close();
            }
        }

        std::cout << "Simulation complete. Total valve open time: "
                  << valve_.total_open_minutes() << " minutes.\n";
    }

private:
    double target_moisture_;
    EcoIrrigationWindow window_;
    PIDController pid_;
    SoilMoistureSensor sensor_;
    ValveActuator valve_;
    double current_hour_;
};

} // namespace eco

int main() {
    using namespace eco;

    EcoIrrigationWindow window;
    window.start_hour = 4;            // 4 AM
    window.end_hour   = 9;            // 9 AM
    window.max_runtime_minutes = 60;  // no more than 1 hour per day

    IrrigationValveController controller(
        40.0,   // target moisture (%)
        window,
        0.3,    // Kp
        0.02,   // Ki
        0.05    // Kd
    );

    controller.run_simulation(24, 10.0);

    return 0;
}
