// File: cpp/eco_restoration/psych_risk_features_and_reliability_token_fsm.cpp
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <iomanip>

namespace praxis {
namespace eco {

// ----------------------------------------------------------
// 7. Psych-risk feature engineering and capability invariant
// ----------------------------------------------------------

struct MultimodalSample {
    double eeg_power_band;      // normalized [0,1] aggregate of stress-related bands
    double fnirs_oxy_change;    // normalized [0,1] hemodynamic change in prefrontal cortex
    double gsr_level;           // normalized [0,1] galvanic skin response
    double ambient_temp;        // normalized [0,1] ambient temperature
    double electrode_snr;       // signal-to-noise ratio (dB), normalized [0,1]
    double electrode_drift;     // calibration drift [%/hr], normalized [0,1]
};

struct PsychRiskScore {
    double score;               // [0,1] robust, aggregated psych-risk
    double capability_index;    // [0,1] derived capability index for non-negative delta checks
};

// Feature engineering strategy:
// - Construct composite features that blend modalities, avoiding single-feature leakage.
// - Example composites:
//   f1 = eeg_power_band * fnirs_oxy_change
//   f2 = gsr_level * ambient_temp
//   f3 = electrode_stability = snr_weight * electrode_snr + drift_weight * (1 - electrode_drift)
//   f4 = cross_term = (eeg_power_band + gsr_level) * (1 - ambient_temp)
//
// Psych-risk score is then a bounded aggregation of these composites.
// Capability index is defined as 1 - score, so that interventions must not reduce it.
PsychRiskScore compute_psych_risk_score(const MultimodalSample& s) {
    double eeg   = s.eeg_power_band;
    double fnirs = s.fnirs_oxy_change;
    double gsr   = s.gsr_level;
    double temp  = s.ambient_temp;
    double snr_n = s.electrode_snr;
    double drift_n = s.electrode_drift;

    // Composite features (no single raw feature dominates).
    double f1 = eeg * fnirs;
    double f2 = gsr * temp;
    double f3 = 0.6 * snr_n + 0.4 * (1.0 - drift_n);
    double f4 = (eeg + gsr) * (1.0 - temp);

    // Aggregate with weights; clamp to [0,1].
    double risk_raw = 0.35 * f1 + 0.25 * f2 + 0.25 * (1.0 - f3) + 0.15 * f4;
    if (risk_raw < 0.0) risk_raw = 0.0;
    if (risk_raw > 1.0) risk_raw = 1.0;

    // Capability index: higher capabilities => lower psych-risk.
    double capability = 1.0 - risk_raw;
    return PsychRiskScore{risk_raw, capability};
}

// Validate non-negative capability delta across a sequence of interventions:
//
// We take a baseline sequence of samples (pre-intervention) and a matched
// sequence post-intervention, compute capability indices, and check:
//   capability_post[i] >= capability_pre[i]  for all i.
//
// In Rust+Kani, this would be a proof harness; here we implement a deterministic check.
struct CapabilityDeltaCheck {
    bool   invariant_holds;
    double min_delta;
};

CapabilityDeltaCheck validate_non_negative_capability_delta(
    const std::vector<MultimodalSample>& pre,
    const std::vector<MultimodalSample>& post) {

    std::size_t n = std::min(pre.size(), post.size());
    if (n == 0) {
        return CapabilityDeltaCheck{true, 0.0};
    }

    bool ok = true;
    double min_delta = 1e9;
    for (std::size_t i = 0; i < n; ++i) {
        PsychRiskScore pre_score  = compute_psych_risk_score(pre[i]);
        PsychRiskScore post_score = compute_psych_risk_score(post[i]);

        double delta = post_score.capability_index - pre_score.capability_index;
        if (delta < 0.0) {
            ok = false;
        }
        if (delta < min_delta) {
            min_delta = delta;
        }
    }

    if (min_delta == 1e9) {
        min_delta = 0.0;
    }
    return CapabilityDeltaCheck{ok, min_delta};
}

// ----------------------------------------------------------
// 8. Reliability-token lifecycle FSM
// ----------------------------------------------------------

enum class TokenState {
    UNINITIALIZED,
    PENDING,
    VALID,
    EXPIRED,
    REVOKED
};

struct ReliabilityToken {
    TokenState state;
    double     snr_db;           // measured SNR in dB
    double     drift_pct_per_hr; // measured calibration drift [%/hr]
    double     issued_time_hr;   // hours since epoch
    double     expiry_time_hr;   // hours since epoch
    bool       revoked_flag;
};

std::string to_string(TokenState s) {
    switch (s) {
        case TokenState::UNINITIALIZED: return "UNINITIALIZED";
        case TokenState::PENDING:       return "PENDING";
        case TokenState::VALID:         return "VALID";
        case TokenState::EXPIRED:       return "EXPIRED";
        case TokenState::REVOKED:       return "REVOKED";
    }
    return "UNKNOWN";
}

// Transition guards:
// - UNINITIALIZED -> PENDING: triggered when diagnostics are requested.
// - PENDING -> VALID: only if snr_db > 12.0 and drift_pct_per_hr < 2.0 and current_time <= expiry_time_hr.
// - VALID -> EXPIRED: when current_time > expiry_time_hr.
// - VALID -> REVOKED: when hardware reports fault or governance revokes token.
// - PENDING -> REVOKED: if diagnostics fail or snr/drift conditions are violated.
//
// These guards must be proven correct using Kani by asserting that no path leads
// to VALID with snr_db <= 12.0 or drift_pct_per_hr >= 2.0.
bool can_transition_to_pending(const ReliabilityToken& token) {
    return token.state == TokenState::UNINITIALIZED;
}

bool can_transition_to_valid(const ReliabilityToken& token,
                             double current_time_hr) {
    bool base = (token.state == TokenState::PENDING);
    bool snr_ok = token.snr_db > 12.0;
    bool drift_ok = token.drift_pct_per_hr < 2.0;
    bool time_ok = current_time_hr <= token.expiry_time_hr;
    bool not_revoked = !token.revoked_flag;

    return base && snr_ok && drift_ok && time_ok && not_revoked;
}

bool can_transition_to_expired(const ReliabilityToken& token,
                               double current_time_hr) {
    return token.state == TokenState::VALID &&
           current_time_hr > token.expiry_time_hr &&
           !token.revoked_flag;
}

bool can_transition_to_revoked_from_valid(const ReliabilityToken& token) {
    return token.state == TokenState::VALID;
}

bool can_transition_to_revoked_from_pending(const ReliabilityToken& token,
                                            double current_time_hr) {
    // Pending token may be revoked if diagnostics fail or expiry passes without validation.
    bool base = (token.state == TokenState::PENDING);
    bool expired = current_time_hr > token.expiry_time_hr;
    bool snr_bad = token.snr_db <= 12.0;
    bool drift_bad = token.drift_pct_per_hr >= 2.0;
    return base && (expired || snr_bad || drift_bad);
}

// Apply transition if guard holds.
void transition_to_pending(ReliabilityToken& token, double current_time_hr, double ttl_hours) {
    if (can_transition_to_pending(token)) {
        token.state = TokenState::PENDING;
        token.issued_time_hr = current_time_hr;
        token.expiry_time_hr = current_time_hr + ttl_hours;
        token.revoked_flag = false;
    }
}

void transition_to_valid(ReliabilityToken& token, double current_time_hr) {
    if (can_transition_to_valid(token, current_time_hr)) {
        token.state = TokenState::VALID;
    }
}

void transition_to_expired(ReliabilityToken& token, double current_time_hr) {
    if (can_transition_to_expired(token, current_time_hr)) {
        token.state = TokenState::EXPIRED;
    }
}

void transition_to_revoked(ReliabilityToken& token, double current_time_hr, bool from_valid) {
    if (from_valid && can_transition_to_revoked_from_valid(token)) {
        token.state = TokenState::REVOKED;
        token.revoked_flag = true;
    } else if (!from_valid && can_transition_to_revoked_from_pending(token, current_time_hr)) {
        token.state = TokenState::REVOKED;
        token.revoked_flag = true;
    }
}

// ----------------------------------------------------------
// Demonstration main
// ----------------------------------------------------------

int main() {
    std::cout << std::fixed << std::setprecision(4);

    // 7. Demonstrate psych-risk feature engineering and capability invariant.
    MultimodalSample pre_sample{
        0.6,  // eeg_power_band
        0.5,  // fnirs_oxy_change
        0.4,  // gsr_level
        0.3,  // ambient_temp
        0.8,  // electrode_snr (normalized)
        0.2   // electrode_drift (normalized)
    };

    MultimodalSample post_sample{
        0.5,  // eeg_power_band (slightly calmer)
        0.4,  // fnirs
        0.3,  // gsr
        0.3,  // ambient_temp unchanged
        0.85, // better snr
        0.15  // lower drift
    };

    std::vector<MultimodalSample> pre{pre_sample};
    std::vector<MultimodalSample> post{post_sample};

    CapabilityDeltaCheck cap_check = validate_non_negative_capability_delta(pre, post);
    PsychRiskScore pre_score  = compute_psych_risk_score(pre_sample);
    PsychRiskScore post_score = compute_psych_risk_score(post_sample);

    std::cout << "Psych-risk feature engineering:\n";
    std::cout << "  Pre score=" << pre_score.score
              << ", capability_index=" << pre_score.capability_index << "\n";
    std::cout << "  Post score=" << post_score.score
              << ", capability_index=" << post_score.capability_index << "\n";
    std::cout << "  Non-negative capability delta invariant holds? "
              << (cap_check.invariant_holds ? "YES" : "NO")
              << ", min_delta=" << cap_check.min_delta << "\n\n";

    // 8. Demonstrate reliability-token lifecycle FSM.
    ReliabilityToken token{
        TokenState::UNINITIALIZED,
        13.5,   // snr_db
        1.5,    // drift_pct_per_hr
        0.0,
        0.0,
        false
    };

    double t0 = 100.0;     // hours since epoch
    double ttl = 24.0;     // token time-to-live hours

    std::cout << "Reliability token lifecycle:\n";
    std::cout << "  Initial state: " << to_string(token.state) << "\n";

    transition_to_pending(token, t0, ttl);
    std::cout << "  After diagnostics request, state: " << to_string(token.state)
              << " (issued=" << token.issued_time_hr
              << ", expiry=" << token.expiry_time_hr << ")\n";

    transition_to_valid(token, t0 + 1.0);
    std::cout << "  After validation, state: " << to_string(token.state) << "\n";

    transition_to_expired(token, t0 + 25.0);
    std::cout << "  After expiry check at t=" << (t0 + 25.0)
              << "h, state: " << to_string(token.state) << "\n";

    // Reset and demonstrate revocation path from PENDING when conditions fail.
    token.state = TokenState::UNINITIALIZED;
    token.snr_db = 10.0;          // too low
    token.drift_pct_per_hr = 3.0; // too high
    token.revoked_flag = false;

    transition_to_pending(token, t0, ttl);
    transition_to_revoked(token, t0 + 2.0, false);
    std::cout << "  Pending token with bad SNR/drift revoked, state: "
              << to_string(token.state) << "\n";

    return 0;
}

} // namespace eco
} // namespace praxis
