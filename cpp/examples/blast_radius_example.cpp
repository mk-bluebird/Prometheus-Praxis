// File: cpp/examples/blast_radius_example.cpp
#include <iostream>
#include "eco_restoration.hpp"

int main() {
    phoenix_canal::BlastRisk risk = phoenix_canal::run_blast_radius_step();

    std::cout << "Blast-radius example:\n"
              << "  r_hydraulics=" << risk.r_hydraulics << "\n"
              << "  r_energy=" << risk.r_energy << "\n"
              << "  r_topology=" << risk.r_topology << "\n";
    return 0;
}
