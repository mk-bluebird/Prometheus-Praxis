// File: cpp/simulation/albedo_sensitivity_note.cpp
#include <iostream>
#include <cmath>

/**
 * Albedo measurement accuracy and green-fraction sensitivity.
 *
 * We consider:
 * - A model linking LST drop ΔT_target (≈ 1°C) to green_fraction increase Δg,
 *   via an albedo change Δα derived from Sentinel-2 measurements.
 * - deltaVt model: ΔVt = α_v * g^β * sqrt(H), where g depends on albedo.
 *
 * We want the error in required green_fraction increase Δg to be < 10%,
 * given uncertainty in albedo measurement σα.
 *
 * Sensitivity analysis:
 *  1. Relationship between ΔT_target and Δg:
 *     Assume linearised mapping:
 *        ΔT_target ≈ k_T * Δg,
 *     so Δg = ΔT_target / k_T.
 *
 *  2. Albedo-derived green_fraction:
 *     Let albedo α be measured from Sentinel-2 with uncertainty σα.
 *     Suppose g is estimated as:
 *        g = c_0 + c_1 α,
 *     so Δg depends on Δα and measurement error δα.
 *
 *     The propagation of uncertainty:
 *        σ_g = |c_1| σα.
 *
 *     Relative error in Δg:
 *        (σ_g / Δg) = (|c_1| σα) / (ΔT_target / k_T)
 *                   = |c_1| k_T σα / ΔT_target.
 *
 *     We require:
 *        σ_g / Δg < 0.10
 *     ⇒     |c_1| k_T σα / ΔT_target < 0.10
 *     ⇒     σα < 0.10 * ΔT_target / (|c_1| k_T).
 *
 *  3. deltaVt model impact:
 *     ΔVt = α_v g^β sqrt(H).
 *
 *     The sensitivity of ΔVt to g:
 *        d(ΔVt)/dg = α_v β g^(β-1) sqrt(H).
 *
 *     An error σ_g propagates to ΔVt uncertainty:
 *        σ_ΔVt ≈ |d(ΔVt)/dg| σ_g.
 *
 *     To keep the error in Δg below 10%, we focus primarily on the Δg
 *     tolerance; ΔVt sensitivity informs how δg affects turbulence but
 *     does not change the required albedo tolerance formula above.
 *
 * Formula for albedo tolerance:
 *
 *    σα_max = 0.10 * ΔT_target / (|c_1| k_T),
 *
 * where:
 *    - ΔT_target is 1°C (≈ 1 K),
 *    - k_T is the sensitivity of LST to green_fraction (K per unit g),
 *    - c_1 is the slope mapping albedo to green_fraction.
 *
 * For example, if:
 *    k_T = 3 K per unit g,
 *    c_1 = 0.8,
 * then:
 *    σα_max = 0.10 * 1 / (0.8 * 3) ≈ 0.0417,
 * i.e., albedo measurement accuracy better than ≈ 0.04 absolute (4 percentage points)
 * ensures the required green_fraction increase estimate has < 10% relative error.
 */

int main() {
    double deltaT_target = 1.0; // K
    double k_T = 3.0;           // K per unit g (example)
    double c1 = 0.8;            // g vs albedo slope (example)

    double sigmaAlphaMax = 0.10 * deltaT_target / (std::abs(c1) * k_T);
    std::cout << "Max allowable albedo measurement uncertainty (absolute): "
              << sigmaAlphaMax << std::endl;
    return 0;
}
