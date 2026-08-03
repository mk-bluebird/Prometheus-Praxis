// File: cpp/eco_restoration/pfas_fate_corridor.cpp
#include <cmath>
#include <stdexcept>

namespace eco_restoration {

struct PFASState {
    double mass_kg;             // total PFAS mass in the corridor
    double sorbed_fraction;     // fraction sorbed to sediments [0,1]
    double cold_survival_factor;// multiplier for degradation slowdown, >= 0
};

struct PFASStepParams {
    double base_degradation_rate;   // per-step fractional mass loss at reference temperature
    double cold_temp_C;            // threshold temperature for cold-survival corridor (e.g. 12°C)
    double current_temp_C;         // current canal/sediment temperature
    double sorption_increment;     // per-step change in sorbed fraction
};

struct PFASStepResult {
    PFASState next_state;
    double lyapunov_residual_Vt;
    bool corridor_breach;
};

class PFASCorridor {
public:
    PFASCorridor(double max_mass_kg,
                 double max_cold_survival_factor,
                 double w_mass,
                 double w_sorbed,
                 double w_cold)
        : max_mass_kg_(max_mass_kg),
          max_cold_survival_factor_(max_cold_survival_factor),
          w_mass_(w_mass),
          w_sorbed_(w_sorbed),
          w_cold_(w_cold) {
        if (max_mass_kg_ <= 0.0 || max_cold_survival_factor_ <= 0.0) {
            throw std::invalid_argument("PFASCorridor limits must be positive");
        }
        if (w_mass_ < 0.0 || w_sorbed_ < 0.0 || w_cold_ < 0.0) {
            throw std::invalid_argument("PFASCorridor weights must be nonnegative");
        }
    }

    PFASStepResult step(const PFASState& current, const PFASStepParams& params) const {
        PFASState next = current;

        double rate = params.base_degradation_rate;
        if (rate < 0.0) rate = 0.0;
        if (rate > 1.0) rate = 1.0;

        // Cold-survival adjustment: degradation slows when temperature is below corridor threshold.
        double cold_factor = current.cold_survival_factor;
        if (params.current_temp_C <= params.cold_temp_C) {
            cold_factor *= 1.02; // cold-survival corridor: PFAS persists more
        } else {
            cold_factor *= 0.99; // warmer corridor: faster degradation
        }
        if (cold_factor < 0.0) cold_factor = 0.0;

        next.cold_survival_factor = cold_factor;

        double effective_rate = rate / (1.0 + cold_factor);
        double remaining_mass = current.mass_kg * (1.0 - effective_rate);
        if (remaining_mass < 0.0) remaining_mass = 0.0;
        next.mass_kg = remaining_mass;

        double sorbed_next = current.sorbed_fraction + params.sorption_increment;
        if (sorbed_next < 0.0) sorbed_next = 0.0;
        if (sorbed_next > 1.0) sorbed_next = 1.0;
        next.sorbed_fraction = sorbed_next;

        // Normalize state coordinates to [0,1] for Lyapunov residual.
        double r_mass   = next.mass_kg / max_mass_kg_;
        double r_sorbed = next.sorbed_fraction;
        double r_cold   = next.cold_survival_factor / max_cold_survival_factor_;

        if (r_mass < 0.0) r_mass = 0.0;
        if (r_sorbed < 0.0) r_sorbed = 0.0;
        if (r_cold < 0.0) r_cold = 0.0;

        double Vt = w_mass_ * r_mass * r_mass
                  + w_sorbed_ * r_sorbed * r_sorbed
                  + w_cold_ * r_cold * r_cold;

        bool breach = (r_mass > 1.0) || (r_cold > 1.0);

        PFASStepResult result;
        result.next_state = next;
        result.lyapunov_residual_Vt = Vt;
        result.corridor_breach = breach;
        return result;
    }

private:
    double max_mass_kg_;
    double max_cold_survival_factor_;
    double w_mass_;
    double w_sorbed_;
    double w_cold_;
};

} // namespace eco_restoration
