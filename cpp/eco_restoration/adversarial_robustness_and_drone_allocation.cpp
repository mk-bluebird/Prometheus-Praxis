// File: cpp/eco_restoration/adversarial_robustness_and_drone_allocation.cpp
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <iomanip>

namespace praxis {
namespace eco {

double clamp01(double x) {
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

// ----------------------------------------------------------
// 33. Adversarial robustness of psych-risk classifiers
// ----------------------------------------------------------
//
// We consider a psych-risk classifier f(snr, drift, features...) that outputs
// a risk score r ∈ [0,1]. An attacker can perturb the electrode signals but must
// keep snr_db > 12 dB and drift_pct_per_hr < 2%/hr to avoid invalidating the
// reliability_token.
//
// We approximate the classifier locally as a Lipschitz function in the sensor
// feature space. The maximum psych-risk perturbation is bounded by:
//   |Δr| <= L * ||Δx||,
// where x represents sensor features and Δx is constrained by reliability thresholds.
//
// Certified robustness: choose classifier and preprocessing such that L is small
// and enforce margin between bands so that adversarial perturbations within
// allowed Δx cannot cross critical thresholds.

struct SensorFeatures {
    double snr_db;
    double drift_pct_per_hr;
    double eeg_power;
    double fnirs_oxy;
    double gsr;
};

struct ClassifierParams {
    double w_snr;
    double w_drift;
    double w_eeg;
    double w_fnirs;
    double w_gsr;
};

double psych_risk_classifier(const SensorFeatures& s,
                             const ClassifierParams& w) {
    // Simple linear classifier followed by clamp.
    double r =
        w.w_snr   * (1.0 - clamp01((s.snr_db - 12.0) / 10.0)) + // higher snr lowers risk
        w.w_drift * clamp01(s.drift_pct_per_hr / 2.0) +
        w.w_eeg   * s.eeg_power +
        w.w_fnirs * s.fnirs_oxy +
        w.w_gsr   * s.gsr;
    return clamp01(r);
}

// Compute a local Lipschitz bound L based on weights (for L2 norm).
double local_lipschitz_bound(const ClassifierParams& w) {
    double L2 =
        w.w_snr   * w.w_snr +
        w.w_drift * w.w_drift +
        w.w_eeg   * w.w_eeg +
        w.w_fnirs * w.w_fnirs +
        w.w_gsr   * w.w_gsr;
    return std::sqrt(L2);
}

// Maximum adversarial perturbation Δr given constraints on Δx:
// For simplicity, assume attacker can vary snr_db in [12, snr_max]
// and drift_pct_per_hr in [0, 2], plus bounded changes in eeg/fnirs/gsr
// that do not trigger token revocation. We simulate worst-case Δx magnitude.
double max_psych_risk_perturbation(const SensorFeatures& base,
                                   const ClassifierParams& w,
                                   double snr_max,
                                   double drift_max,
                                   double feature_delta_bound) {
    double L = local_lipschitz_bound(w);

    // Construct a worst-case Δx within allowed thresholds.
    SensorFeatures adv = base;
    adv.snr_db = snr_max;        // try lowering risk via snr or raising it via EEG/GSR
    adv.drift_pct_per_hr = drift_max;
    adv.eeg_power = clamp01(base.eeg_power + feature_delta_bound);
    adv.fnirs_oxy = clamp01(base.fnirs_oxy + feature_delta_bound);
    adv.gsr = clamp01(base.gsr + feature_delta_bound);

    // Norm of Δx (simple Euclidean over normalized features).
    double dx_snr   = (adv.snr_db - base.snr_db) / 10.0;
    double dx_drift = (adv.drift_pct_per_hr - base.drift_pct_per_hr) / 2.0;
    double dx_eeg   = adv.eeg_power  - base.eeg_power;
    double dx_fnirs = adv.fnirs_oxy  - base.fnirs_oxy;
    double dx_gsr   = adv.gsr        - base.gsr;

    double norm_dx = std::sqrt(dx_snr*dx_snr + dx_drift*dx_drift +
                               dx_eeg*dx_eeg + dx_fnirs*dx_fnirs +
                               dx_gsr*dx_gsr);

    double delta_r_bound = L * norm_dx;
    return delta_r_bound;
}

// Certified robustness hardening:
//   - Choose classifier weights and feature normalization to minimize L.
//   - Introduce margins between band thresholds:
//       NORMAL: r < 0.4
//       MODERATE: 0.4 <= r < 0.7
//       HIGH: r >= 0.7
//   - Guarantee that delta_r_bound < margin to prevent adversary from crossing band
//     boundaries within allowed sensor perturbations.

// ----------------------------------------------------------
// 34. Resource allocation for eco-restoration drones (LP model)
// ----------------------------------------------------------
//
// We have:
//   - Hex-cells i = 1..n with RoH_i baseline.
//   - Drones j = 1..m, each with limited minutes M_j.
//   - Cooling effect per drone-minute on cell i: c_ij (reduction in RoH_i).
//
// Objective: allocate drone minutes x_ij >= 0 to maximize total corridor cooling:
//   Maximize   Σ_i max(0, RoH_i - 0.30) - Σ_i max(0, RoH_i(x) - 0.30)
//
// Equivalently, minimize Σ_i max(0, RoH_i(x) - 0.30).
//
// Linear approximation:
//   RoH_i(x) ≈ RoH_i - Σ_j c_ij x_ij.
//
// Then objective:
//   Minimize   Σ_i (RoH_i - Σ_j c_ij x_ij - 0.30)_+
//
// We introduce auxiliary variables y_i ≥ 0 to represent residual excess above 0.30:
//
//   y_i ≥ RoH_i - Σ_j c_ij x_ij - 0.30
//   y_i ≥ 0
//
// Minimize Σ_i y_i
//
// Subject to:
//   Σ_i x_ij ≤ M_j   for each drone j
//   x_ij ≥ 0

struct DroneAllocationLP {
    int n_cells;
    int m_drones;
    std::vector<double> RoH;
    std::vector<std::vector<double>> cooling; // c_ij
    std::vector<double> M;                    // drone minutes
};

// Simple distributed heuristic solution:
//   - Each drone locally solves a linear allocation over its neighborhood
//     using gradient descent or projected subgradient to reduce y_i.
//   - Coordination via dual decomposition: introduce prices for y_i and
//     let drones adjust x_ij based on local RoH and prices.
//
// Here we implement a simple centralized gradient-like heuristic as a stand-in.

struct DroneAllocationResult {
    std::vector<std::vector<double>> x; // minutes allocated
    std::vector<double> y;             // residual excess RoH above 0.30
};

DroneAllocationResult solve_allocation_heuristic(const DroneAllocationLP& lp,
                                                 int iterations,
                                                 double step) {
    int n = lp.n_cells;
    int m = lp.m_drones;

    std::vector<std::vector<double>> x(m, std::vector<double>(n, 0.0));
    std::vector<double> y(n, 0.0);

    for (int it = 0; it < iterations; ++it) {
        // Update y_i based on current x.
        for (int i = 0; i < n; ++i) {
            double cooling_sum = 0.0;
            for (int j = 0; j < m; ++j) {
                cooling_sum += lp.cooling[j][i] * x[j][i];
            }
            double excess = lp.RoH[i] - cooling_sum - 0.30;
            y[i] = excess > 0.0 ? excess : 0.0;
        }

        // Gradient step: drones allocate more minutes to cells with largest y_i,
        // subject to per-drone budget M_j.
        for (int j = 0; j < m; ++j) {
            double used = 0.0;
            for (int i = 0; i < n; ++i) {
                used += x[j][i];
            }

            for (int i = 0; i < n; ++i) {
                if (used >= lp.M[j]) break;
                if (y[i] > 0.0 && lp.cooling[j][i] > 0.0) {
                    double increment = step * y[i];
                    double remaining = lp.M[j] - used;
                    if (increment > remaining) increment = remaining;
                    x[j][i] += increment;
                    used += increment;
                }
            }
        }
    }

    // Final y_i recomputation.
    for (int i = 0; i < n; ++i) {
        double cooling_sum = 0.0;
        for (int j = 0; j < m; ++j) {
            cooling_sum += lp.cooling[j][i] * x[j][i];
        }
        double excess = lp.RoH[i] - cooling_sum - 0.30;
        y[i] = excess > 0.0 ? excess : 0.0;
    }

    return DroneAllocationResult{x, y};
}

// ----------------------------------------------------------
// Demonstration main
// ----------------------------------------------------------

int main() {
    std::cout << std::fixed << std::setprecision(4);

    // 33. Adversarial robustness demo.
    SensorFeatures base{
        13.0,  // snr_db (above threshold)
        1.0,   // drift_pct_per_hr (below threshold)
        0.5,   // eeg_power
        0.4,   // fnirs_oxy
        0.3    // gsr
    };

    ClassifierParams w{
        0.2,   // w_snr
        0.3,   // w_drift
        0.4,   // w_eeg
        0.3,   // w_fnirs
        0.3    // w_gsr
    };

    double r_base = psych_risk_classifier(base, w);
    double snr_max = 20.0;
    double drift_max = 1.9;
    double feature_delta_bound = 0.1;

    double delta_r_bound = max_psych_risk_perturbation(base, w,
                                                       snr_max, drift_max,
                                                       feature_delta_bound);

    std::cout << "Adversarial robustness of psych-risk classifier:\n";
    std::cout << "  Baseline risk r=" << r_base << "\n";
    std::cout << "  Max |Δr| within reliability-token thresholds ≲ " << delta_r_bound << "\n";
    std::cout << "  Classifier can be hardened by ensuring this bound is smaller than "
                 "the margin between risk bands, so adversarial perturbations cannot "
                 "cross continuity-trigger thresholds.\n\n";

    // 34. Drone allocation LP demo.
    DroneAllocationLP lp{
        4,  // n_cells
        2,  // m_drones
        {0.35, 0.40, 0.32, 0.28},              // RoH_i
        {
            {0.02, 0.015, 0.010, 0.005},       // cooling by drone 0
            {0.015, 0.020, 0.012, 0.006}       // cooling by drone 1
        },
        {60.0, 60.0}                           // 60 minutes per drone
    };

    DroneAllocationResult res = solve_allocation_heuristic(lp, 50, 0.1);

    std::cout << "Resource allocation for eco-restoration drones (heuristic LP):\n";
    for (int j = 0; j < lp.m_drones; ++j) {
        double used = 0.0;
        std::cout << "  Drone " << j << " allocation:\n";
        for (int i = 0; i < lp.n_cells; ++i) {
            std::cout << "    Cell " << i << ": x_ij=" << res.x[j][i] << " minutes\n";
            used += res.x[j][i];
        }
        std::cout << "    Total minutes used: " << used << " (budget " << lp.M[j] << ")\n";
    }

    std::cout << "  Residual excess RoH above 0.30:\n";
    for (int i = 0; i < lp.n_cells; ++i) {
        std::cout << "    Cell " << i << ": residual y_i=" << res.y[i] << "\n";
    }

    std::cout << "  Distributed solution in practice would use dual decomposition or "
                 "consensus-based optimization, with each drone solving its local "
                 "allocation and exchanging prices or gradients over a sparse corridor graph.\n";

    return 0;
}

} // namespace eco
} // namespace praxis
