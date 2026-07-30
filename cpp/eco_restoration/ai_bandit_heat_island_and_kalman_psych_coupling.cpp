// File: cpp/eco_restoration/ai_bandit_heat_island_and_kalman_psych_coupling.cpp
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
// 11. AI-workload bandit model for heat-island recovery
// ----------------------------------------------------------

// Each arm = edge node in Phoenix.
// Reward = reduction in local heat-island index (HII_delta).
// Constraint: instantaneous RoH rise due to compute heat must not exceed RoH_max_delta.

struct BanditArm {
    std::string name;
    double      HII_reduction_estimate; // current mean reward estimate (delta HII per unit load)
    double      RoH_heat_slope;         // RoH increase per unit load
    int         pulls;                  // number of times this arm has been selected
};

struct BanditContext {
    std::vector<BanditArm> arms;
    double                 RoH_max_delta; // hard constraint on allowed RoH rise
    double                 total_load;    // total workload units allocated
};

// UCB-style index with RoH constraint.
// We treat load unit = 1 for simplicity.
int select_arm_with_constraint(const BanditContext& ctx, double time_step) {
    double log_term = std::log(std::max(1.0, time_step));
    int best_idx = -1;
    double best_index = -1e9;

    for (std::size_t i = 0; i < ctx.arms.size(); ++i) {
        const BanditArm& arm = ctx.arms[i];

        // Future RoH increase if we assign one more unit to this arm.
        double roh_increase = arm.RoH_heat_slope;
        if (roh_increase > ctx.RoH_max_delta) {
            // Hard constraint: never pick arms that would violate RoH limit for a unit of load.
            continue;
        }

        double mean = arm.HII_reduction_estimate;
        double bonus = std::sqrt(2.0 * log_term / std::max(1, arm.pulls));
        double index = mean + bonus;

        if (index > best_index) {
            best_index = index;
            best_idx = static_cast<int>(i);
        }
    }

    return best_idx;
}

// Update rule: after assigning load to an arm and observing realized reward,
// we update the arm's mean reward estimate and pulls counter.
//
// This is standard incremental-mean UCB:
//   μ_i(new) = μ_i(old) + (reward - μ_i(old)) / pulls_i(new)
//
// With the RoH constraint enforced at selection, we minimize regret under
// safety constraints by only exploring arms that respect the thermal corridor.
void update_arm(BanditArm& arm, double observed_reward) {
    arm.pulls += 1;
    double delta = (observed_reward - arm.HII_reduction_estimate) / static_cast<double>(arm.pulls);
    arm.HII_reduction_estimate += delta;
}

// ----------------------------------------------------------
// 12. Kalman-psych coupling equation (joint linear system)
// ----------------------------------------------------------

// Joint state x_t = [p_t, c_t]^T:
//   p_t : psych-state scalar
//   c_t : electrode calibration scalar
//
// Dynamics:
//   x_{t+1} = A x_t + w_t
//   y_t     = H x_t + v_t
//
// where A has non-diagonal coupling:
//   A = [ a_pp  a_pc ]
//       [ a_cp  a_cc ]
//
// and H may observe both p_t and c_t.
//
// Joint Kalman filter:
//   Prediction:
//     x_pred = A x_prev
//     P_pred = A P_prev A^T + Q
//   Update:
//     S = H P_pred H^T + R
//     K = P_pred H^T S^{-1}
//     x_new = x_pred + K (y_t - H x_pred)
//     P_new = (I - K H) P_pred
//
// Cramér–Rao lower bound (CRLB) for p_t estimation given fixed calibration update interval
// is lower-bounded by the corresponding element of the joint error covariance P_t,
// specifically:
//   Var_hat(p_t) >= [P_t]_{0,0}
//
// Under steady-state conditions and periodic calibration updates, P_t converges to P_inf,
// so CRLB ≈ [P_inf]_{0,0}.

struct JointKalmanParams {
    double a_pp;
    double a_pc;
    double a_cp;
    double a_cc;

    double h_p;
    double h_c;

    double q_pp;
    double q_pc;
    double q_cp;
    double q_cc;

    double r_yy;
};

struct JointState {
    double p;
    double c;
};

struct Covariance2x2 {
    double p11;
    double p12;
    double p21;
    double p22;
};

struct JointKalmanResult {
    JointState   state;
    Covariance2x2 cov;
};

