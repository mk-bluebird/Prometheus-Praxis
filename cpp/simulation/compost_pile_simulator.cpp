// File: cpp/simulation/compost_pile_simulator.cpp
#include <iostream>
#include <vector>
#include <string>
#include <iomanip>

namespace eco {

struct CompostLayer {
    std::string name;
    double mass_kg;
    double carbon_fraction;
    double nitrogen_fraction;
};

struct CompostState {
    double temperature_C;
    double moisture_pct;
    double c_to_n_ratio;
    double total_mass_kg;
};

class CompostPileSimulator {
public:
    CompostPileSimulator(std::vector<CompostLayer> layers, double ambient_temp_C)
        : layers_(std::move(layers)), ambient_temp_C_(ambient_temp_C) {
        update_state();
    }

    void step_day() {
        double heat_gen = 0.0;
        for (auto &layer : layers_) {
            double activity = std::min(layer.mass_kg / 50.0, 1.0);
            double temp_factor = 1.0 / (1.0 + std::exp(-(state_.temperature_C - 30.0) / 5.0));
            double moisture_factor = std::exp(-std::pow((state_.moisture_pct - 60.0) / 20.0, 2));
            double daily_loss = 0.02 * activity * temp_factor * moisture_factor * layer.mass_kg;
            layer.mass_kg -= daily_loss;
            if (layer.mass_kg < 0.0) layer.mass_kg = 0.0;
            heat_gen += daily_loss * 2.0;
        }

        state_.temperature_C = ambient_temp_C_ + heat_gen;
        state_.moisture_pct = std::clamp(state_.moisture_pct - 0.5, 30.0, 80.0);
        update_state();
    }

    CompostState state() const { return state_; }

private:
    void update_state() {
        double total_mass = 0.0;
        double total_c = 0.0;
        double total_n = 0.0;
        for (const auto &layer : layers_) {
            total_mass += layer.mass_kg;
            total_c += layer.mass_kg * layer.carbon_fraction;
            total_n += layer.mass_kg * layer.nitrogen_fraction;
        }
        state_.total_mass_kg = total_mass;
        state_.c_to_n_ratio = total_n > 0.0 ? total_c / total_n : 40.0;
        state_.moisture_pct = state_.moisture_pct == 0.0 ? 55.0 : state_.moisture_pct;
        if (state_.temperature_C == 0.0) state_.temperature_C = ambient_temp_C_;
    }

    std::vector<CompostLayer> layers_;
    double ambient_temp_C_;
    CompostState state_{};
};

} // namespace eco

int main() {
    std::vector<eco::CompostLayer> layers{
        {"Kitchen scraps", 20.0, 0.45, 0.04},
        {"Yard waste", 30.0, 0.50, 0.02},
        {"Manure", 10.0, 0.40, 0.08}
    };
    eco::CompostPileSimulator sim(layers, 22.0);
    for (int day = 0; day < 30; ++day) {
        sim.step_day();
        eco::CompostState st = sim.state();
        std::cout << "Day " << std::setw(2) << day
                  << " | Temp: " << st.temperature_C
                  << " C | Mass: " << st.total_mass_kg
                  << " kg | C:N: " << st.c_to_n_ratio << "\n";
    }
    return 0;
}
