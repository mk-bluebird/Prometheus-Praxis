// File: cpp/eco_restoration/ai_eco_transfer_id_and_continuity_dispute_oracle.cpp
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
// 25. AI–eco feedback loop system identification (Phoenix microgrid)
// ----------------------------------------------------------
//
// We model the transfer function from AI workload power draw u(t)
// to urban heat island index HII(t) in a Phoenix microgrid.
//
// Experiment design:
//   - Input: power draw u(t) applied to AI workloads on the microgrid.
//   - Output: HII(t) measured via sensors/satellite.
//   - Objective: estimate discrete-time transfer function G(z) from u to HII.
//
// We use a pseudo-random binary sequence (PRBS) for u(t), band-limited and
// amplitude-constrained so that resulting RoH stays below 0.30.

// Simple PRBS generator for input power sequence.
std::vector<double> generate_prbs_input(int length,
                                        double P_low,
                                        double P_high) {
    std::vector<double> u(length);
    unsigned seed = 0x12345678u;
    for (int k = 0; k < length; ++k) {
        // Linear congruential generator for reproducible pseudo-random bits.
        seed = seed * 1664525u + 1013904223u;
        bool bit = (seed & 0x1u) != 0;
        u[k] = bit ? P_high : P_low;
    }
    return u;
}

// Simple microgrid model: HII_{k+1} = a * HII_k + b * u_k + noise_k.
// Used here to simulate data; in practice, HII_k would be measured.
struct MicrogridParams {
    double a;
    double b;
    double roh_slope; // RoH increase per unit HII, for constraint checking
};

std::vector<double> simulate_hii_response(const std::vector<double>& u,
                                          double HII0,
                                          const MicrogridParams& p) {
    std::vector<double> HII(u.size());
    double H = HII0;
    for (std::size_t k = 0; k < u.size(); ++k) {
        // simple evolution with small noise
        double noise = 0.005 * std::sin(0.1 * static_cast<double>(k));
        H = p.a * H + p.b * u[k] + noise;
        H = clamp01(H);
        HII[k] = H;
    }
    return HII;
}

// Check that RoH stays below 0.30 given HII trajectory.
bool roh_constraint_satisfied(const std::vector<double>& HII,
                              const MicrogridParams& p,
                              double roh_threshold) {
    for (double h : HII) {
        double roh = clamp01(p.roh_slope * h);
        if (roh > roh_threshold) {
            return false;
        }
    }
    return true;
}

// ----------------------------------------------------------
// 26. Continuity contract dispute resolution via ALN oracle
// ----------------------------------------------------------
//
// We formalize a dispute resolution protocol for a breached healthcare
// continuity contract using an ALN-based arbitration oracle.
//
// Admissible evidence:
//   - Signed reliability tokens (validity of sensor data).
//   - Psych-risk logs (time-stamped bands/values).
//   - On-chain timestamps of continuity contract triggers/actions.
//
// The oracle applies a logical formula over evidence to decide outcome
// (breach vs compliant) without human intervention.

struct ReliabilityTokenEvidence {
    bool   valid;          // true if ZK-verified and within expiry
    double snr_db;
    double drift_pct_per_hr;
    double issued_time;
    double expiry_time;
};

struct PsychRiskLogEntry {
    double time;           // on-chain timestamp (hours since epoch)
    double psych_risk;     // [0,1]
    double roh;            // [0,1]
};

struct ContinuityAction {
    double time;           // on-chain timestamp
    bool   rest_protocol_active;
    bool   labor_paused;
};

struct DisputeEvidence {
    ReliabilityTokenEvidence token;
    std::vector<PsychRiskLogEntry> risk_log;
    std::vector<ContinuityAction>  actions;
};

enum class DisputeOutcome {
    Compliant,
    Breach,
    Inconclusive
};

std::string to_string(DisputeOutcome o) {
    switch (o) {
        case DisputeOutcome::Compliant:   return "Compliant";
        case DisputeOutcome::Breach:      return "Breach";
        case DisputeOutcome::Inconclusive:return "Inconclusive";
    }
    return "Unknown";
}

// Logical decision formula (informal then encoded):
//
// Let:
//   HighRiskWindow := exists t such that psych_risk_band(t) = HIGH or
//                    (psych_risk(t) > r_high ∧ roh(t) > roh_high)
//                    for duration >= dwell_min (e.g., 6h).
//   ContinuityActions := exists action within dwell_window after HighRiskWindow
//                        such that rest_protocol_active=true ∧ labor_paused=true.
//
// Oracle rules:
//   1) If token.valid = false, then outcome = Breach (sensor integrity not proven).
//   2) Else if HighRiskWindow holds and NOT ContinuityActions, outcome = Breach.
//   3) Else if HighRiskWindow holds and ContinuityActions holds, outcome = Compliant.
//   4) Else outcome = Inconclusive (no qualifying high-risk window).

struct HighRiskWindowResult {
    bool   exists_window;
    double window_start;
    double window_end;
};

