// File: cpp/eco_restoration/scale_sensitivity_and_lagged_offset.cpp

#include <vector>
#include <string>
#include <cmath>
#include <iostream>

/**
 * 27. Spectral Index Sensitivity to Spatial Resolution
 *
 * Question: How do α, β, γ change when we move from 30 m pixel-scale to
 * 500 m hexagons, and how can we express a scaling law relating coefficient
 * magnitudes to aggregation scale?
 *
 * Conceptual scaling law:
 *
 * Let s denote spatial scale (e.g., characteristic hex diameter in meters).
 * At finer scales, variance of spectral indices and LST is higher; at coarser
 * scales, aggregation smooths extremes and reduces variance.[52][65][171][165]
 *
 * Suppose:
 *   Var(V_s) ∝ s^{-p_V}, Var(B_s) ∝ s^{-p_B}, Var(W_s) ∝ s^{-p_W}
 *
 * For a linear regression:
 *   ΔT_h(s) = α_s V_h(s) + β_s B_h(s) + γ_s W_h(s) + δ_s
 *
 * If the underlying physical sensitivities are scale-invariant, then
 * α_s, β_s, γ_s should adjust to preserve ΔT variance:
 *
 *   α_s ≈ α_ref ⋅ (σ_V_ref / σ_V_s)
 *   β_s ≈ β_ref ⋅ (σ_B_ref / σ_B_s)
 *   γ_s ≈ γ_ref ⋅ (σ_W_ref / σ_W_s)
 *
 * where σ_V_ref is the standard deviation at reference scale (e.g., 30 m),
 * and σ_V_s at new scale s. This yields:
 *
 *   α_s = α_ref ⋅ (s / s_ref)^{p_V / 2}
 *   similarly for β_s, γ_s.
 *
 * We can test this by calibrating the model at multiple hex resolutions
 * and comparing empirical α_s, β_s, γ_s to the predicted scaling.
 */

struct ScaleCalibration {
    double scale_m;  // hex resolution (e.g., 30, 250, 500 m)
    double alpha;
    double beta;
    double gamma;
    double sigma_V;
    double sigma_B;
    double sigma_W;
};

struct ScaleLawPrediction {
    double scale_m;
    double alpha_pred;
    double beta_pred;
    double gamma_pred;
};

ScaleLawPrediction predict_coeffs_at_scale(
        const ScaleCalibration& ref,
        double target_scale_m,
        double p_V,
        double p_B,
        double p_W) {
    double s_ratio = target_scale_m / ref.scale_m;

    double alpha_pred = ref.alpha * std::pow(s_ratio, p_V / 2.0);
    double beta_pred  = ref.beta  * std::pow(s_ratio, p_B / 2.0);
    double gamma_pred = ref.gamma * std::pow(s_ratio, p_W / 2.0);

    return {target_scale_m, alpha_pred, beta_pred, gamma_pred};
}

int main_scale() {
    // Synthetic reference calibration at 30 m.
    ScaleCalibration ref{30.0, -8.0, 3.0, -5.0, 0.12, 0.08, 0.05};

    // Target calibration at 500 m.
    double target_scale = 500.0;
    double p_V = 1.0; // approximate variance scaling exponent
    double p_B = 1.0;
    double p_W = 1.0;

    ScaleLawPrediction pred = predict_coeffs_at_scale(ref, target_scale, p_V, p_B, p_W);

    std::cout << "Predicted coefficients at 500 m:\n"
              << "  alpha_500 ≈ " << pred.alpha_pred << "\n"
              << "  beta_500  ≈ " << pred.beta_pred << "\n"
              << "  gamma_500 ≈ " << pred.gamma_pred << "\n";

    return 0;
}

/**
 * 28. Heat-Island Offset with Multi-Temporal Memory
 *
 * Lagged offset model:
 *
 *   ΔT_h^(t) = α V_h^(t) + β B_h^(t) + γ W_h^(t) + ρ ΔT_h^(t-1) + ε_h^(t)
 *
 * Possible extensions:
 *   ΔT_h^(t) = α V_h^(t) + β B_h^(t) + γ W_h^(t) + ρ_1 ΔT_h^(t-1)
 *              + ρ_2 ΔT_h^(t-2) + ...
 *
 * Conditions for improved predictive power:
 *   - Significant autocorrelation in ΔT_h over Landsat overpass intervals
 *     (weeks), indicating thermal inertia of surfaces and infrastructure.
 *   - Slowly evolving land cover (V_h, B_h, W_h) relative to thermal response,
 *     so past ΔT contains useful prognostic information beyond current indices.[168][52][65]
 *
 * Interpretation of ρ:
 *   - |ρ| close to 0: little thermal memory; current ΔT is driven mostly
 *     by instantaneous land cover and meteorology.
 *   - 0 < ρ < 1: positive thermal inertia; prior warmth contributes to
 *     current UHI (e.g., heat stored in building materials).
 *   - ρ near 1: strong persistence; UHI decays slowly, indicating highly
 *     inert surfaces and weak night-time cooling.
 *
 * We implement a simple AR(1)-augmented regression fit.
 */

