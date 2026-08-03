// File: cpp/simulation/canal_lyapunov_opengl_visualiser.cpp
#include <iostream>
#include <vector>
#include <string>
#include <cmath>

// NOTE: This is a self-contained numerical core for a Lyapunov visualiser.
// In a real build, you would link against OpenGL/GLFW/SDL and replace the
// placeholder "render" calls with actual OpenGL drawing using canal state
// and Lyapunov trajectories. Here we compute trajectories and print values,
// ready to be wired into an OpenGL rendering loop.

namespace eco {

struct CanalState {
    double Q;
    double C;
    double T;
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

struct TrajectoryPoint {
    double t;
    CanalState state;
    double V;
};

// Quadratic Lyapunov function V(Q,C,T).
double lyapunov_V(const CanalState& s,
                  const CanalTargets& tgt,
                  const CanalParams& p) {
    double xQ = (s.Q - tgt.Q_star) / tgt.Q_star;
    double xC = (s.C - tgt.C_star) / tgt.C_star;
    double xT = (s.T - tgt.T_star) / tgt.T_star;
    return 0.5 * (p.a_Q * xQ * xQ + p.a_C * xC * xC + p.a_T * xT * xT);
}

// Canal dynamics with simple control policy (as in earlier Lyapunov controller).
CanalState canal_dynamics(const CanalState& s,
                          const CanalTargets& tgt,
                          const CanalParams& p,
                          double t) {
    CanalState ds{};
    double xQ = (s.Q - tgt.Q_star) / tgt.Q_star;

    // Hydraulic dynamics
    double u_p = -0.5 * xQ;
    double u_g = -0.3 * xQ;
    ds.Q = -p.alpha_Q * (s.Q - tgt.Q_star) + p.b_p * u_p + p.b_g * u_g;

    // Pollution dynamics
    double k_inflow_Q = p.k_inflow_base + 0.001 * s.Q;
    double k_flush_Q  = p.k_flush_base + 0.002 * s.Q;
    ds.C = -p.k_decay * s.C + k_inflow_Q * tgt.C_star - k_flush_Q * s.C;

    // Temperature dynamics
    double k_mix_Q = p.k_mix_base + 0.001 * s.Q;
    double solar = (t >= 6.0 && t <= 18.0)
                   ? p.k_solar_amp * std::sin((t - 6.0) * 3.141592653589793 / 12.0)
                   : 0.0;
    ds.T = -p.k_T * (s.T - tgt.T_star)
           + solar
           - k_mix_Q * (s.T - tgt.T_ambient);
    return ds;
}

// Simulate canal state and Lyapunov V over time (for visualisation).
std::vector<TrajectoryPoint> simulate_trajectory(CanalState s0,
                                                 const CanalTargets& tgt,
                                                 const CanalParams& p,
                                                 double t_end,
                                                 double dt) {
    std::vector<TrajectoryPoint> traj;
    double t = 0.0;
    while (t <= t_end) {
        double V = lyapunov_V(s0, tgt, p);
        traj.push_back({t, s0, V});
        CanalState ds = canal_dynamics(s0, tgt, p, t);
        s0.Q += ds.Q * dt;
        s0.C += ds.C * dt;
        s0.T += ds.T * dt;
        t += dt;
    }
    return traj;
}

// Placeholder "render" function: in practice, this would issue OpenGL calls
// to plot Q,C,T and V(t) as lines/points over time.
void render_trajectory(const std::vector<TrajectoryPoint>& traj) {
    std::cout << "Canal Lyapunov trajectory:\n";
    for (const auto& pt : traj) {
        std::cout << "t=" << pt.t
                  << " Q=" << pt.state.Q
                  << " C=" << pt.state.C
                  << " T=" << pt.state.T
                  << " V=" << pt.V << "\n";
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
    p.k_inflow_base = 0.02;
    p.k_flush_base = 0.03;

    p.k_T = 0.1;
    p.k_mix_base = 0.02;
    p.k_solar_amp = 0.05;

    double t_end = 48.0;
    double dt = 0.5;
    auto traj = simulate_trajectory(s0, tgt, p, t_end, dt);
    render_trajectory(traj);

    return 0;
}
