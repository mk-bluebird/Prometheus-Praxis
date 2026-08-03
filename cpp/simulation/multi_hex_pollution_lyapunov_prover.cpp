// File: cpp/simulation/multi_hex_pollution_lyapunov_prover.cpp
#include <iostream>
#include <vector>
#include <string>
#include <cmath>

namespace eco {

struct PollutionState {
    double BOD; // biochemical oxygen demand
    double TSS; // total suspended solids
    double CEC; // contaminants of emerging concern
};

struct HexPollutionState {
    std::string hex_id;
    PollutionState state;
};

struct CouplingParams {
    double k_BOD_decay;
    double k_TSS_settle;
    double k_CEC_decay;
    double k_BOD_coupling;
    double k_TSS_coupling;
    double k_CEC_coupling;
};

struct LyapunovParams {
    double a_BOD;
    double a_TSS;
    double a_CEC;
};

struct StabilityReport {
    bool stable;
    double max_V;
    double min_dVdt;
};

// Lyapunov–Krasovskii candidate for multi-hex pollution:
// V = sum_h 0.5*(a_BOD x_BOD^2 + a_TSS x_TSS^2 + a_CEC x_CEC^2),
// where x = normalized deviation from target.
double lyapunov_V(const std::vector<HexPollutionState>& hexes,
                  const PollutionState& target,
                  const LyapunovParams& lp) {
    double V = 0.0;
    for (const auto& h : hexes) {
        double xB = (h.state.BOD - target.BOD) / std::max(target.BOD, 1e-3);
        double xT = (h.state.TSS - target.TSS) / std::max(target.TSS, 1e-3);
        double xC = (h.state.CEC - target.CEC) / std::max(target.CEC, 1e-3);
        V += 0.5 * (lp.a_BOD * xB * xB +
                    lp.a_TSS * xT * xT +
                    lp.a_CEC * xC * xC);
    }
    return V;
}

// Coupled ODEs for pollution across hexes (simplified linear couplings).
std::vector<HexPollutionState> pollution_dynamics(
        const std::vector<HexPollutionState>& hexes,
        const CouplingParams& cp) {
    std::size_t H = hexes.size();
    std::vector<HexPollutionState> dhex(H);

    for (std::size_t i = 0; i < H; ++i) {
        const auto& h = hexes[i];

        // Local decay/settling
        double dB_local = -cp.k_BOD_decay * h.state.BOD;
        double dT_local = -cp.k_TSS_settle * h.state.TSS;
        double dC_local = -cp.k_CEC_decay * h.state.CEC;

        // Coupling with neighbors (simple mean-field approximation)
        double B_mean = 0.0, T_mean = 0.0, C_mean = 0.0;
        for (const auto& hh : hexes) {
            B_mean += hh.state.BOD;
            T_mean += hh.state.TSS;
            C_mean += hh.state.CEC;
        }
        B_mean /= static_cast<double>(H);
        T_mean /= static_cast<double>(H);
        C_mean /= static_cast<double>(H);

        double dB_cpl = cp.k_BOD_coupling * (B_mean - h.state.BOD);
        double dT_cpl = cp.k_TSS_coupling * (T_mean - h.state.TSS);
        double dC_cpl = cp.k_CEC_coupling * (C_mean - h.state.CEC);

        dhex[i].hex_id = h.hex_id;
        dhex[i].state.BOD = dB_local + dB_cpl;
        dhex[i].state.TSS = dT_local + dT_cpl;
        dhex[i].state.CEC = dC_local + dC_cpl;
    }

    return dhex;
}

// Numerically verify that Lyapunov V decreases over time for given parameters.
StabilityReport verify_stability(std::vector<HexPollutionState> hexes,
                                 const PollutionState& target,
                                 const CouplingParams& cp,
                                 const LyapunovParams& lp,
                                 double t_end,
                                 double dt) {
    double t = 0.0;
    double max_V = lyapunov_V(hexes, target, lp);
    double min_dVdt = std::numeric_limits<double>::infinity();
    bool stable = true;

    while (t < t_end) {
        double V = lyapunov_V(hexes, target, lp);
        auto dhex = pollution_dynamics(hexes, cp);

        // Approximate dV/dt using directional derivative
        double dVdt = 0.0;
        for (std::size_t i = 0; i < hexes.size(); ++i) {
            double xB = (hexes[i].state.BOD - target.BOD) / std::max(target.BOD, 1e-3);
            double xT = (hexes[i].state.TSS - target.TSS) / std::max(target.TSS, 1e-3);
            double xC = (hexes[i].state.CEC - target.CEC) / std::max(target.CEC, 1e-3);

            double dB = dhex[i].state.BOD;
            double dT = dhex[i].state.TSS;
            double dC = dhex[i].state.CEC;

            dVdt += lp.a_BOD * xB * (dB / std::max(target.BOD, 1e-3))
                  + lp.a_TSS * xT * (dT / std::max(target.TSS, 1e-3))
                  + lp.a_CEC * xC * (dC / std::max(target.CEC, 1e-3));
        }

        if (V > max_V) {
            max_V = V;
        }
        if (dVdt < min_dVdt) {
            min_dVdt = dVdt;
        }
        if (dVdt > 1e-6) {
            stable = false;
        }

        // Euler step
        auto next_hexes = hexes;
        for (std::size_t i = 0; i < hexes.size(); ++i) {
            next_hexes[i].state.BOD += dhex[i].state.BOD * dt;
            next_hexes[i].state.TSS += dhex[i].state.TSS * dt;
            next_hexes[i].state.CEC += dhex[i].state.CEC * dt;
        }
        hexes = next_hexes;
        t += dt;
    }

    StabilityReport rep;
    rep.stable = stable;
    rep.max_V = max_V;
    rep.min_dVdt = min_dVdt;
    return rep;
}

void print_stability_report(const StabilityReport& rep) {
    std::cout << "Multi-hex pollution Lyapunov-Krasovskii stability report:\n";
    std::cout << "  stable: " << (rep.stable ? "true" : "false") << "\n";
    std::cout << "  max_V: " << rep.max_V << "\n";
    std::cout << "  min_dVdt: " << rep.min_dVdt << "\n";
}

} // namespace eco

int main() {
    using namespace eco;

    // Example: 3 hexes with initial pollution states.
    std::vector<HexPollutionState> hexes = {
        {"hex_1", {10.0, 20.0, 0.5}},
        {"hex_2", {12.0, 18.0, 0.4}},
        {"hex_3", {9.0, 22.0, 0.6}}
    };

    PollutionState target{5.0, 10.0, 0.2};
    CouplingParams cp{};
    cp.k_BOD_decay = 0.1;
    cp.k_TSS_settle = 0.08;
    cp.k_CEC_decay = 0.05;
    cp.k_BOD_coupling = 0.02;
    cp.k_TSS_coupling = 0.02;
    cp.k_CEC_coupling = 0.01;

    LyapunovParams lp{};
    lp.a_BOD = 1.0;
    lp.a_TSS = 1.0;
    lp.a_CEC = 2.0;

    StabilityReport rep = verify_stability(hexes, target, cp, lp,
                                           /*t_end=*/48.0,
                                           /*dt=*/0.5);
    print_stability_report(rep);

    return 0;
}
