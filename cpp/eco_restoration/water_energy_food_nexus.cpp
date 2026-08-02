// File: cpp/eco_restoration/water_energy_food_nexus.cpp

#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <cmath>

struct NexusState {
    double w_in;        // imported water [m3/day]
    double r_reclaim;   // reclaimed/recycled water [m3/day]
    double w_out;       // exported/consumed water (including discharge) [m3/day]
    double et;          // evapotranspiration from green infra [m3/day equivalent]
    double delta_s;     // change in storage [m3/day]
};

struct BiosignalIntegration {
    // Real-time biosignal integration field capturing ET-related signals
    // For simplicity, we model ET as a function of vegetation index and soil moisture.
    double vegetation_index; // e.g., NDVI or similar scaled [0,1]
    double soil_moisture;    // scaled [0,1]
    double air_temperature;  // °C
};

class ETTracker {
public:
    // Simple ET estimation using a linear biosignal model:
    // ET ~ k_veg * vegetation_index + k_moist * soil_moisture + k_temp * max(0, air_temperature - T_ref)
    // converted to m3/day for the hex cell via an area factor.
    double estimate_et(const BiosignalIntegration& bio,
                       double cell_area_m2,
                       double k_veg,
                       double k_moist,
                       double k_temp,
                       double t_ref) const
    {
        double term_veg   = k_veg * bio.vegetation_index;
        double term_moist = k_moist * bio.soil_moisture;
        double term_temp  = k_temp * std::max(0.0, bio.air_temperature - t_ref);

        // ET in mm/day over the cell area
        double et_mm_per_day = term_veg + term_moist + term_temp;
        if (et_mm_per_day < 0.0) et_mm_per_day = 0.0;

        // Convert mm/day to m3/day: 1 mm over 1 m2 = 0.001 m3
        double et_m3_per_day = et_mm_per_day * 0.001 * cell_area_m2;
        return et_m3_per_day;
    }
};

struct SovereigntyPolicy {
    // Maximum net consumptive use per hex cell under Arizona water law and local agreements.
    // Net consumptive use = W_in + R_reclaim - W_out - ET - DeltaS_returnable
    double max_net_consumptive_use_m3_per_day;
};

class NexusBalanceChecker {
public:
    // Check nexus balance and sovereignty inequality:
    // W_in + R_reclaim - W_out - ET = ΔS
    // Sovereignty constraint: Net consumptive use <= max_net_consumptive_use.
    bool check_balance_and_sovereignty(const NexusState& state,
                                       const SovereigntyPolicy& policy,
                                       double storage_returnable_fraction,
                                       std::string& violation_reason) const
    {
        // Check balance equation consistency
        double lhs = state.w_in + state.r_reclaim - state.w_out - state.et;
        double rhs = state.delta_s;
        double balance_error = std::abs(lhs - rhs);

        if (balance_error > 1e-3) {
            violation_reason = "Nexus balance equation not satisfied; check water accounting.";
            return false;
        }

        // Compute net consumptive use: portion of ΔS that is not returnable
        double delta_s_returnable = storage_returnable_fraction * state.delta_s;
        double net_consumptive_use = state.w_in + state.r_reclaim - state.w_out - state.et - delta_s_returnable;

        if (net_consumptive_use > policy.max_net_consumptive_use_m3_per_day) {
            violation_reason = "Net consumptive use exceeds sovereignty limit under Arizona water law.";
            return false;
        }

        violation_reason.clear();
        return true;
    }
};

int main() {
    // Example biosignal inputs for a green hex cell in Phoenix
    BiosignalIntegration bio{
        0.7,   // vegetation_index
        0.5,   // soil_moisture
        38.0   // air_temperature [°C]
    };

    double cell_area_m2 = 10000.0; // 1 ha hex cell
    ETTracker et_tracker;

    // Coefficients tuned from calibration against micrometeorological ET estimates
    double k_veg   = 2.0;  // mm/day per unit vegetation index
    double k_moist = 1.5;  // mm/day per unit soil moisture
    double k_temp  = 0.2;  // mm/day per °C above T_ref
    double t_ref   = 25.0; // reference temperature

    double et_m3_per_day = et_tracker.estimate_et(bio, cell_area_m2,
                                                  k_veg, k_moist, k_temp, t_ref);

    // Example nexus state for a hex cell
    NexusState state{
        150.0,              // W_in [m3/day]
        30.0,               // R_reclaim [m3/day]
        100.0,              // W_out [m3/day]
        et_m3_per_day,      // ET [m3/day]
        10.0                // ΔS [m3/day]
    };

    SovereigntyPolicy policy{
        50.0 // max net consumptive use [m3/day] (example value)
    };

    NexusBalanceChecker checker;
    std::string reason;
    double storage_returnable_fraction = 0.3; // fraction of ΔS that can be legally returned

    bool ok = checker.check_balance_and_sovereignty(state, policy,
                                                    storage_returnable_fraction, reason);

    std::cout << "Estimated ET (m3/day) from biosignal_integration: " << et_m3_per_day << "\n";
    if (ok) {
        std::cout << "Nexus balance and sovereignty inequality satisfied for this hex cell.\n";
    } else {
        std::cout << "Nexus balance / sovereignty violation: " << reason << "\n";
    }

    return 0;
}
