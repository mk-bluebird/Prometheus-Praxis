// File: cpp/eco_restoration/pinn_evapotranspiration_loss.cpp

#include <vector>
#include <string>
#include <cmath>
#include <iostream>

/**
 * Physics-informed neural network (PINN) loss for hex-level ET modeling.
 *
 * We model evapotranspiration (ET_h) per hex h as a function of Landsat-8
 * indices and ancillary drivers:
 *   inputs: NDVI_h, NDWI_h, albedo_h, LST_h, etc.
 *   output: ET̂_h = f_θ(inputs)
 *
 * Calibrated α captures part of the cooling due to ET via:
 *   ΔT_h ≈ α V_h + ...
 * with V_h ~ NDVI or canopy proxy. Higher ET should correlate with stronger
 * vegetation cooling (more negative α contribution).[65][166][165]
 *
 * We train a PINN using flux tower observations ET_obs,h and the α-based
 * cooling relationship as a soft constraint.
 *
 * Observed data term:
 *   L_data = Σ_h (ET̂_h - ET_obs,h)^2
 *
 * Physics (α-based) constraint term:
 *   We approximate ΔT_ET,h, the portion of ΔT attributable to ET, from α:
 *     ΔT_ET,h ≈ α * V_h (using vegetation index)
 *   and link it to ET via a simple energy balance surrogate:
 *     ΔT_ET,h ≈ -k_ET * ET̂_h
 *   i.e., more ET implies more cooling.[148][152][155]
 *
 *   L_phys = Σ_h (ΔT_ET,h + k_ET * ET̂_h)^2
 *
 * Structural consistency with neighboring ET (optional):
 *   L_struct = Σ_h (ET̂_h - mean_neighbors(ET̂))^2
 *
 * Full PINN loss:
 *   L_total = L_data + λ_phys L_phys + λ_struct L_struct
 */

struct HexETSample {
    double ndvi;
    double ndwi;
    double lst;
    double et_obs;    // observed ET (e.g., mm/day) from flux tower upscaling
    double alpha;     // calibrated vegetation cooling coefficient for this epoch
    double V;         // vegetation index used with α (e.g., NDVI or canopy fraction)
    double et_pred;   // predicted ET̂_h from PINN (placeholder for coupling)
};

struct NeighborInfo {
    std::vector<int> neighbor_indices; // indices in the sample array
};

struct PinnLossTerms {
    double L_data;
    double L_phys;
    double L_struct;
    double L_total;
};

double compute_neighbor_mean_et(const std::vector<HexETSample>& samples,
                                const NeighborInfo& neigh) {
    if (neigh.neighbor_indices.empty()) {
        return 0.0;
    }
    double sum = 0.0;
    int count = 0;
    for (int idx : neigh.neighbor_indices) {
        if (idx < 0 || idx >= static_cast<int>(samples.size())) continue;
        sum += samples[idx].et_pred;
        ++count;
    }
    return (count > 0) ? sum / static_cast<double>(count) : 0.0;
}

PinnLossTerms compute_pinn_loss(const std::vector<HexETSample>& samples,
                                const std::vector<NeighborInfo>& neighbors,
                                double k_ET,
                                double lambda_phys,
                                double lambda_struct) {
    double L_data = 0.0;
    double L_phys = 0.0;
    double L_struct = 0.0;

    std::size_t n = samples.size();
    for (std::size_t i = 0; i < n; ++i) {
        const auto& s = samples[i];
        double et_hat = s.et_pred;

        // Data mismatch term.
        double d = et_hat - s.et_obs;
        L_data += d * d;

        // Physics term via α-based cooling.
        double delta_T_ET = s.alpha * s.V; // portion of ΔT due to vegetation/ET
        double phys_residual = delta_T_ET + k_ET * et_hat;
        L_phys += phys_residual * phys_residual;

        // Structural neighbor term.
        double neigh_mean = compute_neighbor_mean_et(samples, neighbors[i]);
        double struct_residual = et_hat - neigh_mean;
        L_struct += struct_residual * struct_residual;
    }

    double L_total = L_data + lambda_phys * L_phys + lambda_struct * L_struct;
    return {L_data, L_phys, L_struct, L_total};
}

int main() {
    // Synthetic PINN loss example for 3 hexes.
    std::vector<HexETSample> samples = {
        {0.35, 0.10, 42.0, 4.2, -8.0, 0.35, 4.0},
        {0.40, 0.12, 41.0, 4.5, -7.5, 0.40, 4.3},
        {0.25, 0.08, 43.0, 3.8, -8.2, 0.25, 3.9}
    };
    std::vector<NeighborInfo> neighbors = {
        {{1}},      // hex0 neighbor hex1
        {{0, 2}},   // hex1 neighbors hex0, hex2
        {{1}}       // hex2 neighbor hex1
    };

    double k_ET = 0.5;         // coupling factor between ET and ΔT_ET
    double lambda_phys = 2.0;  // weight on physics constraint
    double lambda_struct = 0.5;

    PinnLossTerms loss = compute_pinn_loss(samples, neighbors,
                                           k_ET, lambda_phys, lambda_struct);

    std::cout << "PINN ET loss terms:\n"
              << "  L_data   = " << loss.L_data << "\n"
              << "  L_phys   = " << loss.L_phys << "\n"
              << "  L_struct = " << loss.L_struct << "\n"
              << "  L_total  = " << loss.L_total << "\n";

    return 0;
}
