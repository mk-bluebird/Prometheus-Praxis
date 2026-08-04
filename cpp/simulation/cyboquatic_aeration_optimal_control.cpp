// File: cpp/simulation/cyboquatic_aeration_optimal_control.cpp
#include <iostream>
#include <vector>
#include <cmath>
#include <iomanip>

/**
 * Cyboquatic aeration optimal control for maximum carbon negativity
 * under time-varying grid carbon intensity, discretised from a
 * calculus-of-variations formulation.
 *
 * Continuous-time problem:
 *
 * Let:
 *   u(t)   : aeration control (0..1, fraction of max aeration).
 *   B(t)   : BOD concentration state.
 *   ker_e(t): eco-impact rate (kg CO2-eq/s), negative is better.
 *   E(t)   : energy_cost(t) (J/s), proportional to u(t).
 *   c_grid(t): grid_carbon_intensity(t) (kg CO2-eq/J), time-varying.
 *
 * Objective functional:
 *
 *   J[u] = ∫_0^T [ ker_e(t) + λ E(t) c_grid(t) ] dt
 *
 * subject to BOD dynamics:
 *
 *   dB/dt = -k(T(t)) B(t) + q_in(t),
 *
 * where k(T(t)) is temperature-dependent decay and q_in(t) is inflow loading.
 * Aeration u(t) affects ker_e(t) and energy E(t); we focus on E(t) and
 * assume ker_e(t) is a known function of B(t) and u(t).
 *
 * For demonstration, we discretise the Euler-Lagrange-like conditions for u(t)
 * using finite differences for model predictive control (MPC).
 */

struct AerationState {
    double t;          // time (s)
    double B;          // BOD (mg/L)
    double u;          // aeration control (0..1)
    double ker_e;      // eco-impact rate (kg CO2-eq/s)
    double energy_W;   // power (J/s)
    double c_grid;     // grid carbon intensity (kg CO2-eq/J)
};

class AerationOptimalControl {
public:
    AerationOptimalControl(double k20_per_s,
                           double theta_coeff,
                           double lambda_weight,
                           double P_max_W)
        : k20_(k20_per_s),
          theta_(theta_coeff),
          lambda_(lambda_weight),
          P_max_(P_max_W)
    {}

    // Temperature-dependent decay k(T).
    double k_of_T(double T_c) const {
        return k20_ * std::pow(theta_, T_c - 20.0);
    }

    // Simple ker_e model: eco-impact rate proportional to -B (more BOD removal is better).
    double ker_e_rate(double B_mgL) const {
        // Scale mg/L to kg CO2-eq/s proxy.
        const double scale = -1e-6;
        return scale * B_mgL;
    }

    // Energy cost: E(t) = P_max * u(t).
    double energy_cost(double u) const {
        return P_max_ * u;
    }

    // Discretised Euler-Lagrange update for u over horizon [0,T] with step dt.
    // For simplicity, we perform a gradient descent step on u at each time slice
    // based on the discrete derivative of J w.r.t u.
    void optimise(std::vector<AerationState>& states, double dt, int iterations) const {
        const double alpha = 1e-4; // step size for gradient descent on u

        for (int it = 0; it < iterations; ++it) {
            // Forward simulate B and cost contributions.
            for (size_t i = 0; i < states.size(); ++i) {
                AerationState& s = states[i];
                double T_c = 20.0; // assume constant 20°C for this example
                double k = k_of_T(T_c);
                double q_in = 0.0; // no incoming BOD for simplicity

                if (i == 0) {
                    // B initial handled externally.
                } else {
                    double B_prev = states[i - 1].B;
                    // Explicit Euler for BOD:
                    s.B = B_prev + dt * (-k * B_prev + q_in);
                }

                s.ker_e = ker_e_rate(s.B);
                s.energy_W = energy_cost(s.u);
            }

            // Gradient of J with respect to u_i:
            // For each time slice i, approximate:
            //   ∂J/∂u_i ≈ ∂ker_e/∂u_i * dt + λ * ∂(E c_grid)/∂u_i * dt.
            // Here, we assume ker_e does not directly depend on u (only via B),
            // and treat the energy term as the dominant controllable part:
            //   ∂J/∂u_i ≈ λ * P_max * c_grid_i * dt.
            for (size_t i = 0; i < states.size(); ++i) {
                AerationState& s = states[i];
                double dJ_du = lambda_ * P_max_ * s.c_grid * dt;
                // Gradient descent update:
                s.u -= alpha * dJ_du;
                if (s.u < 0.0) s.u = 0.0;
                if (s.u > 1.0) s.u = 1.0;
            }
        }
    }

private:
    double k20_;
    double theta_;
    double lambda_;
    double P_max_;
};

int main() {
    double T = 3600.0;  // 1-hour horizon
    double dt = 60.0;   // 1-minute time step
    int N = static_cast<int>(T / dt);

    AerationOptimalControl controller(
        1e-5,  // k20_per_s (example)
        1.047, // theta_coeff
        1.0,   // lambda_weight
        5000.0 // P_max_W
    );

    std::vector<AerationState> states;
    states.reserve(N);
    double B0 = 10.0; // initial BOD mg/L
    for (int i = 0; i < N; ++i) {
        AerationState s{};
        s.t = i * dt;
        s.B = (i == 0) ? B0 : B0;
        s.u = 0.5; // initial aeration
        s.c_grid = (i < N / 2) ? 0.4 : 0.1; // higher carbon intensity first half, lower second half
        states.push_back(s);
    }

    controller.optimise(states, dt, 50);

    std::cout << std::fixed << std::setprecision(4);
    for (const auto& s : states) {
        std::cout << "t=" << s.t
                  << " B=" << s.B
                  << " u=" << s.u
                  << " c_grid=" << s.c_grid
                  << " energy_W=" << s.energy_W
                  << " ker_e=" << s.ker_e
                  << "\n";
    }

    return 0;
}

/*
Continuous-time calculus of variations summary (conceptual, not executed):

We define the Lagrangian:

    L(B, u, t) = ker_e(B, u, t) + λ E(u, t) c_grid(t),

with state dynamics:

    dB/dt = f(B, u, t) = -k(T(t)) B + q_in(t).

Treat u(t) as control and B(t) as state; the Euler-Lagrange-like optimality
conditions in the Pontryagin framework involve costate p(t):

    dB/dt = ∂H/∂p,
    dp/dt = -∂H/∂B,
    ∂H/∂u = 0,

where H is Hamiltonian:

    H(B, u, p, t) = L(B, u, t) + p f(B, u, t).

This yields:

    ∂H/∂u = ∂L/∂u + p ∂f/∂u = 0,

giving the optimality condition for u(t). In the simplified model where
ker_e does not depend directly on u and f does not depend on u, we get:

    ∂L/∂u = λ ∂(E c_grid)/∂u = 0,

which guides energy minimisation under carbon intensity. More complete
models would include explicit dependence of B dynamics on aeration u.
*/
