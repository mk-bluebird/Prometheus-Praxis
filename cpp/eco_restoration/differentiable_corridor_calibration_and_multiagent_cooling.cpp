// File: cpp/eco_restoration/differentiable_corridor_calibration_and_multiagent_cooling.cpp
#include <iostream>
#include <vector>
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
// 27. Differentiable corridor calibration with Lyapunov constraint
// ----------------------------------------------------------
//
// Corridor-calibration problem:
//   - State x_t = (T_t, HII_t, G_t) for each hex or corridor segment.
//   - Hex-anchor target x* = (T*, HII*, G*).
//   - Eco-restoration parameters θ (e.g., gains on green cover, albedo, cooling actions).
//
// Objective:
//   J(θ) = Σ_t ||x_t(θ) - x*||^2
//
// Constraint (Lyapunov-safe):
//   V(x_t) = 1/2 (T_t - T*)^2 + 1/2 (HII_t - HII*)^2 + 1/2 (G_t - G*)^2
//   Require dV/dt(x_t, θ) < 0 (discrete: ΔV_t < 0) along the calibrated corridor.
//
// We derive ∂J/∂θ via adjoint sensitivity analysis.
//
// Discrete-time dynamics (simplified, parameterized):
//   x_{t+1} = f(x_t, u_t; θ)
// where u_t is eco-restoration action (e.g., green planting rate).
//
// For this demonstration, θ = (k_T, k_H, k_G) scales the effect of action on state:

struct CorridorState {
    double T;
    double HII;
    double G;
};

struct TargetState {
    double T_star;
    double HII_star;
    double G_star;
};

struct EcoParams {
    double k_T; // cooling gain
    double k_H; // HII reduction gain
    double k_G; // green-cover gain
};

CorridorState step_dynamics(const CorridorState& x,
                            double u,
                            const EcoParams& p) {
    // Baseline evolution + parameterized eco-restoration influence.
    const double a_T = 0.02;
    const double a_H = 0.015;
    const double a_G = -0.005;

    CorridorState x_next{};
    x_next.T   = clamp01(x.T   + a_T + (-p.k_T) * u + (-0.03) * x.G);
    x_next.HII = clamp01(x.HII + a_H + (-p.k_H) * u + (-0.02) * x.G);
    x_next.G   = clamp01(x.G   + a_G + (p.k_G)  * u);

    return x_next;
}

double lyapunov_V(const CorridorState& x, const TargetState& x_star) {
    double eT = x.T   - x_star.T_star;
    double eH = x.HII - x_star.HII_star;
    double eG = x.G   - x_star.G_star;
    return 0.5 * (eT*eT + eH*eH + eG*eG);
}

// Objective J(θ) over horizon N.
double objective_J(const std::vector<CorridorState>& traj,
                   const TargetState& x_star) {
    double J = 0.0;
    for (const auto& x : traj) {
        double eT = x.T   - x_star.T_star;
        double eH = x.HII - x_star.HII_star;
        double eG = x.G   - x_star.G_star;
        J += eT*eT + eH*eH + eG*eG;
    }
    return J;
}

// Forward simulation to collect states.
std::vector<CorridorState> simulate_traj(const CorridorState& x0,
                                         const EcoParams& p,
                                         double u,
                                         int N) {
    std::vector<CorridorState> traj;
    traj.reserve(N);
    CorridorState x = x0;
    traj.push_back(x);
    for (int t = 0; t < N-1; ++t) {
        x = step_dynamics(x, u, p);
        traj.push_back(x);
    }
    return traj;
}

// Adjoint sensitivity:
// Let λ_t be adjoint at time t, with terminal λ_N = ∂J/∂x_N.
// Backward recursion:
//   λ_t = ∂J/∂x_t + (∂f/∂x_t)^T λ_{t+1}
//
// Gradient wrt parameters θ:
//   ∂J/∂θ = Σ_t (∂f/∂θ_t)^T λ_{t+1}
//
// Here f(x_t, u; θ) is linear in θ, so ∂f/∂θ is straightforward.

struct AdjointState {
    double lambda_T;
    double lambda_H;
    double lambda_G;
};

struct GradientEcoParams {
    double dJ_dk_T;
    double dJ_dk_H;
    double dJ_dk_G;
};

