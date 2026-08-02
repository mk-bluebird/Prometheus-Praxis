// File: cpp/simulation/compost_pile_simulator.cpp
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <limits>

namespace eco {

struct CompostLayer {
    std::string name;
    double mass_kg;
    double carbon_fraction;
    double nitrogen_fraction;
};

struct CompostMacroState {
    double temperature_C;
    double moisture_pct;
    double c_to_n_ratio;
    double total_mass_kg;
};

struct CompostMicroState {
    double T;  // temperature (C)
    double M;  // moisture (fraction 0..1)
    double N;  // microbial population (normalized)
};

class CompostPileSimulator {
public:
    CompostPileSimulator(const std::vector<CompostLayer>& layers,
                         double ambient_temp_C,
                         double a1, double b1, double c1,
                         double a2, double b2, double c2,
                         double a3, double b3,
                         double N_max)
        : layers_(layers),
          ambient_temp_C_(ambient_temp_C),
          T_ambient_(ambient_temp_C),
          a1_(a1), b1_(b1), c1_(c1),
          a2_(a2), b2_(b2), c2_(c2),
          a3_(a3), b3_(b3),
          N_max_(N_max) {
        macro_state_ = {};
        micro_state_ = {ambient_temp_C_, 0.55, 0.5};
        update_macro_state();
    }

    double derive_max_safe_aeration_frequency(double lambda_N_linear,
                                              double k_aer_gain) const {
        double trace_J = -b1_ + lambda_N_linear;
        if (k_aer_gain <= 0.0) {
            return 2.0;
        }
        double f_max = -trace_J / k_aer_gain;
        if (f_max < 0.0) {
            return 3.0;
        }
        if (f_max > 4.0) {
            f_max = 4.0;
        }
        return f_max;
    }

    void simulate_micro(double days,
                        double dt_hours,
                        double f_aer_per_day,
                        CompostMicroState init) {
        micro_state_ = init;
        double t_hours = 0.0;
        double total_steps = days * 24.0 / dt_hours;
        double aeration_interval_hours = (f_aer_per_day > 0.0)
            ? (24.0 / f_aer_per_day)
            : std::numeric_limits<double>::infinity();
        double next_aeration = aeration_interval_hours;

        std::cout << std::fixed << std::setprecision(3);
        std::cout << "Micro-scale simulation: " << days
                  << " days, aeration " << f_aer_per_day << " /day\n";

        for (int step = 0; step < static_cast<int>(total_steps); ++step) {
            bool aeration_event = (t_hours >= next_aeration - 1e-6);
            double f_aeration = aeration_event ? 1.0 : 0.0;

            double dTdt = a1_ * micro_state_.N
                          - b1_ * (micro_state_.T - T_ambient_)
                          - c1_ * f_aeration;
            double dMdt = -a2_ * micro_state_.N
                          - b2_ * (micro_state_.M - 0.5)
                          + c2_ * f_aeration;
            double stress = std::max(0.0, std::fabs(micro_state_.T - 55.0) / 20.0);
            double dNdt = a3_ * micro_state_.N * (1.0 - micro_state_.N / N_max_)
                          - b3_ * stress * micro_state_.N;

            micro_state_.T += dTdt * (dt_hours / 24.0);
            micro_state_.M += dMdt * (dt_hours / 24.0);
            micro_state_.N += dNdt * (dt_hours / 24.0);

            micro_state_.M = std::clamp(micro_state_.M, 0.0, 1.0);
            micro_state_.N = std::max(0.0, micro_state_.N);

            if (aeration_event) {
                next_aeration += aeration_interval_hours;
            }

            if (step % static_cast<int>(12.0 / dt_hours) == 0) {
                std::cout << "t=" << t_hours << " h, T=" << micro_state_.T
                          << " C, M=" << micro_state_.M
                          << ", N=" << micro_state_.N << "\n";
            }

            t_hours += dt_hours;
        }
    }

    void step_macro_day() {
        double heat_gen = 0.0;
        for (auto& layer : layers_) {
            double activity = std::min(layer.mass_kg / 50.0, 1.0);
            double temp_factor = 1.0 / (1.0 + std::exp(-(macro_state_.temperature_C - 30.0) / 5.0));
            double moisture_factor = std::exp(-std::pow((macro_state_.moisture_pct - 60.0) / 20.0, 2));
            double daily_loss = 0.02 * activity * temp_factor * moisture_factor * layer.mass_kg;
            layer.mass_kg -= daily_loss;
            if (layer.mass_kg < 0.0) {
                layer.mass_kg = 0.0;
            }
            heat_gen += daily_loss * 2.0;
        }

        macro_state_.temperature_C = ambient_temp_C_ + heat_gen;
        macro_state_.moisture_pct = std::clamp(macro_state_.moisture_pct - 0.5, 30.0, 80.0);
        update_macro_state();
    }

    CompostMacroState macro_state() const {
        return macro_state_;
    }

    CompostMicroState micro_state() const {
        return micro_state_;
    }

private:
    void update_macro_state() {
        double total_mass = 0.0;
        double total_c = 0.0;
        double total_n = 0.0;
        for (const auto& layer : layers_) {
            total_mass += layer.mass_kg;
            total_c += layer.mass_kg * layer.carbon_fraction;
            total_n += layer.mass_kg * layer.nitrogen_fraction;
        }
        macro_state_.total_mass_kg = total_mass;
        macro_state_.c_to_n_ratio = total_n > 0.0 ? total_c / total_n : 40.0;
        if (macro_state_.moisture_pct == 0.0) {
            macro_state_.moisture_pct = 55.0;
        }
        if (macro_state_.temperature_C == 0.0) {
            macro_state_.temperature_C = ambient_temp_C_;
        }
    }

    std::vector<CompostLayer> layers_;
    double ambient_temp_C_;

    double T_ambient_;
    double a1_, b1_, c1_;
    double a2_, b2_, c2_;
    double a3_, b3_;
    double N_max_;

    CompostMacroState macro_state_;
    CompostMicroState micro_state_;
};

} // namespace eco

int main() {
    using namespace eco;

    std::vector<CompostLayer> layers{
        {"Kitchen scraps", 20.0, 0.45, 0.04},
        {"Yard waste",     30.0, 0.50, 0.02},
        {"Manure",         10.0, 0.40, 0.08}
    };

    CompostPileSimulator sim(
        layers,
        32.0,
        0.8,
        0.4,
        3.0,
        0.3,
        0.2,
        1.5,
        0.9,
        0.3,
        1.0
    );

    double lambda_N_linear = -0.2;
    double k_aer_gain = 0.5;
    double f_max = sim.derive_max_safe_aeration_frequency(lambda_N_linear, k_aer_gain);

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "Maximum safe aeration frequency: " << f_max << " events per day\n";

    eco::CompostMicroState init_micro{45.0, 0.6, 0.5};
    sim.simulate_micro(5.0, 6.0, f_max, init_micro);

    std::cout << "\nMacro-scale daily evolution:\n";
    for (int day = 0; day < 30; ++day) {
        sim.step_macro_day();
        eco::CompostMacroState st = sim.macro_state();
        std::cout << "Day " << std::setw(2) << day
                  << " | Temp: " << st.temperature_C
                  << " C | Mass: " << st.total_mass_kg
                  << " kg | C:N: " << st.c_to_n_ratio << "\n";
    }

    return 0;
}
