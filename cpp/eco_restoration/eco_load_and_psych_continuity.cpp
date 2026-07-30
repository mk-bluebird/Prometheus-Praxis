// File: cpp/eco_restoration/eco_load_and_psych_continuity.cpp
#include <iostream>
#include <vector>
#include <cmath>
#include <iomanip>
#include <string>

namespace praxis {
namespace eco {

// ----------------------------------------
// 5. Eco-load index from AI workloads
// ----------------------------------------

struct ComputeSample {
    double t;             // time (hours since t0)
    double P_compute;     // compute power (kW)
    double eta_heat;      // heat-island amplification factor (dimensionless >= 1)
};

struct EcoLoadResult {
    double eco_load_index;        // integral E ≈ Σ P_compute * eta_heat * Δt
    double perf_budget_used;      // total compute energy (kWh)
};

double clamp01(double x) {
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

// Numerically integrate eco-load index over samples assuming uniform Δt.
EcoLoadResult integrate_eco_load(const std::vector<ComputeSample>& samples) {
    if (samples.size() < 2) {
        return EcoLoadResult{0.0, 0.0};
    }

    double E = 0.0;
    double perf = 0.0;

    for (std::size_t i = 1; i < samples.size(); ++i) {
        double dt = samples[i].t - samples[i-1].t;
        double P_mid = 0.5 * (samples[i].P_compute + samples[i-1].P_compute);
        double eta_mid = 0.5 * (samples[i].eta_heat + samples[i-1].eta_heat);
        E    += P_mid * eta_mid * dt;
        perf += P_mid * dt;
    }

    return EcoLoadResult{E, perf};
}

// Simple Phoenix data-center scheduling model.
// We distribute a fixed performance budget (total kWh) over N centers with different
// heat amplification factors, and iteratively shift load from hotter centers to
// cooler ones to reduce E while keeping total perf fixed.
struct DataCenter {
    std::string name;
    double eta_heat;     // heat amplification factor for this center
    double P_compute;    // average compute power allocated (kW)
};

struct SchedulingResult {
    std::vector<DataCenter> centers;
    double eco_load_index;
    double perf_budget;
};

// Compute eco-load given per-center power schedule over a fixed horizon H hours.
double eco_load_for_schedule(const std::vector<DataCenter>& centers,
                             double horizon_hours) {
    double E = 0.0;
    for (const auto& dc : centers) {
        E += dc.P_compute * dc.eta_heat * horizon_hours;
    }
    return E;
}

double perf_for_schedule(const std::vector<DataCenter>& centers,
                         double horizon_hours) {
    double perf = 0.0;
    for (const auto& dc : centers) {
        perf += dc.P_compute * horizon_hours;
    }
    return perf;
}

// Gradient-aware scheduling heuristic:
// - Given initial allocation and a fixed perf_budget (kWh),
// - Shift small epsilon of power from centers with higher eta_heat to those with lower,
//   approximating a gradient descent on eco-load subject to perf constraint.
SchedulingResult minimize_eco_load(std::vector<DataCenter> centers,
                                   double horizon_hours,
                                   double perf_budget,
                                   int iterations = 200,
                                   double step_kW = 5.0) {
    // Normalize initial schedule to match perf_budget.
    double perf_current = perf_for_schedule(centers, horizon_hours);
    if (perf_current <= 0.0) {
        return SchedulingResult{centers, 0.0, 0.0};
    }
    double scale = perf_budget / perf_current;
    for (auto& dc : centers) {
        dc.P_compute *= scale;
    }

    for (int it = 0; it < iterations; ++it) {
        // Find hottest and coolest centers by eta_heat.
        std::size_t idx_hot = 0, idx_cool = 0;
        for (std::size_t i = 0; i < centers.size(); ++i) {
            if (centers[i].eta_heat > centers[idx_hot].eta_heat) {
                idx_hot = i;
            }
            if (centers[i].eta_heat < centers[idx_cool].eta_heat) {
                idx_cool = i;
            }
        }

        // Compute eco-load gradient direction: move load from hot to cool.
        if (idx_hot != idx_cool && centers[idx_hot].P_compute > step_kW) {
            centers[idx_hot].P_compute -= step_kW;
            centers[idx_cool].P_compute += step_kW;
        }
    }

    double E_final = eco_load_for_schedule(centers, horizon_hours);
    double perf_final = perf_for_schedule(centers, horizon_hours);

    return SchedulingResult{centers, E_final, perf_final};
}

// ----------------------------------------
// 6. Psych-continuity trigger logic
// ----------------------------------------

enum class PsychBand {
    NORMAL,
    MODERATE,
    HIGH
};

struct RiskSample {
    double t_hours;
    double psych_risk;   // [0,1]
    double roh;          // Risk-of-Harm [0,1]
};

struct ContinuityState {
    PsychBand band;
    bool      rest_protocol_active;
};

PsychBand classify_band(double psych_risk, double roh) {
    psych_risk = clamp01(psych_risk);
    roh        = clamp01(roh);

    if (psych_risk < 0.4 && roh < 0.2) {
        return PsychBand::NORMAL;
    }
    if (psych_risk < 0.8 && roh <= 0.30) {
        return PsychBand::MODERATE;
    }
    return PsychBand::HIGH;
}

// State machine transitions on a sliding window of psych-risk and RoH.
// We assume samples are sorted by t_hours and window_length_hours is the width
// used to detect sustained HIGH states.
ContinuityState update_continuity(const std::vector<RiskSample>& window,
                                  const ContinuityState& prev,
                                  double window_length_hours) {
    if (window.empty()) {
        return prev;
    }

    // Compute average psych_risk and max RoH over the window.
    double sum_psych = 0.0;
    double max_roh   = 0.0;
    for (const auto& s : window) {
        sum_psych += s.psych_risk;
        if (s.roh > max_roh) max_roh = s.roh;
    }
    double avg_psych = sum_psych / static_cast<double>(window.size());

    PsychBand band_now = classify_band(avg_psych, max_roh);

    ContinuityState next = prev;
    next.band = band_now;

    // Trigger rest protocol if HIGH persists over the window.
    if (band_now == PsychBand::HIGH && window_length_hours >= 6.0) {
        next.rest_protocol_active = true;
    }

    // If band drops to NORMAL and RoH is low, allow rest protocol to deactivate gradually.
    if (band_now == PsychBand::NORMAL && max_roh < 0.15) {
        next.rest_protocol_active = false;
    }

    return next;
}

// Illustration of temporal logic (LTL) specification via comments:
//
// Let propositions:
//   H  := (band == HIGH)
//   R  := rest_protocol_active
//   M  := (band == MODERATE)
//   N  := (band == NORMAL)
//
// Example LTL formulas that the Rust/Kani stack should enforce:
//
// 1. G (H -> F_[<=6h] R)
//    "Globally, whenever psych_band=HIGH, within 6 hours rest_protocol_active must eventually hold."
//
// 2. G (R -> (H || M))
//    "Rest protocol is active only in MODERATE or HIGH bands."
//
// 3. G (N & roh < 0.15 -> !R)
//    "If band is NORMAL and RoH is low, rest protocol must eventually be deactivated."
//
// Kani harnesses over the Rust implementation would simulate windows and assert that
// these properties hold for all execution paths; here we only encode the state
// machine efficiently, leaving formal proof to the Rust side.

// ----------------------------------------
// Demonstration main
// ----------------------------------------

int main() {
    std::cout << std::fixed << std::setprecision(4);

    // 5. Eco-load index demonstration.
    std::vector<DataCenter> centers = {
        {"PHX_DC_NORTH", 1.25, 150.0},
        {"PHX_DC_SOUTH", 1.40, 200.0},
        {"PHX_DC_COOL",  1.05, 50.0}
    };

    double horizon_hours = 24.0;
    double perf_budget   = 10000.0; // kWh over horizon

    SchedulingResult sched = minimize_eco_load(centers, horizon_hours, perf_budget);

    std::cout << "Eco-load index from AI workloads (Phoenix data centers):\n";
    std::cout << "  Performance budget (kWh): " << sched.perf_budget << "\n";
    std::cout << "  Eco-load index E = Σ P_compute * eta_heat * Δt: " << sched.eco_load_index << "\n";
    std::cout << "  Final per-center schedule:\n";
    for (const auto& dc : sched.centers) {
        std::cout << "    " << dc.name << ": P_compute=" << dc.P_compute
                  << " kW, eta_heat=" << dc.eta_heat << "\n";
    }
    std::cout << "\n";

    // 6. Psych-continuity trigger logic demonstration.
    std::vector<RiskSample> window;
    for (int i = 0; i < 12; ++i) {
        // 12 samples over 6 hours (0.5h spacing) with rising psych_risk and RoH.
        double t = 0.5 * i;
        double psych = 0.5 + 0.04 * i;  // rising psych_risk
        double roh   = 0.18 + 0.01 * i; // rising RoH toward 0.30+
        window.push_back(RiskSample{t, clamp01(psych), clamp01(roh)});
    }

    ContinuityState prev{PsychBand::NORMAL, false};
    ContinuityState next = update_continuity(window, prev, 6.0);

    std::cout << "Psych-continuity state machine:\n";
    std::cout << "  Band after 6h window: "
              << (next.band == PsychBand::NORMAL ? "NORMAL" :
                  next.band == PsychBand::MODERATE ? "MODERATE" : "HIGH")
              << "\n";
    std::cout << "  Rest protocol active? "
              << (next.rest_protocol_active ? "YES" : "NO") << "\n";

    return 0;
}

} // namespace eco
} // namespace praxis