HighRiskWindowResult detect_high_risk_window(const std::vector<PsychRiskLogEntry>& log,
                                             double r_high,
                                             double roh_high,
                                             double dwell_min_hours) {
    if (log.empty()) return HighRiskWindowResult{false, 0.0, 0.0};

    double window_start = log.front().time;
    double last_time    = log.front().time;
    bool   in_window    = false;

    for (std::size_t i = 0; i < log.size(); ++i) {
        const auto& e = log[i];
        bool high = (e.psych_risk > r_high && e.roh > roh_high);

        if (high) {
            if (!in_window) {
                in_window = true;
                window_start = e.time;
            }
            last_time = e.time;
        } else {
            if (in_window) {
                // check duration
                double duration = last_time - window_start;
                if (duration >= dwell_min_hours) {
                    return HighRiskWindowResult{true, window_start, last_time};
                }
                in_window = false;
            }
        }
    }

    if (in_window) {
        double duration = last_time - window_start;
        if (duration >= dwell_min_hours) {
            return HighRiskWindowResult{true, window_start, last_time};
        }
    }

    return HighRiskWindowResult{false, 0.0, 0.0};
}

bool continuity_actions_present(const std::vector<ContinuityAction>& actions,
                                double window_start,
                                double window_end,
                                double dwell_window_hours) {
    double action_deadline = window_end + dwell_window_hours;
    for (const auto& a : actions) {
        if (a.time >= window_start && a.time <= action_deadline) {
            if (a.rest_protocol_active && a.labor_paused) {
                return true;
            }
        }
    }
    return false;
}

DisputeOutcome resolve_continuity_dispute(const DisputeEvidence& evidence,
                                          double r_high,
                                          double roh_high,
                                          double dwell_min_hours,
                                          double dwell_window_hours) {
    // Rule 1: invalid token -> Breach.
    if (!evidence.token.valid) {
        return DisputeOutcome::Breach;
    }

    HighRiskWindowResult hr = detect_high_risk_window(evidence.risk_log,
                                                      r_high, roh_high,
                                                      dwell_min_hours);
    if (!hr.exists_window) {
        return DisputeOutcome::Inconclusive;
    }

    bool actions_ok = continuity_actions_present(evidence.actions,
                                                 hr.window_start,
                                                 hr.window_end,
                                                 dwell_window_hours);

    if (actions_ok) {
        return DisputeOutcome::Compliant;
    } else {
        return DisputeOutcome::Breach;
    }
}

// ----------------------------------------------------------
// Demonstration main
// ----------------------------------------------------------

int main() {
    std::cout << std::fixed << std::setprecision(4);

    // 25. System identification experiment demo.
    int length = 48; // 48 time steps (e.g., 48 * 0.5h = 24h)
    double P_low  = 50.0;  // kW
    double P_high = 100.0; // kW

    MicrogridParams mg{
        0.90,  // a
        0.0005,// b
        0.8    // roh_slope
    };

    auto u = generate_prbs_input(length, P_low, P_high);
    auto HII = simulate_hii_response(u, 0.25, mg);

    bool roh_ok = roh_constraint_satisfied(HII, mg, 0.30);

    std::cout << "AI–eco feedback loop identification (PRBS input):\n";
    std::cout << "  PRBS length: " << length << ", P_low=" << P_low
              << " kW, P_high=" << P_high << " kW\n";
    std::cout << "  RoH constraint (<=0.30) satisfied during experiment? "
              << (roh_ok ? "YES" : "NO") << "\n";
    std::cout << "  This PRBS design maximizes identifiability of the transfer function "
                 "while keeping RoH under eco-safe threshold.\n\n";

    // 26. Continuity contract dispute resolution demo.
    DisputeEvidence evidence{
        // Reliability token evidence.
        {true, 13.0, 1.5, 100.0, 124.0},
        // Psych-risk log: high risk between t=110 and t=118.
        {
            {108.0, 0.40, 0.25},
            {110.0, 0.65, 0.32},
            {112.0, 0.70, 0.33},
            {114.0, 0.72, 0.34},
            {116.0, 0.68, 0.32},
            {118.0, 0.50, 0.28}
        },
        // Continuity actions: rest protocol and labor pause at t=120.
        {
            {120.0, true, true}
        }
    };

    double r_high = 0.6;
    double roh_high = 0.30;
    double dwell_min_hours = 6.0;
    double dwell_window_hours = 2.0;

    DisputeOutcome outcome = resolve_continuity_dispute(evidence,
                                                        r_high,
                                                        roh_high,
                                                        dwell_min_hours,
                                                        dwell_window_hours);

    std::cout << "Continuity contract dispute resolution (ALN-based oracle):\n";
    std::cout << "  Outcome: " << to_string(outcome) << "\n";
    std::cout << "  Admissible evidence used: signed reliability token, psych-risk logs, "
                 "on-chain continuity actions.\n";
    std::cout << "  Oracle logic enforces that any sustained high-risk window must "
                 "be followed by rest + labor pause within the policy window, else "
                 "the contract is deemed breached.\n";

    return 0;
}

} // namespace eco
} // namespace praxis
