// File: cpp/eco_restoration/hex_cooling_time_constant.cpp

#include <vector>
#include <string>
#include <cmath>
#include <iostream>

/**
 * 39. Hex-specific exponential cooling time constant τ_h.
 *
 * From multi-temporal Landsat (and optionally night-time sensors), we can
 * approximate the post-sunset cooling of each hex h as:
 *
 *   T_surf,h(t) ≈ T_eq,h + (T_surf,h(t_0) - T_eq,h) ⋅ exp(-(t - t_0) / τ_h)
 *
 * where:
 *   - T_surf,h(t) is surface temperature at time t.
 *   - T_eq,h is asymptotic equilibrium temperature (near night-time baseline).
 *   - τ_h is the hex-specific cooling time constant.
 *
 * Rearranging for discrete observations at times t_1, t_2:
 *
 *   ln( T_surf,h(t_1) - T_eq,h ) - ln( T_surf,h(t_2) - T_eq,h )
 *     = (t_2 - t_1) / τ_h
 *
 * so:
 *
 *   τ_h = (t_2 - t_1) /
 *         [ ln( T_surf,h(t_1) - T_eq,h ) - ln( T_surf,h(t_2) - T_eq,h ) ]
 *
 * With more time steps, we can fit τ_h via linear regression in log-space.
 *
 * We then relate τ_h to material composition through α, β, γ:
 *   - High impervious fraction (large B_h, positive β) → larger thermal inertia → larger τ_h.
 *   - High vegetation (large V_h, negative α) → faster cooling → smaller τ_h.
 *   - Water (W_h, negative γ) also accelerates local cooling.
 *
 * A simple linear link:
 *
 *   τ_h ≈ τ_0
 *       + k_α ⋅ |α| ⋅ (1 - V_h)
 *       + k_β ⋅ β    ⋅ B_h
 *       + k_γ ⋅ |γ| ⋅ (1 - W_h)
 *
 * where τ_0, k_α, k_β, k_γ are fitted from data; higher τ_h indicates
 * greater thermal inertia for that hex.[168][52][65][165]
 */

struct CoolingObservation {
    double time_hours;   // time since reference (e.g., hours after sunset)
    double T_surf;       // surface temperature
};

double fit_tau_for_hex(const std::vector<CoolingObservation>& obs, double T_eq) {
    // Fit τ via linear regression on ln(T_surf - T_eq) vs time.
    std::vector<double> xs;
    std::vector<double> ys;

    for (const auto& o : obs) {
        double diff = o.T_surf - T_eq;
        if (diff <= 0.0) continue;
        xs.push_back(o.time_hours);
        ys.push_back(std::log(diff));
    }

    int n = static_cast<int>(xs.size());
    if (n < 2) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    double sum_x = 0.0, sum_y = 0.0, sum_xx = 0.0, sum_xy = 0.0;
    for (int i = 0; i < n; ++i) {
        sum_x += xs[i];
        sum_y += ys[i];
        sum_xx += xs[i] * xs[i];
        sum_xy += xs[i] * ys[i];
    }

    double mean_x = sum_x / n;
    double mean_y = sum_y / n;
    double denom = sum_xx - n * mean_x * mean_x;
    if (std::fabs(denom) < 1e-12) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    double slope = (sum_xy - n * mean_x * mean_y) / denom;
    // ln(T - T_eq) ≈ ln(T0 - T_eq) - t / τ_h ⇒ slope = -1 / τ_h
    double tau = (slope != 0.0) ? -1.0 / slope : std::numeric_limits<double>::quiet_NaN();
    return tau;
}

struct HexCoolingParams {
    std::string hex_id;
    double alpha;
    double beta;
    double gamma;
    double V;
    double B;
    double W;
};

double estimate_tau_from_composition(const HexCoolingParams& h,
                                     double tau0,
                                     double k_alpha,
                                     double k_beta,
                                     double k_gamma) {
    double tau =
        tau0
        + k_alpha * std::fabs(h.alpha) * (1.0 - h.V)
        + k_beta  * h.beta            * h.B
        + k_gamma * std::fabs(h.gamma) * (1.0 - h.W);
    return tau;
}

int main_tau() {
    // Synthetic cooling observations for a hex.
    std::vector<CoolingObservation> obs = {
        {0.0, 45.0},   // at sunset
        {2.0, 40.0},
        {4.0, 37.0},
        {6.0, 35.5}
    };
    double T_eq = 34.0;
    double tau_fit = fit_tau_for_hex(obs, T_eq);

    HexCoolingParams h{"hex_10_20", -8.0, 3.0, -5.0, 0.25, 0.5, 0.05};
    double tau_est = estimate_tau_from_composition(h, /*tau0=*/2.0,
                                                   /*k_alpha=*/1.0,
                                                   /*k_beta=*/0.5,
                                                   /*k_gamma=*/0.8);

    std::cout << "Fitted τ_h from cooling curve: " << tau_fit << " hours\n";
    std::cout << "Estimated τ_h from α,β,γ,V,B,W: " << tau_est << " hours\n";

    return 0;
}
