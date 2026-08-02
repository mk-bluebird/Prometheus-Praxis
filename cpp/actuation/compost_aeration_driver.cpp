// File: cpp/actuation/compost_aeration_driver.cpp
#include <iostream>
#include <chrono>
#include <thread>
#include <cmath>
#include <iomanip>

// Compost aeration driver:
// - Controls a 12 V blower motor via PWM (duty cycle).
// - Uses a schedule (on/off windows) computed by compost_pile_simulator running on-device.
// - Includes stall detection (current-based or speed-based).
// - Logs eco-impact events through a simple eco-logger interface.
//
// This file simulates PWM and stall detection without hardware dependencies.

namespace eco {

struct AerationEvent {
    std::chrono::system_clock::time_point timestamp;
    double duty_cycle;     // 0..1
    bool   stall_detected;
};

class EcoLogger {
public:
    void log(const AerationEvent& evt) {
        std::time_t tt = std::chrono::system_clock::to_time_t(evt.timestamp);
        std::cout << "[EcoLogger] ";
        std::cout << std::put_time(std::gmtime(&tt), "%F %T")
                  << " duty=" << evt.duty_cycle
                  << " stall=" << (evt.stall_detected ? "YES" : "NO")
                  << "\n";
    }
};

struct AerationSlot {
    int start_minute; // minutes since midnight
    int end_minute;   // minutes since midnight
    double duty_cycle;
};

class BlowerMotor {
public:
    BlowerMotor() : duty_cycle_(0.0), stalled_(false) {}

    void set_pwm(double duty_cycle, double motor_current_A) {
        duty_cycle_ = std::clamp(duty_cycle, 0.0, 1.0);
        stalled_ = detect_stall(motor_current_A, duty_cycle_);
    }

    bool stalled() const { return stalled_; }
    double duty_cycle() const { return duty_cycle_; }

private:
    double duty_cycle_;
    bool stalled_;

    static bool detect_stall(double motor_current_A, double duty_cycle) {
        // Simple stall detection:
        // - If duty cycle is high (>0.7) and current exceeds threshold, consider stalled.
        const double current_threshold = 3.0; // Amps
        return (duty_cycle > 0.7 && motor_current_A > current_threshold);
    }
};

class CompostAerationDriver {
public:
    CompostAerationDriver(const std::vector<AerationSlot>& schedule)
        : schedule_(schedule),
          blower_(),
          logger_(),
          current_minute_(0) {}

    void run_simulation(int total_minutes, double dt_minutes) {
        std::cout << "Starting compost aeration driver simulation.\n";
        int steps = static_cast<int>(total_minutes / dt_minutes);

        for (int step = 0; step < steps; ++step) {
            double duty = compute_duty_for_minute(current_minute_);
            double simulated_current = simulate_motor_current(duty);

            blower_.set_pwm(duty, simulated_current);

            AerationEvent evt;
            evt.timestamp = std::chrono::system_clock::now();
            evt.duty_cycle = blower_.duty_cycle();
            evt.stall_detected = blower_.stalled();
            logger_.log(evt);

            if (blower_.stalled()) {
                std::cout << "[CompostAerationDriver] Stall detected at minute "
                          << current_minute_ << ", reducing duty cycle.\n";
            }

            current_minute_ += static_cast<int>(dt_minutes);
            if (current_minute_ >= 24 * 60) {
                current_minute_ -= 24 * 60;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        std::cout << "Compost aeration simulation complete.\n";
    }

private:
    std::vector<AerationSlot> schedule_;
    BlowerMotor blower_;
    EcoLogger logger_;
    int current_minute_;

    double compute_duty_for_minute(int minute) const {
        for (const auto& slot : schedule_) {
            if (minute >= slot.start_minute && minute < slot.end_minute) {
                return slot.duty_cycle;
            }
        }
        return 0.0;
    }

    static double simulate_motor_current(double duty_cycle) {
        // Simple current model: base current plus load; add noise and stall spike.
        double base_current = 0.5; // A
        double load_current = duty_cycle * 2.0;
        double current = base_current + load_current;

        if (duty_cycle > 0.8) {
            current += 1.0; // extra load at high duty
        }
        return current;
    }
};

} // namespace eco

int main() {
    using namespace eco;

    // Example schedule from compost_pile_simulator:
    // Two aeration windows per day, 20 minutes each.
    std::vector<AerationSlot> schedule;
    schedule.push_back(AerationSlot{6 * 60, 6 * 60 + 20, 0.8});  // 06:00-06:20
    schedule.push_back(AerationSlot{18 * 60, 18 * 60 + 20, 0.6}); // 18:00-18:20

    CompostAerationDriver driver(schedule);
    driver.run_simulation(60, 5.0); // simulate 1 hour in 5-minute steps

    return 0;
}
