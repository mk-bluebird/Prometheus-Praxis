// File: cpp/eco_restoration/provenance_harness_specs_and_risk_band_algorithm.cpp
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <iomanip>

namespace praxis {
namespace eco {

// ----------------------------------------------------------
// 15. Proof burden for provenance harnesses (SensorIntegrityKernel)
// ----------------------------------------------------------

// Sensor failure scenarios to be covered by provenance harnesses:
//
// 1) Signal loss (SNR -> 0, no valid samples).
// 2) Noise spike (transient high-amplitude artifact).
// 3) Long-term drift (baseline shift beyond allowed drift %/hr).
// 4) PFAS degradation (electrode material degradation impacting SNR/drift).

enum class FailureScenario {
    SignalLoss,
    NoiseSpike,
    LongTermDrift,
    PFASDegradation
};

std::string to_string(FailureScenario s) {
    switch (s) {
        case FailureScenario::SignalLoss:      return "SignalLoss";
        case FailureScenario::NoiseSpike:      return "NoiseSpike";
        case FailureScenario::LongTermDrift:   return "LongTermDrift";
        case FailureScenario::PFASDegradation: return "PFASDegradation";
    }
    return "Unknown";
}

struct HoareTriple {
    FailureScenario scenario;
    std::string     precondition;
    std::string     postcondition;
};

std::vector<HoareTriple> sensor_provenance_hoare_triples() {
    std::vector<HoareTriple> v;

    // 1) Signal loss.
    v.push_back(HoareTriple{
        FailureScenario::SignalLoss,
        "Pre: snr_db <= 0.0 ∧ valid_samples = 0 ∧ state = UNINITIALIZED ∨ PENDING",
        "Post: reliability_token.state = REVOKED ∧ reliability_token.revoked_flag = true ∧ "
        "SensorIntegrityKernel marks sensor_status = FAILED ∧ no psych_state decisions permitted."
    });

    // 2) Noise spike.
    v.push_back(HoareTriple{
        FailureScenario::NoiseSpike,
        "Pre: snr_db fluctuates rapidly with transient spike amplitude > spike_threshold ∧ "
        "drift_pct_per_hr within nominal range ∧ state = PENDING",
        "Post: reliability_token.state = REVOKED ∧ sensor_status = DEGRADED ∧ "
        "kernel logs anomaly and rejects reliability_token issuance; no transition to VALID."
    });

    // 3) Long-term drift.
    v.push_back(HoareTriple{
        FailureScenario::LongTermDrift,
        "Pre: drift_pct_per_hr >= drift_limit (e.g., 2%/hr) for window_duration ≥ drift_window_min ∧ "
        "state ∈ {PENDING, VALID}",
        "Post: reliability_token.state = REVOKED ∧ sensor_status = FAILED ∧ "
        "all subsequent psych_state updates require new calibration; continuity claims blocked."
    });

    // 4) PFAS degradation.
    v.push_back(HoareTriple{
        FailureScenario::PFASDegradation,
        "Pre: pfas_degradation_flag = true ∧ snr_db decreasing trend below SNR_min (e.g., 12 dB) ∧ "
        "drift_pct_per_hr trending upward beyond drift_limit ∧ state ∈ {PENDING, VALID}",
        "Post: reliability_token.state = REVOKED ∧ sensor_status = DEGRADED ∧ "
        "kernel emits critical provenance log and prohibits VALID state until PFAS remediation or "
        "electrode replacement; psych_risk remains in observational-only mode."
    });

    return v;
}

void print_hoare_triples(const std::vector<HoareTriple>& triples) {
    std::cout << "SensorIntegrityKernel provenance Hoare triples (proof burden):\n\n";
    for (const auto& ht : triples) {
        std::cout << "[" << to_string(ht.scenario) << "]\n";
        std::cout << "  Pre:  " << ht.precondition  << "\n";
        std::cout << "  Post: " << ht.postcondition << "\n\n";
    }
}

// ----------------------------------------------------------
// 16. Dynamic risk-band reclassification algorithm
// ----------------------------------------------------------

// Risk bands: NORMAL, MODERATE, HIGH.
enum class RiskBand {
    NORMAL,
    MODERATE,
    HIGH
};

std::string to_string(RiskBand b) {
    switch (b) {
        case RiskBand::NORMAL:   return "NORMAL";
        case RiskBand::MODERATE: return "MODERATE";
        case RiskBand::HIGH:     return "HIGH";
    }
    return "UNKNOWN";
}

// Online algorithm with hysteresis and dwell-time constraints.
//
// Inputs:
//   r_t    : psych-risk score at discrete time t (normalized [0,1])
//   dt     : sampling interval (hours)
//   band   : current band
//   dwell  : time spent in current band (hours)
//
// Algorithm:
//   - If band=NORMAL and r_t > thresh_up (e.g., 0.5) for dwell >= dwell_min,
//       band -> MODERATE, dwell reset.
//   - If band=MODERATE and r_t < thresh_down (e.g., 0.3) for dwell >= dwell_min,
//       band -> NORMAL, dwell reset.
//   - HIGH transitions follow similar logic but are not central to the oscillation bound.
//
// This hysteresis + dwell-time ensures at most one NORMAL↔MODERATE transition per hour
// under clean sensor data, because dwell_min ≥ 1.0 and dt <= dwell_min/steps_per_hour.

struct RiskBandState {
    RiskBand band;
    double   dwell_hours;
};

RiskBandState update_risk_band(double r_t, double dt,
                               const RiskBandState& prev,
                               double thresh_up,
                               double thresh_down,
                               double dwell_min_hours) {
    RiskBandState next = prev;
    next.dwell_hours += dt;

    switch (prev.band) {
        case RiskBand::NORMAL:
            if (r_t > thresh_up && next.dwell_hours >= dwell_min_hours) {
                next.band = RiskBand::MODERATE;
                next.dwell_hours = 0.0;
            }
            break;
        case RiskBand::MODERATE:
            if (r_t < thresh_down && next.dwell_hours >= dwell_min_hours) {
                next.band = RiskBand::NORMAL;
                next.dwell_hours = 0.0;
            }
            break;
        case RiskBand::HIGH:
            // For completeness, simple HIGH logic: if r_t < thresh_high_down, drop to MODERATE.
            if (r_t < thresh_up && next.dwell_hours >= dwell_min_hours) {
                next.band = RiskBand::MODERATE;
                next.dwell_hours = 0.0;
            }
            break;
    }

    return next;
}

// Non-linear difference inclusion interpretation:
//
// Let x_t = (band_t, dwell_t) be the state.
// The update can be written as:
//   dwell_{t+1} ∈ dwell_t + dt
//   band_{t+1} ∈ f(band_t, dwell_t, r_t)
//
// where f is piecewise-defined with thresholds:
//   If band_t = NORMAL and r_t <= thresh_up => band_{t+1} = NORMAL.
//   If band_t = NORMAL and r_t >  thresh_up and dwell_t >= dwell_min => band_{t+1} = MODERATE.
//   etc.
//
// Stability claim under clean sensor data:
//   If dt <= dwell_min_hours / 1 and r_t is bounded and monotone on each hour-scale,
//   then band_t cannot oscillate NORMAL→MODERATE→NORMAL more than once per hour.
//
// Sketch of proof in this implementation:
//   - A transition requires dwell ≥ dwell_min_hours.
//   - After a transition, dwell resets to 0.
//   - Thus, two opposite-direction transitions require at least 2 * dwell_min_hours.
//   - For dwell_min_hours ≥ 1.0, this implies at most one NORMAL↔MODERATE oscillation per hour.

struct OscillationCheckResult {
    int  transitions_normal_to_moderate;
    int  transitions_moderate_to_normal;
    bool within_bound;
};

OscillationCheckResult simulate_band_oscillation(const std::vector<double>& r_series,
                                                 double dt,
                                                 double thresh_up,
                                                 double thresh_down,
                                                 double dwell_min_hours) {
    RiskBandState state{RiskBand::NORMAL, 0.0};
    int n2m = 0;
    int m2n = 0;

    double time = 0.0;
    for (double r_t : r_series) {
        RiskBandState next = update_risk_band(r_t, dt, state, thresh_up, thresh_down, dwell_min_hours);
        if (state.band == RiskBand::NORMAL && next.band == RiskBand::MODERATE) {
            ++n2m;
        } else if (state.band == RiskBand::MODERATE && next.band == RiskBand::NORMAL) {
            ++m2n;
        }
        state = next;
        time += dt;
    }

    // Bound: at most floor(total_time / dwell_min_hours) transitions of any kind.
    int max_allowed = static_cast<int>(std::floor(time / dwell_min_hours));
    bool within_bound = (n2m + m2n) <= max_allowed;

    return OscillationCheckResult{n2m, m2n, within_bound};
}

// ----------------------------------------------------------
// Demonstration main
// ----------------------------------------------------------

int main() {
    std::cout << std::fixed << std::setprecision(4);

    // 15. Print Hoare triples for SensorIntegrityKernel provenance.
    auto triples = sensor_provenance_hoare_triples();
    print_hoare_triples(triples);

    // 16. Demonstrate dynamic risk-band reclassification and oscillation bound.
    double dt = 0.25;             // 15 minutes per sample
    double thresh_up = 0.5;
    double thresh_down = 0.3;
    double dwell_min_hours = 1.0; // require 1h in a band before transition

    // Simulated clean sensor data: psych-risk slowly rising then falling over 4 hours.
    std::vector<double> r_series;
    for (int i = 0; i < 16; ++i) { // 16 samples over 4 hours
        double t = dt * i;
        double r_t;
        if (t < 2.0) {
            r_t = 0.2 + 0.2 * (t / 2.0); // rising from 0.2 to 0.4
        } else {
            r_t = 0.4 - 0.2 * ((t - 2.0) / 2.0); // falling back to 0.2
        }
        r_series.push_back(clamp01(r_t));
    }

    OscillationCheckResult osc = simulate_band_oscillation(r_series, dt,
                                                           thresh_up, thresh_down,
                                                           dwell_min_hours);

    std::cout << "Dynamic risk-band reclassification:\n";
    std::cout << "  NORMAL->MODERATE transitions: " << osc.transitions_normal_to_moderate << "\n";
    std::cout << "  MODERATE->NORMAL transitions: " << osc.transitions_moderate_to_normal << "\n";
    std::cout << "  Within theoretical bound (<= floor(total_time / dwell_min))? "
              << (osc.within_bound ? "YES" : "NO") << "\n";

    return 0;
}

} // namespace eco
} // namespace praxis
