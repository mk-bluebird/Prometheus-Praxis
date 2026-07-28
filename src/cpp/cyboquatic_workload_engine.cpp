// filename: src/cpp/cyboquatic_workload_engine.cpp
// license: MIT OR Apache-2.0
// role: Non-actuating cyboquatic workload kernel for normalized risk coordinates and Lyapunov residuals.
// note: This file is pure numeric; it performs no IO, no device access, and no actuation.

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace cyboquatic_workload {

struct WorkloadInput {
    // Identity
    const char* node_id;        // Logical canal node identifier (not dereferenced here).
    double      window_start_s; // Window start time (seconds since epoch).
    double      window_end_s;   // Window end time (seconds since epoch).

    // Raw workload telemetry
    double energy_req_j;        // Required energy for the window (Joules).
    double throughput_m3;       // Volume throughput over the window (m^3).
    double head_m;              // Effective hydraulic head (m).
    double duty_cycle;          // Fraction of time active in [0, 1].
    double uncertainty_factor;  // Normalized uncertainty hint in [0, 1].
};

struct WorkloadConfig {
    // Normalization bands (domain-specific but fixed per deployment).
    double min_energy_j;
    double max_energy_j;

    double min_throughput_m3;
    double max_throughput_m3;

    double min_head_m;
    double max_head_m;

    // Lyapunov weights for risk planes.
    double w_energy;
    double w_hydraulics;
    double w_uncertainty;

    // Reference Lyapunov value and maximum allowed increase.
    double v_ref;
    double max_delta_v;
};

struct WorkloadOutput {
    // Normalized risk coordinates in [0, 1].
    double r_energy;
    double r_hydraulics;
    double r_uncertainty;

    // Lyapunov values and residual.
    double v_t;      // Current Lyapunov value.
    double v_next;   // Hypothetical next Lyapunov value under the same conditions.
    double delta_v;  // v_next - v_t.

    // Simple KER-style score for convenience (k = 1 - r_energy, e = 1 - r_hydraulics, r = r_uncertainty).
    double k_knowledge;
    double e_ecoimpact;
    double r_risk;
    double ker_score;

    // Flags for governance layers (diagnostic only, no actuation).
    bool   lyapunov_ok;   // true if delta_v <= max_delta_v.
    bool   window_valid;  // true if window_end_s > window_start_s.
};

static double clamp01(double x) {
    if (x < 0.0) {
        return 0.0;
    }
    if (x > 1.0) {
        return 1.0;
    }
    return x;
}

static double normalize_band(double value, double v_min, double v_max) {
    if (v_max <= v_min) {
        return 0.0;
    }
    const double scaled = (value - v_min) / (v_max - v_min);
    return clamp01(scaled);
}

static double quadratic_lyapunov(
    double r_energy,
    double r_hydraulics,
    double r_uncertainty,
    double w_energy,
    double w_hydraulics,
    double w_uncertainty
) {
    const double term_energy      = w_energy      * r_energy      * r_energy;
    const double term_hydraulics  = w_hydraulics  * r_hydraulics  * r_hydraulics;
    const double term_uncertainty = w_uncertainty * r_uncertainty * r_uncertainty;
    return term_energy + term_hydraulics + term_uncertainty;
}

// Core numeric kernel: compute normalized risk coordinates and Lyapunov residual for a workload window.
// Returns 0 on success, non-zero on invalid input.
int compute_workload_window(
    const WorkloadInput*  input,
    const WorkloadConfig* config,
    WorkloadOutput*       output
) {
    if (input == nullptr || config == nullptr || output == nullptr) {
        return 1;
    }

    // Basic window sanity check.
    const bool window_valid = (input->window_end_s > input->window_start_s);
    output->window_valid = window_valid;

    // Normalize risk planes.
    const double r_energy = normalize_band(
        input->energy_req_j,
        config->min_energy_j,
        config->max_energy_j
    );

    const double r_hydraulics = normalize_band(
        input->head_m,
        config->min_head_m,
        config->max_head_m
    );

    // Uncertainty is already in [0, 1] by contract but we clamp defensively.
    const double r_uncertainty = clamp01(input->uncertainty_factor);

    output->r_energy      = r_energy;
    output->r_hydraulics  = r_hydraulics;
    output->r_uncertainty = r_uncertainty;

    // Lyapunov value for current window.
    const double v_t = quadratic_lyapunov(
        r_energy,
        r_hydraulics,
        r_uncertainty,
        config->w_energy,
        config->w_hydraulics,
        config->w_uncertainty
    );

    // For a simple diagnostic kernel, we treat v_next as v_t drifted toward v_ref based on duty_cycle.
    const double duty = clamp01(input->duty_cycle);
    const double v_next = v_t + duty * (config->v_ref - v_t);
    const double delta_v = v_next - v_t;

    output->v_t     = v_t;
    output->v_next  = v_next;
    output->delta_v = delta_v;

    // Simple KER interpretation: lower risk coordinates yield higher k and e; r is the risk scalar.
    const double k = clamp01(1.0 - r_energy);
    const double e = clamp01(1.0 - r_hydraulics);
    const double r = clamp01(r_uncertainty);

    output->k_knowledge = k;
    output->e_ecoimpact = e;
    output->r_risk      = r;

    // KER score k * e - r.
    output->ker_score = k * e - r;

    // Lyapunov band check.
    output->lyapunov_ok = (delta_v <= config->max_delta_v);

    return 0;
}

// CSV-style diagnostic helper (for future CLI integration, not used by this kernel directly).
// Example CSV header:
// node_id,window_start_s,window_end_s,r_energy,r_hydraulics,r_uncertainty,v_t,v_next,delta_v,k,e,r,ker_score,lyapunov_ok,window_valid
// This kernel intentionally does not perform any IO.

} // namespace cyboquatic_workload
