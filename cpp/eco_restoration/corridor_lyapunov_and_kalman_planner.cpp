// File: cpp/eco_restoration/corridor_lyapunov_and_kalman_planner.cpp
#include <iostream>
#include <cmath>
#include <iomanip>

namespace praxis {
namespace eco {

// -----------------------------
// Corridor Lyapunov utilities
// -----------------------------

struct CorridorState {
    double T_surface;     // normalized surface temperature [0,1]
    double HII;           // heat-island intensity index [0,1]
    double green_cover;   // green-cover fraction [0,1]
};

struct CorridorTarget {
    double T_target;
    double HII_target;
    double green_target;
};

double clamp01(double x) {
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

// Simple quadratic Lyapunov candidate:
// V(x) = w_T (T - T*)^2 + w_H (HII - HII*)^2 + w_G (g - g*)^2
double corridor_lyapunov(const CorridorState& x,
                          const CorridorTarget& x_star,
                          double w_T, double w_H, double w_G) {
    double dT = x.T_surface   - x_star.T_target;
    double dH = x.HII         - x_star.HII_target;
    double dG = x.green_cover - x_star.green_target;
    return w_T * dT * dT + w_H * dH * dH + w_G * dG * dG;
}

// Simple linear corridor dynamics driven by eco-restoration action s_t:
// x_{t+1} = x_t + A x_t + B s_t
// Here we encode:
// - T_surface decreases with s_t and green_cover.
// - HII decreases with s_t and green_cover.
// - green_cover increases with s_t.
CorridorState corridor_step(const CorridorState& x,
                            double s_t) {
    const double a_T  = 0.02;  // uncontrolled warming per sol
    const double a_H  = 0.015; // uncontrolled HII growth
    const double a_G  = -0.005; // passive green-cover loss

    const double b_Ts = -0.06; // cooling effect per unit s_t
    const double b_Hs = -0.05; // HII mitigation per unit s_t
    const double b_Gs = 0.04;  // green-cover gain per unit s_t

    const double b_Tg = -0.03; // extra cooling from existing green cover
    const double b_Hg = -0.02; // extra HII reduction from green cover

    CorridorState x_next{};
    x_next.T_surface = clamp01(x.T_surface + a_T + b_Ts * s_t + b_Tg * x.green_cover);
    x_next.HII       = clamp01(x.HII       + a_H + b_Hs * s_t + b_Hg * x.green_cover);
    x_next.green_cover = clamp01(x.green_cover + a_G + b_Gs * s_t);

    return x_next;
}

// Compute the minimal alpha so that ΔV <= -alpha * s_t ensures hex-anchored convergence within N sols.
// We approximate ΔV per sol under a fixed action s_t and require V_N <= V_target_tol.
double minimal_alpha_for_horizon(const CorridorState& x0,
                                 const CorridorTarget& x_star,
                                 double w_T, double w_H, double w_G,
                                 double s_t,
                                 int N_sols,
                                 double V_target_tol) {
    // Simulate N_sols with given s_t and record total decrease.
    CorridorState x = x0;
    double V0 = corridor_lyapunov(x0, x_star, w_T, w_H, w_G);
    double V_prev = V0;
    double total_decrease = 0.0;

    for (int k = 0; k < N_sols; ++k) {
        CorridorState x_next = corridor_step(x, s_t);
        double V_next = corridor_lyapunov(x_next, x_star, w_T, w_H, w_G);
        double dV = V_next - V_prev;
        total_decrease += (dV < 0.0 ? -dV : 0.0); // accumulate only decreases
        x = x_next;
        V_prev = V_next;
    }

    double V_N = corridor_lyapunov(x, x_star, w_T, w_H, w_G);

    // Require V_N <= V_target_tol to "reach the hex-anchor corridor".
    if (V_N > V_target_tol) {
        // Under current s_t the corridor does not reach target; alpha must be at least large enough
        // that enforced inequality ΔV <= -alpha*s_t would imply a stronger contraction.
        // For a conservative bound, assume worst-case per-step decrease total_decrease/N_sols.
        double avg_dV = (N_sols > 0 ? total_decrease / N_sols : 0.0);
        // If avg_dV is already > 0, alpha_min ~ avg_dV / s_t.
        double alpha_min = (s_t > 0.0 ? avg_dV / s_t : 0.0);
        return alpha_min;
    }

    // If we already reach the target, compute minimal alpha consistent with observed decreases:
    // We approximate ΔV_k ≈ -alpha*s_t  => alpha ≈ (V_{k} - V_{k+1}) / s_t, and take the minimum over k.
    x = x0;
    V_prev = V0;
    double alpha_min = 1e9;

    for (int k = 0; k < N_sols; ++k) {
        CorridorState x_next = corridor_step(x, s_t);
        double V_next = corridor_lyapunov(x_next, x_star, w_T, w_H, w_G);
        double dV = V_next - V_prev;
        if (dV < 0.0 && s_t > 0.0) {
            double alpha_k = -dV / s_t;
            if (alpha_k < alpha_min) {
                alpha_min = alpha_k;
            }
        }
        x = x_next;
        V_prev = V_next;
    }

    if (alpha_min == 1e9) {
        alpha_min = 0.0;
    }
    return alpha_min;
}

// -----------------------------
// PFAS electrode Kalman tuning
// -----------------------------

// Simple 1D steady-state Kalman filter model for psych-state estimation,
// with scalar process noise Q and measurement noise R.
struct KalmanParams {
    double A;  // state transition coefficient
    double H;  // measurement matrix coefficient
    double Q;  // process noise variance (temperature drift, model error)
    double R;  // measurement noise variance (PFAS electrode calibration)
};

// Compute steady-state error covariance P_inf for scalar case:
// Solve Riccati: P = A^2 P + Q - A^2 P^2 H^2 / (H^2 P + R)
double steady_state_error_covariance(const KalmanParams& kp,
                                     int max_iter = 1000,
                                     double tol = 1e-9) {
    double P = 1.0; // initial guess
    for (int i = 0; i < max_iter; ++i) {
        double S = kp.H * kp.H * P + kp.R;
        double K = (kp.A * P * kp.H) / S;
        double P_next = kp.A * kp.A * P + kp.Q - K * kp.H * kp.A * P;
        if (std::fabs(P_next - P) < tol) {
            P = P_next;
            break;
        }
        P = P_next;
    }
    return P;
}

// Compute steady-state Kalman gain K_inf for 1D psych-state estimate.
double steady_state_gain(const KalmanParams& kp) {
    double P = steady_state_error_covariance(kp);
    double S = kp.H * kp.H * P + kp.R;
    return (kp.A * P * kp.H) / S;
}

// Tune Q and R to keep sqrt(P_inf) <= epsilon across permissible operating conditions.
// We approximate permissible operating conditions as ranges on temperature drift (affecting Q)
// and calibration variance (affecting R).
struct OperatingEnvelope {
    double Q_min;
    double Q_max;
    double R_min;
    double R_max;
};

struct KalmanTuningResult {
    double Q_tuned;
    double R_tuned;
    double K_inf;
    double P_inf;
    bool   within_threshold;
};

KalmanTuningResult tune_kalman_for_threshold(const KalmanParams& base,
                                             const OperatingEnvelope& env,
                                             double epsilon) {
    // Start from midpoints of envelope and adjust Q/R heuristically.
    KalmanParams kp = base;
    kp.Q = 0.5 * (env.Q_min + env.Q_max);
    kp.R = 0.5 * (env.R_min + env.R_max);

    // Heuristic tuning loop: adjust Q and R to push sqrt(P_inf) below epsilon.
    for (int iter = 0; iter < 200; ++iter) {
        double P = steady_state_error_covariance(kp);
        double error_std = std::sqrt(P);

        if (error_std <= epsilon) {
            double K_inf = steady_state_gain(kp);
            return KalmanTuningResult{kp.Q, kp.R, K_inf, P, true};
        }

        // If error is too high, we can:
        // - decrease R (trust PFAS electrode measurements more),
        // - increase Q slightly to let the filter follow measurements.
        kp.R = std::max(env.R_min, kp.R * 0.8);
        kp.Q = std::min(env.Q_max, kp.Q * 1.2);
    }

    double P_final = steady_state_error_covariance(kp);
    double K_inf_final = steady_state_gain(kp);
    bool ok = std::sqrt(P_final) <= epsilon;
    return KalmanTuningResult{kp.Q, kp.R, K_inf_final, P_final, ok};
}

// -----------------------------
// Demonstration main
// -----------------------------

int main() {
    // 1. Corridor Lyapunov: Phoenix heat-island corridor over 180 sols.
    CorridorState x0{0.80, 0.75, 0.20}; // hot surface, high HII, low green cover
    CorridorTarget x_star{0.40, 0.30, 0.60}; // hex-anchor targets
    double w_T = 1.0;
    double w_H = 0.8;
    double w_G = 0.6;

    double s_t = 0.7;        // strong eco-restoration action per sol (green corridors, albedo changes)
    int N_sols = 180;
    double V_tol = 0.02;     // tolerance near target

    double alpha_min = minimal_alpha_for_horizon(x0, x_star, w_T, w_H, w_G,
                                                 s_t, N_sols, V_tol);

    std::cout << std::fixed << std::setprecision(4);
    std::cout << "Corridor Lyapunov planning over " << N_sols << " sols:\n";
    std::cout << "  Minimal alpha consistent with ΔV <= -alpha*s_t and reaching target: "
              << alpha_min << "\n\n";

    // 2. PFAS electrode Kalman tuning: psych-state estimation error threshold epsilon=0.05.
    KalmanParams base_params{
        1.0,   // A: we track a slowly varying psych-state
        1.0,   // H: direct measurement of state
        0.001, // Q: initial guess for process noise (temperature drift)
        0.01   // R: initial measurement noise (PFAS electrode calibration variance)
    };

    OperatingEnvelope env{
        0.0005, // Q_min
        0.005,  // Q_max
        0.005,  // R_min
        0.05    // R_max
    };

    double epsilon = 0.05;

    KalmanTuningResult res = tune_kalman_for_threshold(base_params, env, epsilon);

    std::cout << "PFAS electrode Kalman tuning for psych-state estimation:\n";
    std::cout << "  Q_tuned=" << res.Q_tuned
              << ", R_tuned=" << res.R_tuned << "\n";
    std::cout << "  Steady-state gain K_inf=" << res.K_inf
              << ", error std sqrt(P_inf)=" << std::sqrt(res.P_inf) << "\n";
    std::cout << "  Within continuity threshold epsilon=" << epsilon
              << "? " << (res.within_threshold ? "YES" : "NO") << "\n";

    return 0;
}

} // namespace eco
} // namespace praxis
