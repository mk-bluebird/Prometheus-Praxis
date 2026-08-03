// File: cpp/simulation/lqr_lyapunov_supervisory_controller_test.cpp
#include <iostream>
#include <vector>
#include <string>
#include <cmath>

namespace eco {

struct CanalState {
    double Q; // discharge
    double C; // pollutant concentration
    double T; // temperature
};

struct CanalTargets {
    double Q_star;
    double C_star;
    double T_star;
    double T_ambient;
};

struct CanalParams {
    double a_Q;
    double a_C;
    double a_T;

    double alpha_Q;
    double b_p;
    double b_g;

    double k_decay;
    double k_inflow_base;
    double k_flush_base;

    double k_T;
    double k_mix_base;
    double k_solar_amp;
};

struct LqrGains {
    double K_Q;
    double K_C;
    double K_T;
};

struct GovernanceParams {
    double V_cap;        // Lyapunov residual cap
    double delta_V_max;  // per-step Lyapunov drift cap
};

// Quadratic Lyapunov function.
double lyapunov_V(const CanalState& s,
                  const CanalTargets& tgt,
                  const CanalParams& p) {
    double xQ = (s.Q - tgt.Q_star) / tgt.Q_star;
    double xC = (s.C - tgt.C_star) / tgt.C_star;
    double xT = (s.T - tgt.T_star) / tgt.T_star;
    return 0.5 * (p.a_Q * xQ * xQ + p.a_C * xC * xC + p.a_T * xT * xT);
}

// LQR controller: u = -K x (simplified, same gain for pump and gate).
void lqr_control(const CanalState& s,
                 const CanalTargets& tgt,
                 const LqrGains& K,
                 double& u_p,
                 double& u_g) {
    double xQ = (s.Q - tgt.Q_star) / tgt.Q_star;
    double xC = (s.C - tgt.C_star) / tgt.C_star;
    double xT = (s.T - tgt.T_star) / tgt.T_star;

    double u = -K.K_Q * xQ - K.K_C * xC - K.K_T * xT;
    u_p = u;
    u_g = u;
}

// Canal dynamics under given controls.
CanalState canal_dynamics(const CanalState& s,
                          const CanalTargets& tgt,
                          const CanalParams& p,
                          double u_p,
                          double u_g,
                          double t) {
    CanalState ds{};
    ds.Q = -p.alpha_Q * (s.Q - tgt.Q_star) + p.b_p * u_p + p.b_g * u_g;

    double k_inflow_Q = p.k_inflow_base + 0.001 * s.Q;
    double k_flush_Q  = p.k_flush_base + 0.002 * s.Q;
    ds.C = -p.k_decay * s.C + k_inflow_Q * tgt.C_star - k_flush_Q * s.C;

    double k_mix_Q = p.k_mix_base + 0.001 * s.Q;
    double solar = (t >= 6.0 && t <= 18.0)
                   ? p.k_solar_amp * std::sin((t - 6.0) * 3.141592653589793 / 12.0)
                   : 0.0;
    ds.T = -p.k_T * (s.T - tgt.T_star)
           + solar
           - k_mix_Q * (s.T - tgt.T_ambient);
    return ds;
}

// Actuation gate: check Lyapunov constraints and block unsafe commands.
bool actuation_allowed(const CanalState& s,
                       const CanalTargets& tgt,
                       const CanalParams& p,
                       const GovernanceParams& gv,
                       double u_p,
                       double u_g,
                       double t,
                       double dt,
                       double current_V) {
    // Predict next state and Lyapunov drift.
    CanalState ds = canal_dynamics(s, tgt, p, u_p, u_g, t);
    CanalState s_next = s;
    s_next.Q += ds.Q * dt;
    s_next.C += ds.C * dt;
    s_next.T += ds.T * dt;

    double V_next = lyapunov_V(s_next, tgt, p);
    double delta_V = V_next - current_V;

    if (V_next > gv.V_cap) {
        return false;
    }
    if (delta_V > gv.delta_V_max) {
        return false;
    }
    return true;
}

// Harness: simulate canal reach with hybrid LQR/Lyapunov supervisory controller.
void run_hybrid_controller_test() {
    CanalState s{110.0, 1.6, 307.0};
    CanalTargets tgt{100.0, 1.0, 300.0, 298.0};
    CanalParams p{};
    p.a_Q = 1.0;
    p.a_C = 2.0;
    p.a_T = 1.5;

    p.alpha_Q = 0.3;
    p.b_p = 1.0;
    p.b_g = 0.5;

    p.k_decay = 0.05;
    p.k_inflow_base = 0.02;
    p.k_flush_base = 0.03;

    p.k_T = 0.1;
    p.k_mix_base = 0.02;
    p.k_solar_amp = 0.05;

    LqrGains K{};
    K.K_Q = 0.8;
    K.K_C = 0.4;
    K.K_T = 0.3;

    GovernanceParams gv{};
    gv.V_cap = 1.0;
    gv.delta_V_max = 0.05;

    double t = 0.0;
    double dt = 0.5;
    double t_end = 24.0;

    double V = lyapunov_V(s, tgt, p);

    std::cout << "LQR/Lyapunov supervisory controller integration test:\n";
    while (t <= t_end) {
        double u_p = 0.0, u_g = 0.0;
        lqr_control(s, tgt, K, u_p, u_g);

        bool allowed = actuation_allowed(s, tgt, p, gv, u_p, u_g, t, dt, V);

        std::cout << "t=" << t
                  << " Q=" << s.Q
                  << " C=" << s.C
                  << " T=" << s.T
                  << " V=" << V
                  << " u_p=" << u_p
                  << " u_g=" << u_g
                  << " actuation=" << (allowed ? "ALLOWED" : "BLOCKED")
                  << "\n";

        if (allowed) {
            CanalState ds = canal_dynamics(s, tgt, p, u_p, u_g, t);
            s.Q += ds.Q * dt;
            s.C += ds.C * dt;
            s.T += ds.T * dt;
            V = lyapunov_V(s, tgt, p);
        } else {
            // If blocked, we skip control (hold state or apply safe fallback).
            // For this harness, we keep state constant to show blocking behavior.
        }

        t += dt;
    }
}

} // namespace eco

int main() {
    using namespace eco;
    run_hybrid_controller_test();
    return 0;
}