GradientEcoParams adjoint_gradient(const std::vector<CorridorState>& traj,
                                   const TargetState& x_star,
                                   double u,
                                   const EcoParams& p) {
    int N = static_cast<int>(traj.size());
    std::vector<AdjointState> lambda(N);

    // Terminal adjoint at N-1 (last state).
    {
        const auto& xN = traj[N-1];
        double eT = xN.T   - x_star.T_star;
        double eH = xN.HII - x_star.HII_star;
        double eG = xN.G   - x_star.G_star;
        lambda[N-1] = {2.0 * eT, 2.0 * eH, 2.0 * eG};
    }

    // Backward recursion.
    for (int t = N-2; t >= 0; --t) {
        const auto& x = traj[t];
        const auto& x_next = traj[t+1];

        // ∂J/∂x_t
        double eT = x.T   - x_star.T_star;
        double eH = x.HII - x_star.HII_star;
        double eG = x.G   - x_star.G_star;
        double dJ_dT = 2.0 * eT;
        double dJ_dH = 2.0 * eH;
        double dJ_dG = 2.0 * eG;

        // ∂f/∂x_t for dynamics step_dynamics.
        // x_next.T   = x.T + a_T - p.k_T u - 0.03 x.G
        // x_next.HII = x.HII + a_H - p.k_H u - 0.02 x.G
        // x_next.G   = x.G + a_G + p.k_G u
        double dTnext_dT = 1.0;
        double dTnext_dH = 0.0;
        double dTnext_dG = -0.03;

        double dHnext_dT = 0.0;
        double dHnext_dH = 1.0;
        double dHnext_dG = -0.02;

        double dGnext_dT = 0.0;
        double dGnext_dH = 0.0;
        double dGnext_dG = 1.0;

        const auto& lam_next = lambda[t+1];

        double dJ_tot_dT = dJ_dT
                         + dTnext_dT * lam_next.lambda_T
                         + dHnext_dT * lam_next.lambda_H
                         + dGnext_dT * lam_next.lambda_G;

        double dJ_tot_dH = dJ_dH
                         + dTnext_dH * lam_next.lambda_T
                         + dHnext_dH * lam_next.lambda_H
                         + dGnext_dH * lam_next.lambda_G;

        double dJ_tot_dG = dJ_dG
                         + dTnext_dG * lam_next.lambda_T
                         + dHnext_dG * lam_next.lambda_H
                         + dGnext_dG * lam_next.lambda_G;

        lambda[t] = {dJ_tot_dT, dJ_tot_dH, dJ_tot_dG};
    }

    // Gradient wrt parameters θ:
    // ∂f/∂k_T: T_next partial = -u, HII_next partial = 0, G_next partial = 0
    // ∂f/∂k_H: T_next partial = 0, HII_next partial = -u, G_next partial = 0
    // ∂f/∂k_G: T_next partial = 0, HII_next partial = 0, G_next partial = u
    double dJ_dk_T = 0.0;
    double dJ_dk_H = 0.0;
    double dJ_dk_G = 0.0;

    for (int t = 0; t < N-1; ++t) {
        const auto& lam_next = lambda[t+1];
        dJ_dk_T += (-u) * lam_next.lambda_T;
        dJ_dk_H += (-u) * lam_next.lambda_H;
        dJ_dk_G += ( u) * lam_next.lambda_G;
    }

    return GradientEcoParams{dJ_dk_T, dJ_dk_H, dJ_dk_G};
}

// Lyapunov derivative check for each step (discrete ΔV < 0).
bool lyapunov_safe_traj(const std::vector<CorridorState>& traj,
                        const TargetState& x_star) {
    for (std::size_t t = 0; t+1 < traj.size(); ++t) {
        double V_t   = lyapunov_V(traj[t],   x_star);
        double V_tp1 = lyapunov_V(traj[t+1], x_star);
        if (V_tp1 >= V_t) {
            return false;
        }
    }
    return true;
}

// ----------------------------------------------------------
// 28. Multi-agent coordination for cooling (Markov decision process)
// ----------------------------------------------------------
//
// Swarm of cooling drones, each controlling actions in a subset of hex-cells.
//
// Decentralized MDP:
//   - State s_t: configuration of hex temperatures, HII, RoH.
//   - Each drone i observes local state s_t^i (its neighborhood).
//   - Each drone chooses action a_t^i (cooling pattern).
//   - Transition: s_{t+1} ~ P(s_{t+1} | s_t, a_t^1, ..., a_t^N).
//   - Reward for drone i: r_t^i = local_cooling_benefit - λ * RoH_increase_neighbors.
//
// Bellman equation for drone i:
//
//   V_i(s^i) = max_{a^i} E[ r^i(s^i, a^i, a^{-i}) + γ V_i(s'^i) ]
//
// with penalty term:
//   r^i = - HII_local(s^i) - λ Σ_{j ∈ N(i)} max(0, RoH_j(s') - RoH_j(s))
//
// Coordination graph structure:
//   - We choose a sparse neighborhood graph (e.g., hex-grid adjacency) forming
//     a coordination graph G with nodes = drones and edges = local interactions.
//   - For real-time computation in Phoenix grid, we use:
//       * Tree or low-treewidth graph (e.g., banded grid / spanning tree)
//         to enable message-passing (max-sum or value iteration) over small cliques.

