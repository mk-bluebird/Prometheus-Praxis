// File: cpp/eco_restoration/hex_oaxaca_blinder_decomposition.cpp

#include <vector>
#include <string>
#include <cmath>
#include <iostream>

/**
 * Bi-temporal hex calibration and Oaxaca–Blinder-style decomposition
 * for change attribution in UHI.
 *
 * We consider two Landsat epochs: t0 (e.g., 2015) and t1 (e.g., 2022).
 *
 * For each hex h:
 *   ΔT_h(t0) = α(t0) V_h(t0) + β(t0) B_h(t0) + γ(t0) W_h(t0) + δ(t0)
 *   ΔT_h(t1) = α(t1) V_h(t1) + β(t1) B_h(t1) + γ(t1) W_h(t1) + δ(t1)
 *
 * Define:
 *   θ(t) = [α(t), β(t), γ(t), δ(t)]^T
 *   X_h(t) = [V_h(t), B_h(t), W_h(t), 1]^T
 *
 * Oaxaca–Blinder decomposition (scalar ΔT per hex):
 *
 *   ΔT_h(t1) - ΔT_h(t0)
 *   = [θ(t0) · X_h(t1) - θ(t0) · X_h(t0)]      (composition / land-cover effect)
 *   + [θ(t1) · X_h(t1) - θ(t0) · X_h(t1)]      (coefficient / sensitivity effect)
 *
 *   = C_h + S_h
 *
 * where:
 *   C_h = θ(t0) · (X_h(t1) - X_h(t0))   (change in land-cover fractions)
 *   S_h = (θ(t1) - θ(t0)) · X_h(t1)     (change in sensitivity parameters).
 *
 * We implement per-hex decomposition and aggregate statistics across the grid.
 */

struct HexEpoch {
    std::string hex_id;
    double V;
    double B;
    double W;
};

struct CoeffEpoch {
    double alpha;
    double beta;
    double gamma;
    double delta;
};

struct HexDecomposition {
    std::string hex_id;
    double delta_T_t0;
    double delta_T_t1;
    double composition_effect; // C_h
    double sensitivity_effect; // S_h
    double total_change;       // ΔT_h(t1) - ΔT_h(t0)
};

double compute_delta_T(const CoeffEpoch& coeffs, const HexEpoch& epoch) {
    return coeffs.alpha * epoch.V
         + coeffs.beta  * epoch.B
         + coeffs.gamma * epoch.W
         + coeffs.delta;
}

HexDecomposition decompose_hex_change(const std::string& hex_id,
                                      const HexEpoch& h_t0,
                                      const HexEpoch& h_t1,
                                      const CoeffEpoch& coeffs_t0,
                                      const CoeffEpoch& coeffs_t1) {
    double delta_T_t0 = compute_delta_T(coeffs_t0, h_t0);
    double delta_T_t1 = compute_delta_T(coeffs_t1, h_t1);

    // Composition effect C_h = θ(t0) · (X_h(t1) - X_h(t0)).
    double dV = h_t1.V - h_t0.V;
    double dB = h_t1.B - h_t0.B;
    double dW = h_t1.W - h_t0.W;
    double composition_effect =
        coeffs_t0.alpha * dV +
        coeffs_t0.beta  * dB +
        coeffs_t0.gamma * dW; // δ cancels in difference since 1 - 1 = 0.

    // Sensitivity effect S_h = (θ(t1) - θ(t0)) · X_h(t1).
    double d_alpha = coeffs_t1.alpha - coeffs_t0.alpha;
    double d_beta  = coeffs_t1.beta  - coeffs_t0.beta;
    double d_gamma = coeffs_t1.gamma - coeffs_t0.gamma;
    double d_delta = coeffs_t1.delta - coeffs_t0.delta;
    double sensitivity_effect =
        d_alpha * h_t1.V +
        d_beta  * h_t1.B +
        d_gamma * h_t1.W +
        d_delta * 1.0;

    double total_change = delta_T_t1 - delta_T_t0;

    HexDecomposition dec;
    dec.hex_id = hex_id;
    dec.delta_T_t0 = delta_T_t0;
    dec.delta_T_t1 = delta_T_t1;
    dec.composition_effect = composition_effect;
    dec.sensitivity_effect = sensitivity_effect;
    dec.total_change = total_change;
    return dec;
}

int main() {
    // Example hex with synthetic epochs.
    HexEpoch h2015{"hex_10_20", 0.25, 0.5, 0.05};
    HexEpoch h2022{"hex_10_20", 0.30, 0.55, 0.08};

    CoeffEpoch coeffs2015{-8.0, 3.0, -5.0, 0.5};
    CoeffEpoch coeffs2022{-7.5, 3.2, -4.8, 0.7};

    auto dec = decompose_hex_change("hex_10_20", h2015, h2022, coeffs2015, coeffs2022);

    std::cout << "Hex " << dec.hex_id << " Oaxaca–Blinder decomposition:\n"
              << "  ΔT_2015 = " << dec.delta_T_t0 << "\n"
              << "  ΔT_2022 = " << dec.delta_T_t1 << "\n"
              << "  Composition effect C_h = " << dec.composition_effect << "\n"
              << "  Sensitivity effect S_h = " << dec.sensitivity_effect << "\n"
              << "  Total change ΔT = " << dec.total_change << "\n";

    return 0;
}
