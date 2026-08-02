// File: cpp/tools/hex_ubem_coupling_fmi.cpp

#include <string>
#include <vector>
#include <iostream>

/**
 * Wiring pattern: integration of hex-level ΔT predictions with
 * Urban Building Energy Model (UBEM) via co-simulation (FMI).
 *
 * Hex-level offset model (Rust crate):
 *   ΔT_h = α V_h + β B_h + γ W_h + δ
 *
 * The Rust crate exposes per-hex ΔT_h and UHI_h at given times.
 * UBEM (e.g., CitySim, EnergyPlus-based UBEM) simulates building
 * energy loads given outdoor conditions and building properties.[65][171][165]
 *
 * Coupling mechanism:
 *  - Use Functional Mock-up Interface (FMI) for co-simulation; export UBEM
 *    as an FMU (Functional Mock-up Unit).
 *  - The Rust crate acts as a master (or client to a master) providing
 *    hex-level ΔT_h as external disturbance to the UBEM FMU.
 *
 * Data exchange format:
 *  - Hex-level “outdoor condition” payload per time step:
 *      struct HexBoundaryCondition {
 *          std::string hex_id;
 *          double delta_T;     // ΔT_h from offset model
 *          double base_T;      // rural reference T
 *          double T_outdoor;   // base_T + ΔT_h
 *      };
 *
 *  - UBEM expects per-building outdoor temperature; we map buildings
 *    to hexes using a building-to-hex index, and set T_outdoor at
 *    each building node in the FMU at each coupling step.
 */

struct HexBoundaryCondition {
    std::string hex_id;
    double delta_T;
    double base_T;
    double T_outdoor() const { return base_T + delta_T; }
};

struct BuildingToHexMap {
    std::string building_id;
    std::string hex_id;
};

struct UbemInput {
    std::string building_id;
    double T_outdoor;
};

class HexUbemCoupler {
public:
    HexUbemCoupler(std::vector<BuildingToHexMap> mapping)
        : mapping_(std::move(mapping)) {}

    // Convert hex boundary conditions to UBEM inputs.
    std::vector<UbemInput> generate_ubem_inputs(
            const std::vector<HexBoundaryCondition>& hex_bc) const {
        std::vector<UbemInput> inputs;
        inputs.reserve(mapping_.size());

        for (const auto& m : mapping_) {
            const HexBoundaryCondition* bc = find_bc(hex_bc, m.hex_id);
            if (!bc) continue;
            UbemInput in;
            in.building_id = m.building_id;
            in.T_outdoor   = bc->T_outdoor();
            inputs.push_back(in);
        }

        return inputs;
    }

private:
    std::vector<BuildingToHexMap> mapping_;

    const HexBoundaryCondition* find_bc(
            const std::vector<HexBoundaryCondition>& bc_vec,
            const std::string& hex_id) const {
        for (const auto& bc : bc_vec) {
            if (bc.hex_id == hex_id) {
                return &bc;
            }
        }
        return nullptr;
    }
};

int main() {
    // Example mapping of buildings to hexes.
    std::vector<BuildingToHexMap> mapping = {
        {"bldg_001", "hex_10_20"},
        {"bldg_002", "hex_10_20"},
        {"bldg_003", "hex_11_20"}
    };

    HexUbemCoupler coupler(mapping);

    // Hex boundary conditions at a given time step from Rust offset crate.
    std::vector<HexBoundaryCondition> hex_bc = {
        {"hex_10_20", 6.0, 35.0},
        {"hex_11_20", 5.0, 35.0}
    };

    auto ubem_inputs = coupler.generate_ubem_inputs(hex_bc);

    std::cout << "UBEM coupling inputs:\n";
    for (const auto& in : ubem_inputs) {
        std::cout << "Building " << in.building_id
                  << " | T_outdoor=" << in.T_outdoor << " °C\n";
    }

    return 0;
}