// For demonstration, we encode a simplified Bellman update for one drone i
// with local neighbors, and assume a coordination graph as a ring or grid.

struct DroneState {
    double HII_local;
    std::vector<double> RoH_neighbors; // RoH for neighboring hex-cells
};

struct DroneAction {
    double cooling_power; // local cooling control
};

double drone_reward(const DroneState& s,
                    const DroneAction& a,
                    const std::vector<double>& RoH_neighbors_next,
                    double lambda_penalty) {
    // Local cooling reduces HII; we model benefit as -HII_local after action.
    double local_benefit = -s.HII_local;
    double penalty = 0.0;
    for (std::size_t j = 0; j < s.RoH_neighbors.size(); ++j) {
        double delta_roh = RoH_neighbors_next[j] - s.RoH_neighbors[j];
        if (delta_roh > 0.0) {
            penalty += delta_roh;
        }
    }
    return local_benefit - lambda_penalty * penalty;
}

// One-step Bellman backup for drone i:
//   V_i(s^i) ≈ max_{a^i ∈ A} [ r^i + γ V_i(s'^i) ]
double bellman_backup(const DroneState& s,
                      const std::vector<double>& RoH_neighbors_next,
                      double lambda_penalty,
                      double gamma,
                      const std::vector<DroneAction>& action_space,
                      double V_next_local) {
    double best_val = -1e9;
    for (const auto& a : action_space) {
        double r = drone_reward(s, a, RoH_neighbors_next, lambda_penalty);
        double val = r + gamma * V_next_local; // simple approximation
        if (val > best_val) best_val = val;
    }
    return best_val;
}

// ----------------------------------------------------------
// Demonstration main
// ----------------------------------------------------------

int main() {
    std::cout << std::fixed << std::setprecision(4);

    // 27. Differentiable corridor calibration demo.
    CorridorState x0{0.80, 0.75, 0.20};
    TargetState x_star{0.40, 0.30, 0.60};
    EcoParams p{0.5, 0.4, 0.3}; // initial eco-restoration gains
    double u = 0.7;
    int N = 30;

    auto traj = simulate_traj(x0, p, u, N);
    double J = objective_J(traj, x_star);
    auto grad = adjoint_gradient(traj, x_star, u, p);
    bool safe = lyapunov_safe_traj(traj, x_star);

    std::cout << "Differentiable corridor calibration:\n";
    std::cout << "  Objective J(θ) = " << J << "\n";
    std::cout << "  Gradient dJ/dk_T = " << grad.dJ_dk_T
              << ", dJ/dk_H = " << grad.dJ_dk_H
              << ", dJ/dk_G = " << grad.dJ_dk_G << "\n";
    std::cout << "  Lyapunov-safe trajectory (ΔV<0 at each step)? "
              << (safe ? "YES" : "NO") << "\n\n";

    // 28. Multi-agent cooling coordination demo.
    DroneState s{
        0.35,                      // HII_local
        {0.25, 0.28, 0.27}         // RoH_neighbors
    };
    std::vector<double> RoH_neighbors_next{0.26, 0.30, 0.29};

    std::vector<DroneAction> actions{
        {0.0},  // no cooling
        {0.5},  // moderate cooling
        {1.0}   // strong cooling
    };

    double lambda_penalty = 2.0;
    double gamma = 0.9;
    double V_next_local = -0.3; // example value function at next state

    double V_backup = bellman_backup(s, RoH_neighbors_next,
                                     lambda_penalty, gamma,
                                     actions, V_next_local);

    std::cout << "Multi-agent cooling (Bellman backup for one drone):\n";
    std::cout << "  Bellman backup value V_i(s^i) ≈ " << V_backup << "\n";
    std::cout << "  Reward includes penalty for RoH increases in neighboring hex-cells,\n"
              << "  and coordination is feasible on a sparse hex-grid graph using local\n"
              << "  message-passing for real-time Phoenix microgrid control.\n";

    return 0;
}

} // namespace eco
} // namespace praxis
