// File: cpp/middleware/eco_data_fusion_engine.cpp
#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <cmath>
#include <iomanip>

// EcoDataFusionEngine:
// - Implements a simple Kalman filter per hex cell.
// - Fuses soil moisture, water quality, and weather forecast into a unified
//   "irrigation readiness" index (0..1).
// - Designed as middleware between raw sensors and higher-level controllers.
//
// State model (per hex):
//   x_k = irrigation_readiness at time k.
//   x_k = x_{k-1} + w_k,  w_k ~ N(0, Q)
//
// Measurements:
//   z1_k: soil moisture (%)
//   z2_k: water quality index (0..1; higher = cleaner)
//   z3_k: forecast evapotranspiration demand index (0..1; higher = more demand)
//
// We map measurements to an "ideal" readiness observation h_k via a static function,
// then apply a scalar Kalman update.

namespace eco {

struct FusionInput {
    double soil_moisture_pct;       // 0..100
    double water_quality_index;     // 0..1
    double forecast_evapo_index;    // 0..1
};

struct FusionState {
    double readiness;               // 0..1
    double variance;                // uncertainty
};

class EcoDataFusionEngine {
public:
    EcoDataFusionEngine(double process_var,
                        double meas_var)
        : Q_(process_var),
          R_(meas_var) {}

    // Update irrigation readiness for a given hex.
    FusionState update_hex(const std::string& hex_id,
                           const FusionInput& input) {
        FusionState& st = states_[hex_id];

        if (!initialized_[hex_id]) {
            st.readiness = initial_readiness(input);
            st.variance  = 0.5; // initial uncertainty
            initialized_[hex_id] = true;
            return st;
        }

        // Predict step: x_k^- = x_{k-1}, P_k^- = P_{k-1} + Q.
        double x_prior = st.readiness;
        double P_prior = st.variance + Q_;

        // Measurement model: z = h(input), scalar.
        double z = measurement_readiness(input);

        // Kalman gain: K = P_prior / (P_prior + R).
        double K = P_prior / (P_prior + R_);

        // Update state.
        double x_post = x_prior + K * (z - x_prior);
        double P_post = (1.0 - K) * P_prior;

        st.readiness = clamp01(x_post);
        st.variance  = P_post;

        return st;
    }

    void print_state(const std::string& hex_id) const {
        auto it = states_.find(hex_id);
        if (it == states_.end()) {
            std::cout << "Hex " << hex_id << " has no fusion state.\n";
            return;
        }
        const FusionState& st = it->second;
        std::cout << std::fixed << std::setprecision(3);
        std::cout << "Hex " << hex_id
                  << " irrigation_readiness=" << st.readiness
                  << " variance=" << st.variance << "\n";
    }

private:
    double Q_; // process variance
    double R_; // measurement variance

    std::unordered_map<std::string, FusionState> states_;
    std::unordered_map<std::string, bool> initialized_;

    static double clamp01(double x) {
        if (x < 0.0) return 0.0;
        if (x > 1.0) return 1.0;
        return x;
    }

    static double initial_readiness(const FusionInput& in) {
        return clamp01(measurement_readiness(in));
    }

    static double measurement_readiness(const FusionInput& in) {
        // Map raw inputs to an irrigation readiness suggestion:
        // - Soil moisture: low moisture -> high readiness.
        // - Water quality: higher quality -> higher readiness.
        // - Forecast evapotranspiration: higher demand -> higher readiness.
        double soil_term = 1.0 - (in.soil_moisture_pct / 100.0);      // dry -> 1
        double water_term = in.water_quality_index;                   // clean -> 1
        double evapo_term = in.forecast_evapo_index;                  // high demand -> 1

        // Weighted combination.
        double readiness = 0.5 * soil_term + 0.3 * water_term + 0.2 * evapo_term;
        return readiness;
    }
};

} // namespace eco

int main() {
    using namespace eco;

    EcoDataFusionEngine engine(
        0.02, // process variance
        0.05  // measurement variance
    );

    // Simulated updates for a single hex.
    std::string hex_id = "PHX-HX-0-0";
    FusionInput in1{35.0, 0.8, 0.6};
    FusionInput in2{28.0, 0.85, 0.7};
    FusionInput in3{42.0, 0.9, 0.4};

    FusionState st1 = engine.update_hex(hex_id, in1);
    engine.print_state(hex_id);

    FusionState st2 = engine.update_hex(hex_id, in2);
    engine.print_state(hex_id);

    FusionState st3 = engine.update_hex(hex_id, in3);
    engine.print_state(hex_id);

    return 0;
}
