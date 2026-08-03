// File: cpp/eco_restoration/ker_lyapunov_carbon_stability.cpp
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <limits>

namespace eco {

// -----------------------------
// Core data structures
// -----------------------------

struct Workload {
    std::string id;
    double k;           // knowledge factor in [0,1]
    double e;           // eco-efficiency in [0,1]
    double r;           // risk-of-harm in [0,1]
    bool is_research;   // true if in pure RESEARCH lane

    double eco_energy;  // E^{(i,h)} base energy term
    double eco_efficiency; // eco_efficiency in [0,1]

    Workload(const std::string& id_,
             double k_, double e_, double r_,
             bool is_research_,
             double eco_energy_,
             double eco_eff_)
        : id(id_), k(k_), e(e_), r(r_),
          is_research(is_research_),
          eco_energy(eco_energy_), eco_efficiency(eco_eff_) {}
};

struct HexCell {
    std::string id;
    double carbon_intensity; // local carbon intensity
    double max_carbon;       // corridor max for normalization
    double c_min;            // minimum allowed carbon corridor
    double s_min;            // minimum allowed KER scalar for non-RESEARCH
    double epsilon;          // per-step Lyapunov drift cap
    double B;                // saturation bound for residual
    double V0;               // initial residual

    std::vector<Workload> active_workloads;

    HexCell(const std::string& id_,
            double carbon_intensity_,
            double max_carbon_,
            double c_min_,
            double s_min_,
            double epsilon_,
            double B_,
            double V0_)
        : id(id_),
          carbon_intensity(carbon_intensity_),
          max_carbon(max_carbon_),
          c_min(c_min_),
          s_min(s_min_),
          epsilon(epsilon_),
          B(B_),
          V0(V0_) {}
};

// -----------------------------
// KER–Lyapunov–carbon corridor logic
// -----------------------------

struct StabilityParams {
    double alpha;       // scaling for eco penalty
    double beta;        // Lyapunov drift coupling factor
    double gamma;       // KER coupling
    double delta;       // carbon corridor coupling
    double delta_V_max; // global cap on per-workload drift

    StabilityParams(double alpha_,
                    double beta_,
                    double gamma_,
                    double delta_,
                    double delta_V_max_)
        : alpha(alpha_),
          beta(beta_),
          gamma(gamma_),
          delta(delta_),
          delta_V_max(delta_V_max_) {}
};

// Compute KER scalar s = k*e - r, with non-RESEARCH constraint s >= s_min.
double ker_scalar(const Workload& w, double s_min) {
    double s = w.k * w.e - w.r;
    if (!w.is_research) {
        if (s < s_min) {
            // Enforce non-RESEARCH constraint by clamping to s_min
            s = s_min;
        }
    }
    return s;
}

// Compute carbon corridor c_h = 1 - carbon_intensity / max_carbon, with c_h in [0,1]
double carbon_corridor(const HexCell& h) {
    if (h.max_carbon <= 0.0) {
        return 0.0;
    }
    double c = 1.0 - h.carbon_intensity / h.max_carbon;
    if (c < 0.0) c = 0.0;
    if (c > 1.0) c = 1.0;
    if (c < h.c_min) {
        // Enforce minimum corridor for eco-significant workloads
        c = h.c_min;
    }
    return c;
}

// Compute eco energy term E_eco = E * (1 + alpha * (1 - eco_efficiency))
double eco_energy_term(const Workload& w, double alpha) {
    double eff = w.eco_efficiency;
    if (eff < 0.0) eff = 0.0;
    if (eff > 1.0) eff = 1.0;
    return w.eco_energy * (1.0 + alpha * (1.0 - eff));
}

// Compute per-workload Lyapunov drift ΔV_t^{(i,h)} = beta * E_eco,
// then apply corridor bound ΔV_t ≤ min(ΔV_max, γ s, δ c_h).
double workload_drift(const Workload& w,
                      const HexCell& h,
                      const StabilityParams& params) {
    double s = ker_scalar(w, h.s_min);
    double c = carbon_corridor(h);
    double E_eco = eco_energy_term(w, params.alpha);
    double delta_V = params.beta * E_eco;

    double bound_ker = params.gamma * s;
    double bound_carbon = params.delta * c;
    double local_max = std::min(params.delta_V_max,
                                std::min(bound_ker, bound_carbon));

    if (delta_V > local_max) {
        delta_V = local_max;
    }
    if (delta_V < 0.0) {
        delta_V = 0.0;
    }
    return delta_V;
}

// -----------------------------
// Hex Lyapunov residual tracking
// -----------------------------

struct HexResidualState {
    const HexCell* hex;
    double V; // current residual V_t^{(h)}