JointKalmanResult joint_kalman_step(const JointKalmanParams& params,
                                    const JointState& prev_state,
                                    const Covariance2x2& prev_cov,
                                    double y_t) {
    // Build A and H.
    double A11 = params.a_pp;
    double A12 = params.a_pc;
    double A21 = params.a_cp;
    double A22 = params.a_cc;

    double H1 = params.h_p;
    double H2 = params.h_c;

    // Prediction.
    JointState x_pred{
        A11 * prev_state.p + A12 * prev_state.c,
        A21 * prev_state.p + A22 * prev_state.c
    };

    Covariance2x2 P_pred{};
    P_pred.p11 = A11 * (A11 * prev_cov.p11 + A21 * prev_cov.p21) +
                 A12 * (A11 * prev_cov.p12 + A21 * prev_cov.p22) +
                 params.q_pp;
    P_pred.p12 = A11 * (A12 * prev_cov.p11 + A22 * prev_cov.p21) +
                 A12 * (A12 * prev_cov.p12 + A22 * prev_cov.p22) +
                 params.q_pc;
    P_pred.p21 = A21 * (A11 * prev_cov.p11 + A21 * prev_cov.p21) +
                 A22 * (A11 * prev_cov.p12 + A21 * prev_cov.p22) +
                 params.q_cp;
    P_pred.p22 = A21 * (A12 * prev_cov.p11 + A22 * prev_cov.p21) +
                 A22 * (A12 * prev_cov.p12 + A22 * prev_cov.p22) +
                 params.q_cc;

    // Innovation covariance S (scalar).
    double S = H1 * (H1 * P_pred.p11 + H2 * P_pred.p21) +
               H2 * (H1 * P_pred.p12 + H2 * P_pred.p22) +
               params.r_yy;

    // Kalman gain K (2x1).
    double K1 = (P_pred.p11 * H1 + P_pred.p12 * H2) / S;
    double K2 = (P_pred.p21 * H1 + P_pred.p22 * H2) / S;

    // Innovation.
    double y_pred = H1 * x_pred.p + H2 * x_pred.c;
    double innov  = y_t - y_pred;

    // Update state.
    JointState x_new{
        x_pred.p + K1 * innov,
        x_pred.c + K2 * innov
    };

    // Update covariance: P_new = (I - K H) P_pred
    // (I - K H) = [[1 - K1*H1,      -K1*H2],
    //              [   -K2*H1, 1 - K2*H2]]
    double I11 = 1.0 - K1 * H1;
    double I12 = -K1 * H2;
    double I21 = -K2 * H1;
    double I22 = 1.0 - K2 * H2;

    Covariance2x2 P_new{};
    P_new.p11 = I11 * (P_pred.p11 * I11 + P_pred.p21 * I21) +
                I12 * (P_pred.p11 * I12 + P_pred.p21 * I22);
    P_new.p12 = I11 * (P_pred.p12 * I11 + P_pred.p22 * I21) +
                I12 * (P_pred.p12 * I12 + P_pred.p22 * I22);
    P_new.p21 = I21 * (P_pred.p11 * I11 + P_pred.p21 * I21) +
                I22 * (P_pred.p11 * I12 + P_pred.p21 * I22);
    P_new.p22 = I21 * (P_pred.p12 * I11 + P_pred.p22 * I21) +
                I22 * (P_pred.p12 * I12 + P_pred.p22 * I22);

    return JointKalmanResult{x_new, P_new};
}

// Approximate steady-state CRLB for p_t over a fixed calibration update interval:
//
// We simulate K steps of the joint Kalman filter with periodic calibration updates
// encoded in Q/R, and take the limiting variance element P_{11} as the CRLB bound
// for p_t.
double crlb_for_psych_state(const JointKalmanParams& params,
                            const JointState& x0,
                            const Covariance2x2& P0,
                            int steps,
                            double y_obs) {
    JointState state = x0;
    Covariance2x2 cov = P0;
    for (int k = 0; k < steps; ++k) {
        auto res = joint_kalman_step(params, state, cov, y_obs);
        state = res.state;
        cov   = res.cov;
    }
    // CRLB ≈ Var_hat(p_t) >= P_{11}
    return cov.p11;
}

// ----------------------------------------------------------
// Demonstration main
// ----------------------------------------------------------

int main() {
    std::cout << std::fixed << std::setprecision(4);

    // 11. Bandit allocation demo.
    BanditContext ctx{
        {
            {"EDGE_NORTH", 0.05, 0.010, 1},
            {"EDGE_SOUTH", 0.07, 0.020, 1},
            {"EDGE_COOL",  0.04, 0.005, 1}
        },
        0.015, // RoH_max_delta per unit load
        0.0
    };

    std::cout << "AI workload bandit allocation (heat-island recovery):\n";
    for (int t = 1; t <= 10; ++t) {
        int arm_idx = select_arm_with_constraint(ctx, static_cast<double>(t));
        if (arm_idx < 0) {
            std::cout << "  t=" << t << ": no safe arm respecting RoH constraint.\n";
            continue;
        }
        BanditArm& arm = ctx.arms[arm_idx];

        // Simulate observed reward as noisy reduction of HII.
        double reward = arm.HII_reduction_estimate + 0.01 * std::sin(0.3 * t);
        update_arm(arm, reward);
        ctx.total_load += 1.0;

        std::cout << "  t=" << t << ": selected " << arm.name
                  << ", reward=" << reward
                  << ", updated mean=" << arm.HII_reduction_estimate << "\n";
    }
    std::cout << "  Total load allocated: " << ctx.total_load << "\n\n";

    // 12. Joint Kalman-psych coupling demo.
    JointKalmanParams params{
        0.95,  // a_pp
        0.05,  // a_pc (calibration influences psych-state slightly)
        0.02,  // a_cp (psych-state influences calibration drift)
        0.90,  // a_cc
        1.0,   // h_p (measurement sees psych-state)
        0.2,   // h_c (measurement sees some calibration component)
        0.001, // q_pp
        0.0001,// q_pc
        0.0001,// q_cp
        0.001, // q_cc
        0.01   // r_yy (measurement noise)
    };

    JointState x0{0.5, 0.8};
    Covariance2x2 P0{0.05, 0.0, 0.0, 0.05};
    double y_obs = 0.6; // illustrative measurement
    int steps = 100;

    double crlb_p = crlb_for_psych_state(params, x0, P0, steps, y_obs);

    std::cout << "Joint Kalman-psych coupling:\n";
    std::cout << "  Approximate CRLB for psych-state Var_hat(p_t) >= " << crlb_p << "\n";

    return 0;
}

} // namespace eco
} // namespace praxis