struct HexLagSample {
    double V_t;
    double B_t;
    double W_t;
    double delta_T_t_minus1;
    double delta_T_t;
};

struct LaggedRegression {
    double alpha;
    double beta;
    double gamma;
    double rho;
    double delta;
};

LaggedRegression fit_lagged_offset(const std::vector<HexLagSample>& data) {
    // Design matrix X: [V_t, B_t, W_t, ΔT_{t-1}, 1], response y = ΔT_t.
    int n = static_cast<int>(data.size());
    if (n < 5) {
        throw std::invalid_argument("Need at least 5 samples for lagged regression.");
    }

    const int k = 5; // parameters: α, β, γ, ρ, δ
    double XtX[k][k] = {{0.0}};
    double XtY[k] = {0.0};

    for (const auto& s : data) {
        double x[k] = {s.V_t, s.B_t, s.W_t, s.delta_T_t_minus1, 1.0};
        for (int i = 0; i < k; ++i) {
            XtY[i] += x[i] * s.delta_T_t;
            for (int j = 0; j < k; ++j) {
                XtX[i][j] += x[i] * x[j];
            }
        }
    }

    // Gaussian elimination on XtX * beta = XtY.
    double A[k][k + 1];
    for (int i = 0; i < k; ++i) {
        for (int j = 0; j < k; ++j) {
            A[i][j] = XtX[i][j];
        }
        A[i][k] = XtY[i];
    }

    for (int pivot = 0; pivot < k; ++pivot) {
        int max_row = pivot;
        double max_val = std::fabs(A[pivot][pivot]);
        for (int i = pivot + 1; i < k; ++i) {
            double val = std::fabs(A[i][pivot]);
            if (val > max_val) {
                max_val = val;
                max_row = i;
            }
        }
        if (max_val < 1e-12) {
            throw std::runtime_error("Singular XtX in lagged regression.");
        }
        if (max_row != pivot) {
            for (int j = 0; j <= k; ++j) {
                std::swap(A[pivot][j], A[max_row][j]);
            }
        }

        double diag = A[pivot][pivot];
        for (int j = pivot; j <= k; ++j) {
            A[pivot][j] /= diag;
        }
        for (int i = pivot + 1; i < k; ++i) {
            double factor = A[i][pivot];
            for (int j = pivot; j <= k; ++j) {
                A[i][j] -= factor * A[pivot][j];
            }
        }
    }

    double beta[k];
    for (int i = k - 1; i >= 0; --i) {
        double sum = A[i][k];
        for (int j = i + 1; j < k; ++j) {
            sum -= A[i][j] * beta[j];
        }
        beta[i] = sum;
    }

    LaggedRegression res;
    res.alpha = beta[0];
    res.beta  = beta[1];
    res.gamma = beta[2];
    res.rho   = beta[3];
    res.delta = beta[4];
    return res;
}

int main_lag() {
    // Synthetic lagged samples with thermal inertia.
    std::vector<HexLagSample> data;
    for (int t = 1; t <= 50; ++t) {
        double V_t = 0.3 + 0.001 * t;
        double B_t = 0.4 + 0.0005 * t;
        double W_t = 0.05 + 0.0008 * t;
        double delta_T_prev = -7.5 * V_t + 3.0 * B_t - 4.8 * W_t + 0.5; // approximate
        double delta_T_t = -7.5 * V_t + 3.0 * B_t - 4.8 * W_t + 0.5
                           + 0.6 * delta_T_prev; // ρ ≈ 0.6
        data.push_back({V_t, B_t, W_t, delta_T_prev, delta_T_t});
    }

    LaggedRegression reg = fit_lagged_offset(data);

    std::cout << "Lagged offset regression:\n"
              << "  alpha=" << reg.alpha
              << "  beta=" << reg.beta
              << "  gamma=" << reg.gamma
              << "  rho=" << reg.rho
              << "  delta=" << reg.delta << "\n";

    return 0;
}