    explicit HexResidualState(const HexCell* h)
        : hex(h), V(h->V0) {}
};

// Scheduler ensures per-hex drift cap and saturation bound.
// It returns the actual applied drift after throttling.
double apply_hex_drift(HexResidualState& state,
                       const std::vector<Workload>& workloads,
                       const StabilityParams& params) {
    const HexCell& h = *(state.hex);
    double step_sum = 0.0;
    for (const auto& w : workloads) {
        double dV = workload_drift(w, h, params);
        step_sum += dV;
        if (step_sum >= h.epsilon) {
            step_sum = h.epsilon;
            break;
        }
    }
    double V_candidate = state.V + step_sum;
    double V_cap = h.V0 + h.B;
    if (V_candidate > V_cap) {
        // Throttle by scaling down the applied drift so we do not exceed cap.
        step_sum = std::max(0.0, V_cap - state.V);
        V_candidate = state.V + step_sum;
    }
    state.V = V_candidate;
    return step_sum;
}

// Compute global residual V_t = sum_h V_t^{(h)}
double global_residual(const std::vector<HexResidualState>& states) {
    double total = 0.0;
    for (const auto& s : states) {
        total += s.V;
    }
    return total;
}

// -----------------------------
// Discrete-time simulation for stability demonstration
// -----------------------------

struct SimulationResult {
    std::vector<double> global_V_trace;
    std::vector<std::vector<double>> hex_V_trace;
};

SimulationResult run_stability_simulation(
        const std::vector<HexCell>& hexes,
        const std::vector<std::vector<std::vector<Workload>>>& hex_workloads_over_time,
        const StabilityParams& params,
        bool enforce_non_positive_expected_drift) {

    std::vector<HexResidualState> states;
    states.reserve(hexes.size());
    for (const auto& h : hexes) {
        states.emplace_back(&h);
    }

    SimulationResult result;
    result.hex_V_trace.resize(hexes.size());

    const std::size_t T = hex_workloads_over_time.empty()
                          ? 0
                          : hex_workloads_over_time.front().size();

    for (std::size_t t = 0; t < T; ++t) {
        // Apply scheduler per hex
        for (std::size_t h_idx = 0; h_idx < hexes.size(); ++h_idx) {
            const auto& workloads_at_t = hex_workloads_over_time[h_idx][t];
            double applied_drift = apply_hex_drift(states[h_idx], workloads_at_t, params);

            if (enforce_non_positive_expected_drift) {
                // Simple enforcement: if applied drift would be positive,
                // probabilistically reduce it to zero (toy supermartingale construction).
                if (applied_drift > 0.0) {
                    // For deterministic code, we just cancel drift for demonstration.
                    states[h_idx].V -= applied_drift;
                    if (states[h_idx].V < hexes[h_idx].V0) {
                        states[h_idx].V = hexes[h_idx].V0;
                    }
                }
            }
            result.hex_V_trace[h_idx].push_back(states[h_idx].V);
        }
        result.global_V_trace.push_back(global_residual(states));
    }

    return result;
}

// -----------------------------
// Utility function: print trace
// -----------------------------

void print_simulation(const std::vector<HexCell>& hexes,
                      const SimulationResult& res) {
    std::cout << "Global Lyapunov residual trace V_t:\n";
    for (std::size_t t = 0; t < res.global_V_trace.size(); ++t) {
        std::cout << "t=" << t << " : V_t = " << res.global_V_trace[t] << "\n";
    }
    std::cout << "\nPer-hex residuals:\n";
    for (std::size_t h_idx = 0; h_idx < hexes.size(); ++h_idx) {
        std::cout << "Hex " << hexes[h_idx].id << ":\n";
        for (std::size_t t = 0; t < res.hex_V_trace[h_idx].size(); ++t) {
            std::cout << "  t=" << t << " : V_t^{(h)} = " << res.hex_V_trace[h_idx][t] << "\n";
        }
    }
}

} // namespace eco

// -----------------------------
// Example main: demonstrate boundedness of V_t
// -----------------------------
int main() {
    using namespace eco;

    StabilityParams params(
        /*alpha=*/0.5,
        /*beta=*/0.01,
        /*gamma=*/0.1,
        /*delta=*/0.1,
        /*delta_V_max=*/0.05
    );

    HexCell h1("hex_A", /*carbon_intensity=*/0.3, /*max_carbon=*/1.0,
               /*c_min=*/0.2, /*s_min=*/0.1, /*epsilon=*/0.02,
               /*B=*/0.5, /*V0=*/0.0);
    HexCell h2("hex_B", /*carbon_intensity=*/0.5, /*max_carbon=*/1.0,
               /*c_min=*/0.2, /*s_min=*/0.1, /*epsilon=*/0.015,
               /*B=*/0.4, /*V0=*/0.0);
    std::vector<HexCell> hexes = {h1, h2};

    std::size_t T = 10;

    // hex_workloads_over_time[hex_idx][t] = workloads active on hex at time t
    std::vector<std::vector<std::vector<Workload>>> hex_workloads_over_time;
    hex_workloads_over_time.resize(hexes.size());
    for (std::size_t h_idx = 0; h_idx < hexes.size(); ++h_idx) {
        hex_workloads_over_time[h_idx].resize(T);
    }

    // Populate workloads with moderate eco energy to respect epsilon and B
    for (std::size_t t = 0; t < T; ++t) {
        hex_workloads_over_time[0][t].emplace_back(
            "wA1_t" + std::to_string(t),
            /*k=*/0.9, /*e=*/0.8, /*r=*/0.2,
            /*is_research=*/false,
            /*eco_energy=*/1.0,
            /*eco_efficiency=*/0.9
        );
        hex_workloads_over_time[1][t].emplace_back(
            "wB1_t" + std::to_string(t),
            /*k=*/0.85, /*e=*/0.75, /*r=*/0.25,
            /*is_research=*/false,
            /*eco_energy=*/0.8,
            /*eco_efficiency=*/0.88
        );
    }

    SimulationResult res = run_stability_simulation(
        hexes,
        hex_workloads_over_time,
        params,
        /*enforce_non_positive_expected_drift=*/false
    );

    print_simulation(hexes, res);

    return 0;
}
