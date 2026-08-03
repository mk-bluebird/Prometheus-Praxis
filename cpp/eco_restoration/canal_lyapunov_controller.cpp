// File: cpp/eco_restoration/canal_lyapunov_controller.cpp
#include <iostream>
#include <cmath>
#include <functional>

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
    std::function<double(double)> k_inflow;
    std::function<double(double)> k_flush;

    double k_T;
    std::function<double(double)> k_mix;
    std::function<double(double)> k_solar;
};

struct CanalControl {
    double u_p; // pump control
    double u_g; // gate/flow control
};

double x_Q(const CanalState& s, const CanalTargets& tgt) {
    return (s.Q - tgt.Q_star) / tgt.Q_star;
}

double x_C(const CanalState& s, const CanalTargets& tgt) {
    return (s.C - tgt.C_star) / tgt.C_star;
}

double x_T(const CanalState& s, const CanalTargets& tgt) {
    return (s.T - tgt.T_star) / tgt.T_star;
}

double lyapunov_V(const CanalState& s,
                  const CanalTargets& tgt,
                  const CanalParams& p) {
    double xq = x_Q(s, tgt);
    double xc = x_C(s, tgt);
    double xt = x_T(s, tgt);
    return 0.5 * (p.a_Q * xq * xq + p.a_C * xc * xc + p.a_T * xt * xt);
}

// Continuous-time dynamics (ODE right-hand side) for (Q, C, T).
CanalState canal_dynamics(const CanalState& s,
                          const CanalTargets& tgt,
                          const CanalParams& p,
                          const CanalControl& u,
                          double t) {
    CanalState ds;
    double xq = x_Q(s, tgt);
    ds.Q = -p.alpha_Q * (s.Q - tgt.Q_star) + p.b_p * u.u_p + p.b_g * u.u_g;

    double k_in = p.k_inflow(s.Q);
    double k_fl = p.k_flush(s.Q);
    ds.C = -p.k_decay * s.C + k_in * tgt.C_star - k_fl * s.C;

    double k_mix_val = p.k_mix(s.Q);
    double k_solar_val = p.k_solar(t);
    ds.T = -p.k_T * (s.T - tgt.T_star)
           + k_solar_val
           - k_mix_val * (s.T - tgt.T_ambient);

    return ds;
}

// Lyapunov derivative dot V(Q,C,T) under current state and control.
double lyapunov_dotV(const CanalState& s,
                     const CanalTargets& tgt,
                     const CanalParams& p,
                     const CanalControl& u,
                     double t) {
    double xq = x_Q(s, tgt);
    double xc = x_C(s, tgt);
    double xt = x_T(s, tgt);

    double Q_star = tgt.Q_star;
    double C_star = tgt.C_star;
    double T_star = tgt.T_star;

    double term_Q = p.a_Q * xq / Q_star;
    double term_C = p.a_C * xc / C_star;
    double term_T = p.a_T * xt / T_star;

    double dQ = -p.alpha_Q * (s.Q - tgt.Q_star) + p.b_p * u.u_p + p.b_g * u.u_g;

    double k_in = p.k_inflow(s.Q);
    double k_fl = p.k_flush(s.Q);
    double dC = -p.k_decay * s.C + k_in * tgt.C_star - k_fl * s.C;

    double k_mix_val = p.k_mix(s.Q);
    double k_solar_val = p.k_solar(t);
    double dT = -p.k_T * (s.T - tgt.T_star)
                + k_solar_val
                - k_mix_val * (s.T - tgt.T_ambient);

    return term_Q * dQ + term_C * dC + term_T * dT;
}

// Feedback control policy that enforces dot V <= 0 by damping x_Q and
// increasing flushing/mixing when pollution or temperature exceed corridors.
CanalControl lyapunov_feedback(const CanalState& s,
                               const CanalTargets& tgt,
                               const CanalParams& p,
                               double t,
                               double corridor_C_threshold,
                               double corridor_T_threshold) {
    CanalControl u{};
    double xq = x_Q(s, tgt);
    double xc = x_C(s, tgt);
    double xt = x_T(s, tgt);

    // Hydraulic damping: choose controls such that b_p u_p + b_g u_g = -eta * x_Q
    double eta = 0.5;
    double total = -eta * xq;

    // Split between pump and gate (simple proportional split)
    if (std::fabs(p.b_p) > 1e-6 && std::fabs(p.b_g) > 1e-6) {
        double denom = p.b_p + p.b_g;
        if (std::fabs(denom) < 1e-6) denom = 1.0;
        u.u_p = total * (p.b_p / denom);
        u.u_g = total * (p.b_g / denom);
    } else {
        u.u_p = total;
        u.u_g = 0.0;
    }

    // Pollution corridor enforcement: increase Q via gate when C above corridor
    if (xc > corridor_C_threshold) {
        u.u_g += 0.5 * xc;
    }

    // Temperature corridor enforcement: increase Q to enhance mixing when T above corridor
    if (xt > corridor_T_threshold) {
        u.u_g += 0.5 * xt;
    }

    return u;
}

// Simple Euler integrator to demonstrate Lyapunov decrease.
void simulate_canal(double T_end, double dt,
                    CanalState s,
                    const CanalTargets& tgt,
                    const CanalParams& p) {
    double t = 0.0;
    double corridor_C_threshold = 0.1;
    double corridor_T_threshold = 0.1;

    while (t <= T_end) {
        CanalControl u = lyapunov_feedback(s, tgt, p, t,
                                           corridor_C_threshold,
                                           corridor_T_threshold);
        double V = lyapunov_V(s, tgt, p);
        double dV = lyapunov_dotV(s, tgt, p, u, t);

        std::cout << "t=" << t
                  << " Q=" << s.Q
                  << " C=" << s.C
                  << " T=" << s.T
                  << " V=" << V
                  << " dotV=" << dV
                  << "\n";

        CanalState ds = canal_dynamics(s, tgt, p, u, t);
        s.Q += ds.Q * dt;
        s.C += ds.C * dt;
        s.T += ds.T * dt;

        t += dt;
    }
}

} // namespace eco

int main() {
    using namespace eco;

    CanalState s0{100.0, 1.5, 305.0};
    CanalTargets tgt{100.0, 1.0, 300.0, 298.0};

    CanalParams p{};
    p.a_Q = 1.0;
    p.a_C = 2.0;
    p.a_T = 1.5;

    p.alpha_Q = 0.3;
    p.b_p = 1.0;
    p.b_g = 0.5;

    p.k_decay = 0.05;
    p.k_inflow = [](double Q) {
        return 0.02 + 0.001 * Q;
    };
    p.k_flush = [](double Q) {
        return 0.03 + 0.002 * Q;
    };

    p.k_T = 0.1;
    p.k_mix = [](double Q) {
        return 0.02 + 0.001 * Q;
    };
    p.k_solar = [](double t) {
        // Simple diurnal pattern
        double hour = std::fmod(t, 24.0);
        if (hour < 6.0 || hour > 18.0) return 0.0;
        double peak = std::sin((hour - 6.0) * 3.141592653589793 / 12.0);
        return 0.05 * std::max(0.0, peak);
    };

    simulate_canal(/*T_end=*/48.0, /*dt=*/0.5, s0, tgt, p);

    return 0;
}
