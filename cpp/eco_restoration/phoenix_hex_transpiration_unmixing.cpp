// File: cpp/eco_restoration/phoenix_hex_transpiration_unmixing.cpp
#include <vector>
#include <string>
#include <iostream>
#include <cmath>
#include <limits>

// Inverse problem: given observed cooling effect in Phoenix hex cells after tree planting,
// recover per-species transpiration rate and shading coefficient via constrained least-squares
// spectral unmixing.
//
// Model:
//   For hex h and time t (or post-planting snapshot), observed cooling ΔT_obs(h) is modeled as:
//     ΔT_obs(h) ≈ Σ_s f_{h,s} [ a_s * T_ref(h) + b_s ]
//   where:
//     - s indexes tree species,
//     - f_{h,s} is fraction (or density) of species s in hex h,
//     - a_s is transpiration rate coefficient (K per unit reference potential, e.g. mm/day),
//     - b_s is shading coefficient (K baseline offset per canopy area).
//
// This reduces to a linear system A * x ≈ y, with:
//   - y: vector of ΔT_obs(h),
//   - x: concatenated species parameters [a_1,...,a_S, b_1,...,b_S]^T,
//   - A: design matrix built from species fractions and hex thermal references.
// We solve using constrained least squares with non-negativity constraints on a_s and b_s.

struct HexCoolingObservation {
    int32_t hex_id;
    double delta_T_obs;           // observed cooling (Kelvin)
    double thermal_ref;           // reference potential (e.g. pre-planting band-10 derived)
    std::vector<double> species_fraction; // f_{h,s} for each species s
};

struct SpeciesParameters {
    double transpiration_coeff;   // a_s ≥ 0
    double shading_coeff;         // b_s ≥ 0
};

struct UnmixingResult {
    std::vector<SpeciesParameters> params;
    double residual_norm;
};

// Solve normal equations (A^T A) x = A^T y with simple non-negativity projection.
// This is not a full quadratic programming solver, but a robust spectral-unmixing estimator
// suitable for eco-restoration diagnostics.
UnmixingResult solve_transpiration_unmixing(
    const std::vector<HexCoolingObservation>& observations,
    std::size_t species_count,
    double lambda_reg // small Tikhonov regularization
) {
    const std::size_t H = observations.size();
    const std::size_t S = species_count;
    const std::size_t P = 2 * S;  // parameters per species: a_s and b_s

    // Build normal matrix M = A^T A and rhs vector b = A^T y.
    std::vector<double> M(P * P, 0.0);
    std::vector<double> bvec(P, 0.0);

    for (std::size_t h = 0; h < H; ++h) {
        const auto& obs = observations[h];
        double y = obs.delta_T_obs;
        for (std::size_t s = 0; s < S; ++s) {
            double fhs = obs.species_fraction[s];
            double Ta = fhs * obs.thermal_ref; // contribution to a_s
            double Tb = fhs;                   // contribution to b_s

            std::size_t idx_a = s;
            std::size_t idx_b = S + s;

            // Update rhs
            bvec[idx_a] += Ta * y;
            bvec[idx_b] += Tb * y;

            // Update normal matrix
            for (std::size_t sp = 0; sp < S; ++sp) {
                double fhsp = obs.species_fraction[sp];
                double Ta_sp = fhsp * obs.thermal_ref;
                double Tb_sp = fhsp;

                std::size_t jdx_a = sp;
                std::size_t jdx_b = S + sp;

                M[idx_a * P + jdx_a] += Ta * Ta_sp;
                M[idx_a * P + jdx_b] += Ta * Tb_sp;
                M[idx_b * P + jdx_a] += Tb * Ta_sp;
                M[idx_b * P + jdx_b] += Tb * Tb_sp;
            }
        }
    }

    // Add Tikhonov regularization (lambda_reg * I).
    for (std::size_t p = 0; p < P; ++p) {
        M[p * P + p] += lambda_reg;
    }

    // Solve M x = b via simple Gauss-Seidel iterations with non-negativity projection.
    std::vector<double> x(P, 0.0);
    const std::size_t max_iter = 500;
    const double tol = 1e-6;

    for (std::size_t it = 0; it < max_iter; ++it) {
        double max_delta = 0.0;
        for (std::size_t p = 0; p < P; ++p) {
            double diag = M[p * P + p];
            if (diag <= 0.0) continue;
            double sum = 0.0;
            for (std::size_t q = 0; q < P; ++q) {
                if (q == p) continue;
                sum += M[p * P + q] * x[q];
            }
            double new_x = (bvec[p] - sum) / diag;
            if (new_x < 0.0) new_x = 0.0; // non-negativity constraint
            double delta = std::fabs(new_x - x[p]);
            if (delta > max_delta) max_delta = delta;
            x[p] = new_x;
        }
        if (max_delta < tol) break;
    }

    // Build result
    UnmixingResult result;
    result.params.resize(S);
    for (std::size_t s = 0; s < S; ++s) {
        result.params[s].transpiration_coeff = x[s];
        result.params[s].shading_coeff = x[S + s];
    }

    // Compute residual norm
    double res2 = 0.0;
    for (const auto& obs : observations) {
        double pred = 0.0;
        for (std::size_t s = 0; s < S; ++s) {
            double fhs = obs.species_fraction[s];
            double a_s = result.params[s].transpiration_coeff;
            double b_s = result.params[s].shading_coeff;
            pred += fhs * (a_s * obs.thermal_ref + b_s);
        }
        double r = pred - obs.delta_T_obs;
        res2 += r * r;
    }
    result.residual_norm = std::sqrt(res2);
    return result;
}

// Example synthetic usage with 100 hex cells and 3 species.
int main() {
    const std::size_t H = 100;
    const std::size_t S = 3;

    std::vector<HexCoolingObservation> observations;
    observations.reserve(H);

    // Synthetic ground truth for testing.
    std::vector<SpeciesParameters> true_params(S);
    true_params[0] = {0.015, 0.8};
    true_params[1] = {0.010, 0.6};
    true_params[2] = {0.020, 1.0};

    for (std::size_t h = 0; h < H; ++h) {
        HexCoolingObservation obs;
        obs.hex_id = static_cast<int32_t>(h);
        obs.thermal_ref = 315.0 + (static_cast<double>(h) * 0.05); // varying thermal load

        obs.species_fraction.resize(S);
        double sum_f = 0.0;
        for (std::size_t s = 0; s < S; ++s) {
            double base = 0.3 + 0.1 * (static_cast<double>((h + s) % 3));
            obs.species_fraction[s] = base;
            sum_f += base;
        }
        for (std::size_t s = 0; s < S; ++s) {
            obs.species_fraction[s] /= sum_f;
        }

        double delta_T_true = 0.0;
        for (std::size_t s = 0; s < S; ++s) {
            double fhs = obs.species_fraction[s];
            delta_T_true += fhs * (true_params[s].transpiration_coeff * obs.thermal_ref
                                   + true_params[s].shading_coeff);
        }

        // Add small noise
        double noise = 0.2 * std::sin(static_cast<double>(h));
        obs.delta_T_obs = delta_T_true + noise;

        observations.push_back(obs);
    }

    UnmixingResult result = solve_transpiration_unmixing(observations, S, 1e-3);

    std::cout << "Recovered species parameters:\n";
    for (std::size_t s = 0; s < S; ++s) {
        std::cout << "Species " << s
                  << ": a_s=" << result.params[s].transpiration_coeff
                  << " b_s=" << result.params[s].shading_coeff << "\n";
    }
    std::cout << "Residual norm: " << result.residual_norm << "\n";

    return 0;
}
