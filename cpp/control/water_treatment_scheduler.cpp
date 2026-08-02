// File: cpp/control/water_treatment_scheduler.cpp
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <iomanip>
#include <cmath>

// water_treatment_scheduler:
// - Sequences a small-scale constructed wetland's pump and aeration cycles
//   using water_quality_index predictions (0..1; higher = cleaner).
// - Logs energy and water saved vs. a conventional treatment baseline.
// - Self-contained and suitable for integration on a local controller.

namespace eco {

struct WaterQualityPrediction {
    double index;        // 0..1, higher = cleaner water
    double flow_lps;     // inflow rate (liters per second)
};

class PumpController {
public:
    PumpController(double power_W) : power_W_(power_W), running_(false) {}

    void start() {
        if (!running_) {
            running_ = true;
            std::cout << "[PumpController] Pump STARTED.\n";
        }
    }

    void stop() {
        if (running_) {
            running_ = false;
            std::cout << "[PumpController] Pump STOPPED.\n";
        }
    }

    bool is_running() const { return running_; }
    double power_W() const { return power_W_; }

private:
    double power_W_;
    bool running_;
};

class AerationController {
public:
    AerationController(double power_W) : power_W_(power_W), running_(false) {}

    void start() {
        if (!running_) {
            running_ = true;
            std::cout << "[AerationController] Aerator STARTED.\n";
        }
    }

    void stop() {
        if (running_) {
            running_ = false;
            std::cout << "[AerationController] Aerator STOPPED.\n";
        }
    }

    bool is_running() const { return running_; }
    double power_W() const { return power_W_; }

private:
    double power_W_;
    bool running_;
};

struct TreatmentLogEntry {
    int    cycle_index;
    double time_minutes;
    double water_quality_index;
    double energy_used_kWh_wetland;
    double energy_baseline_kWh;
    double water_saved_m3;
};

class WaterTreatmentScheduler {
public:
    WaterTreatmentScheduler(double pump_power_W,
                            double aeration_power_W,
                            double baseline_energy_kWh_per_m3,
                            double baseline_water_loss_fraction)
        : pump_(pump_power_W),
          aeration_(aeration_power_W),
          baseline_energy_kWh_per_m3_(baseline_energy_kWh_per_m3),
          baseline_water_loss_fraction_(baseline_water_loss_fraction),
          cumulative_energy_wetland_kWh_(0.0),
          cumulative_energy_baseline_kWh_(0.0),
          cumulative_water_saved_m3_(0.0) {}

    void run_schedule(const std::vector<WaterQualityPrediction>& predictions,
                      double cycle_minutes) {
        std::cout << std::fixed << std::setprecision(3);
        std::cout << "Starting water treatment scheduler.\n";

        logs_.clear();
        double time_minutes = 0.0;

        for (std::size_t i = 0; i < predictions.size(); ++i) {
            const auto& p = predictions[i];
            double wqi = p.index;
            double flow_lps = p.flow_lps;

            // Scheduling logic:
            // - If water_quality_index < 0.6, run pump + aeration.
            // - If 0.6 <= index < 0.8, run pump only.
            // - If index >= 0.8, let wetland rest (no active treatment).
            if (wqi < 0.6) {
                pump_.start();
                aeration_.start();
            } else if (wqi < 0.8) {
                pump_.start();
                aeration_.stop();
            } else {
                pump_.stop();
                aeration_.stop();
            }

            // Energy accounting for this cycle.
            double hours = cycle_minutes / 60.0;
            double energy_cycle_wetland_kWh = 0.0;
            if (pump_.is_running()) {
                energy_cycle_wetland_kWh += pump_.power_W() * hours / 1000.0;
            }
            if (aeration_.is_running()) {
                energy_cycle_wetland_kWh += aeration_.power_W() * hours / 1000.0;
            }
            cumulative_energy_wetland_kWh_ += energy_cycle_wetland_kWh;

            // Water volume treated this cycle (m3).
            double volume_m3 = flow_lps * (cycle_minutes * 60.0) / 1000.0;

            // Baseline energy and water loss for same volume.
            double baseline_energy_kWh = baseline_energy_kWh_per_m3_ * volume_m3;
            cumulative_energy_baseline_kWh_ += baseline_energy_kWh;

            double baseline_water_loss_m3 = baseline_water_loss_fraction_ * volume_m3;
            double wetland_water_loss_m3 = 0.1 * baseline_water_loss_m3; // wetlands lose much less
            double water_saved_m3 = baseline_water_loss_m3 - wetland_water_loss_m3;
            cumulative_water_saved_m3_ += water_saved_m3;

            TreatmentLogEntry entry;
            entry.cycle_index = static_cast<int>(i);
            entry.time_minutes = time_minutes;
            entry.water_quality_index = wqi;
            entry.energy_used_kWh_wetland = energy_cycle_wetland_kWh;
            entry.energy_baseline_kWh = baseline_energy_kWh;
            entry.water_saved_m3 = water_saved_m3;
            logs_.push_back(entry);

            log_cycle(entry);

            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            time_minutes += cycle_minutes;
        }

        std::cout << "Scheduler complete.\n";
        std::cout << "Total wetland energy: " << cumulative_energy_wetland_kWh_
                  << " kWh, baseline energy: " << cumulative_energy_baseline_kWh_
                  << " kWh\n";
        std::cout << "Total water saved vs baseline: "
                  << cumulative_water_saved_m3_ << " m3\n";
    }

private:
    PumpController   pump_;
    AerationController aeration_;
    double baseline_energy_kWh_per_m3_;
    double baseline_water_loss_fraction_;

    double cumulative_energy_wetland_kWh_;
    double cumulative_energy_baseline_kWh_;
    double cumulative_water_saved_m3_;
    std::vector<TreatmentLogEntry> logs_;

    static void log_cycle(const TreatmentLogEntry& e) {
        std::cout << "[Cycle " << e.cycle_index << "] t=" << e.time_minutes << " min, "
                  << "WQI=" << e.water_quality_index
                  << ", E_wetland=" << e.energy_used_kWh_wetland << " kWh"
                  << ", E_baseline=" << e.energy_baseline_kWh << " kWh"
                  << ", water_saved=" << e.water_saved_m3 << " m3\n";
    }
};

} // namespace eco

int main() {
    using namespace eco;

    // Example predictions for a small constructed wetland.
    std::vector<WaterQualityPrediction> preds = {
        {0.5, 5.0}, {0.55, 5.0}, {0.65, 4.0}, {0.78, 4.0}, {0.85, 3.0}
    };

    WaterTreatmentScheduler scheduler(
        150.0,  // pump power W
        80.0,   // aeration power W
        0.5,    // baseline energy kWh per m3
        0.15    // baseline water loss fraction
    );

    scheduler.run_schedule(preds, 15.0); // 15-minute cycles

    return 0;
}
