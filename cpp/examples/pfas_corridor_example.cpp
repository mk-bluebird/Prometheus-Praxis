// File: cpp/examples/pfas_corridor_example.cpp
#include <iostream>
#include "eco_restoration.hpp"

int main() {
    eco_pfas::PFASState state{};
    state.mass_kg = 0.002;
    state.sorbed_fraction = 0.5;
    state.cold_survival_factor = 1.0;

    double base_rate = 0.01;
    double current_temp_C = 10.0;
    double cold_temp_C = 12.0;
    double sorption_increment = 0.001;

    eco_pfas::PFASState next = eco_pfas::step_pfas_corridor(
        state, base_rate, current_temp_C, cold_temp_C, sorption_increment
    );

    std::cout << "PFAS corridor example:\n"
              << "  mass_kg=" << next.mass_kg << "\n"
              << "  sorbed_fraction=" << next.sorbed_fraction << "\n"
              << "  cold_survival_factor=" << next.cold_survival_factor << "\n";
    return 0;
}
